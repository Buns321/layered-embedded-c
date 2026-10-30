/**
 * @file flash_contract_test.c
 * @brief Flash Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_flash_backend.h"
#include "platform_flash.h"
#include "system_critical.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TEST_FLASH_START 0x1000U
#define TEST_FLASH_END   0x1FFFU
#define TEST_ERASE_UNIT  256U

static uint8_t s_flash[TEST_FLASH_END - TEST_FLASH_START + 1U];
static platform_status_t s_write_status;
static platform_status_t s_erase_status;
static uint32_t s_backend_error;

/**
 * @brief Flash 契约测试使用的空临界区进入桩。
 * @pre 必须与 Sys_Critical_Exit 配对调用；本测试不在 ISR 中调用。
 * @post 函数体为空，不保存 PRIMASK、不关闭中断、不增加嵌套计数。
 * @warning 该桩不提供互斥或中断保护，只用于让被测 Flash 公共层完成链接。
 * @note 本测试后续不依赖真实临界区；生产 System 实现才有中断/调度保护语义。
 */
void Sys_Critical_Enter(void) {
}

/**
 * @brief Flash 契约测试使用的空临界区退出桩。
 * @pre 之前已调用本文件的 Sys_Critical_Enter 桩；本函数不验证配对关系。
 * @post 函数体为空，不恢复 PRIMASK、不开启中断、不修改嵌套计数。
 * @warning 该桩不提供互斥或中断保护，只用于让被测 Flash 公共层完成链接。
 * @note 必须与 Sys_Critical_Enter 成对调用；生产 System 实现才有恢复语义。
 */
void Sys_Critical_Exit(void) {
}

/**
 * @brief 校验字节范围是否完整落在测试 RAM Flash 窗口内。
 * @param address 起始字节地址；测试窗口为 TEST_FLASH_START..TEST_FLASH_END，不检查对齐。
 * @param size 待访问字节数；必须大于零。
 * @retval true address 位于窗口内且 address + size - 1 不超过 TEST_FLASH_END。
 * @retval false size 为零、address 低于窗口起点，或范围跨越窗口末尾。
 * @post 只进行整数范围比较，不读取或修改 s_flash，也不访问真实 Flash。
 * @note 本函数是测试替身；生产后端按当前芯片的内部 Flash 地址空间判断。
 */
bool platform_flash_backend_range_valid(uint32_t address, uint32_t size) {
  if (
    size == 0U ||
    address < TEST_FLASH_START ||
    address > TEST_FLASH_END
  ) {
    return false;
  }
  return size <= TEST_FLASH_END - address + 1U;
}

/**
 * @brief 返回测试 RAM Flash 窗口的固定擦除单元大小。
 * @param address 待查询的字节地址；允许位于擦除单元内部。
 * @return address 位于 TEST_FLASH_START..TEST_FLASH_END 时返回 TEST_ERASE_UNIT（256 字节），
 *         否则返回 0。
 * @post 只调用 range_valid 并返回常量，不擦除或修改 s_flash。
 * @note 本函数是测试替身；生产后端按 F1 页或 F4 扇区返回真实粒度。
 */
uint32_t platform_flash_backend_erase_unit_size(uint32_t address) {
  return platform_flash_backend_range_valid(address, 1U)
           ? TEST_ERASE_UNIT
           : 0U;
}

/**
 * @brief 从测试 RAM Flash 复制一段字节到调用方缓冲区。
 * @param address 源字节地址；公共 Flash 层已保证范围落在测试窗口内。
 * @param data 目标缓冲区；由调用方拥有，至少可写 size 字节，调用期间不得被其他执行流修改。
 * @param size 要复制的字节数；必须大于零。
 * @retval PLATFORM_STATUS_OK 测试替身已从 s_flash 复制 size 字节到 data。
 * @pre 公共 Flash 层已完成实例、范围、大小和缓冲区校验；测试替身自身不复查这些条件。
 * @post data 前 size 字节等于测试 RAM Flash 对应内容；不修改 s_flash 或 s_backend_error。
 * @warning 非法地址或空指针会越界访问测试数组并导致进程崩溃；生产后端的合法只读地址约束仍适用。
 * @note 本函数是测试替身，使用 RAM 数组和 memcpy，不执行真实存储器读取。
 */
platform_status_t platform_flash_backend_read(
  uint32_t address,
  void *data,
  uint32_t size
) {
  memcpy(data, &s_flash[address - TEST_FLASH_START], size);
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 向测试 RAM Flash 复制写入数据，或返回注入的写失败状态。
 * @param address 目标起始字节地址；公共 Flash 层已保证 4 字节对齐且位于测试窗口内。
 * @param data 源缓冲区；由调用方拥有，至少可读 size 字节，调用期间不得被其他执行流修改。
 * @param size 要写入的字节数；必须为 4 的非零整数倍。
 * @retval PLATFORM_STATUS_OK s_write_status 为 OK；测试替身已把 size 字节复制到 s_flash，
 *         并把 s_backend_error 清零。
 * @retval PLATFORM_STATUS_ERROR s_write_status 为其他值；测试替身把 s_backend_error 置为
 *         PLATFORM_FLASH_ERROR_PROGRAM 并原样返回该注入状态，不写入数组。
 * @pre 公共 Flash 层已完成范围、4 字节对齐、缓冲区和忙状态校验；测试需先设置 s_write_status。
 * @post 成功时 s_flash 对应字节等于 data，s_backend_error 为 NONE；失败时 s_flash 不变，
 *       s_backend_error 为 PROGRAM。
 * @warning 测试替身不执行擦除态检查、不逐字编程，也不访问 HAL；非法地址或空指针会越界访问数组。
 * @note 本函数是测试替身，仅模拟成功或 PROGRAM 失败两条路径。
 */
platform_status_t platform_flash_backend_write(
  uint32_t address,
  const void *data,
  uint32_t size
) {
  if (s_write_status != PLATFORM_STATUS_OK) {
    s_backend_error = PLATFORM_FLASH_ERROR_PROGRAM;
    return s_write_status;
  }
  memcpy(&s_flash[address - TEST_FLASH_START], data, size);
  s_backend_error = PLATFORM_FLASH_ERROR_NONE;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 用 0xFF 覆盖测试 RAM Flash 的一段字节，或返回注入的擦除失败状态。
 * @param address 起始字节地址；公共 Flash 层已保证按擦除单元边界对齐并位于测试窗口内。
 * @param size 要覆盖的字节数；公共 Flash 层保证大于零，测试替身自身不复查。
 * @retval PLATFORM_STATUS_OK s_erase_status 为 OK；测试替身已把 s_flash 的 size 字节填为 0xFF，
 *         并把 s_backend_error 清零。
 * @retval PLATFORM_STATUS_ERROR s_erase_status 为其他值；测试替身把 s_backend_error 置为
 *         PLATFORM_FLASH_ERROR_ERASE 并原样返回该注入状态，不修改数组。
 * @pre 公共 Flash 层已完成范围和对齐校验；测试需先设置 s_erase_status。
 * @post 成功时测试窗口内指定字节为 0xFF，错误快照为 NONE；失败时测试窗口不变，
 *       错误快照为 ERASE。
 * @warning 测试替身不按擦除单元扩展范围、不执行 HAL 擦除，也不模拟部分擦除；非法地址会越界访问数组。
 * @note 本函数是测试替身，仅模拟成功或 ERASE 失败两条路径。
 */
platform_status_t platform_flash_backend_erase(
  uint32_t address,
  uint32_t size
) {
  if (s_erase_status != PLATFORM_STATUS_OK) {
    s_backend_error = PLATFORM_FLASH_ERROR_ERASE;
    return s_erase_status;
  }
  memset(&s_flash[address - TEST_FLASH_START], 0xFF, size);
  s_backend_error = PLATFORM_FLASH_ERROR_NONE;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回本测试替身保存的 Flash 错误快照。
 * @return PLATFORM_FLASH_ERROR_* 位掩码；PLATFORM_FLASH_ERROR_NONE 表示尚未记录错误，
 *         或最近一次后端写/擦除成功。
 * @post 只读取 s_backend_error，不清除该值，也不访问真实 Flash。
 * @note 本函数是测试替身；测试注入的写/擦除失败会分别留下 PROGRAM/ERASE 位。
 */
uint32_t platform_flash_backend_get_error(void) {
  return s_backend_error;
}

/**
 * @brief 验证 Flash 读写、擦除、对齐和错误快照。
 * @post 成功返回时，写入后可读回相同字；非对齐写和擦除返回
 *       INVALID_ARGUMENT 并记录 ALIGNMENT；合法擦除清除通用错误；合法地址返回
 *       256 字节擦除单元，窗口外返回 0；注入 ERROR 后写失败同时记录 PROGRAM
 *       和 HAL 错误位；非法实例查询返回 INVALID_ARGUMENT 错误位。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试 Flash 是 RAM 数组，临界区为空桩，不执行真实擦写或中断屏蔽。
 */
static void test_flash_contract(void) {
  s_write_status = PLATFORM_STATUS_OK;
  s_erase_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_FLASH_ERROR_NONE;

  const uint32_t words[2] = {0x11223344U, 0x55667788U};
  assert(Platform_FLASH_Write(
           PLATFORM_FLASH_1,
           TEST_FLASH_START,
           words,
           sizeof(words)
         ) == PLATFORM_STATUS_OK);

  uint32_t readback[2] = {0};
  assert(Platform_FLASH_Read(
           PLATFORM_FLASH_1,
           TEST_FLASH_START,
           readback,
           sizeof(readback)
         ) == PLATFORM_STATUS_OK);
  assert(readback[0] == words[0]);
  assert(readback[1] == words[1]);

  assert(Platform_FLASH_Write(
           PLATFORM_FLASH_1,
           TEST_FLASH_START + 1U,
           words,
           sizeof(words)
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_FLASH_GetLastError(PLATFORM_FLASH_1) &
          PLATFORM_FLASH_ERROR_ALIGNMENT) != 0U);

  assert(Platform_FLASH_Erase(
           PLATFORM_FLASH_1,
           TEST_FLASH_START + 1U,
           TEST_ERASE_UNIT
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_FLASH_GetLastError(PLATFORM_FLASH_1) &
          PLATFORM_FLASH_ERROR_ALIGNMENT) != 0U);

  assert(Platform_FLASH_Erase(
           PLATFORM_FLASH_1,
           TEST_FLASH_START,
           TEST_ERASE_UNIT
         ) == PLATFORM_STATUS_OK);
  assert(Platform_FLASH_GetLastError(PLATFORM_FLASH_1) ==
         PLATFORM_ERROR_NONE);
  assert(Platform_FLASH_GetEraseUnitSize(
           PLATFORM_FLASH_1,
           TEST_FLASH_START
         ) == TEST_ERASE_UNIT);
  assert(Platform_FLASH_GetEraseUnitSize(
           PLATFORM_FLASH_1,
           TEST_FLASH_END + 1U
         ) == 0U);

  s_write_status = PLATFORM_STATUS_ERROR;
  s_backend_error = PLATFORM_FLASH_ERROR_PROGRAM;
  assert(Platform_FLASH_Write(
           PLATFORM_FLASH_1,
           TEST_FLASH_START,
           words,
           sizeof(words)
         ) == PLATFORM_STATUS_ERROR);
  assert((Platform_FLASH_GetLastError(PLATFORM_FLASH_1) &
          PLATFORM_FLASH_ERROR_PROGRAM) != 0U);
  assert((Platform_FLASH_GetLastError(PLATFORM_FLASH_1) &
          PLATFORM_ERROR_HAL) != 0U);

  assert((Platform_FLASH_GetLastError((platform_flash_instance_t)99U) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

/**
 * @brief 运行 Flash 契约测试并返回进程状态。
 * @return 固定返回 0；只有所有内层断言通过才会到达该返回语句。
 * @post 测试全局状态保留最后一次写失败与错误注入结果。
 * @warning 任一 assert 失败都会终止进程，因此不会返回非零退出码。
 * @note 仅供 CTest 启动本测试可执行文件时调用。
 */
int main(void) {
  test_flash_contract();
  return 0;
}
