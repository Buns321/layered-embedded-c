/**
 * @file app_portability_fixture.c
 * @brief App 源码可移植性和依赖注入最小 fixture。
 * @ingroup tests
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct app_portability_dependencies_s {
  uint32_t *value;
} app_portability_dependencies_t;

static const app_portability_dependencies_t *s_dependencies;

/**
 * @brief 初始化最小 App fixture。
 * @param dependencies 非空依赖集合。
 * @retval true 初始化成功。
 * @retval false 依赖为空。
 * @post 成功时保存依赖指针。
 * @note 只用于编译和主机执行验证。
 */
static bool app_portability_init(
  const app_portability_dependencies_t *dependencies
) {
  if (dependencies == NULL || dependencies->value == NULL) {
    return false;
  }

  s_dependencies = dependencies;
  return true;
}

/**
 * @brief 推进一次最小 App 业务步骤。
 * @param now_ms 当前单调毫秒时间。
 * @post 依赖值更新为传入时间。
 * @note 不访问硬件。
 */
static void app_portability_process(uint32_t now_ms) {
  assert(s_dependencies);
  *s_dependencies->value = now_ms;
}

/**
 * @brief 验证依赖注入和业务处理。
 * @return 0 表示通过。
 * @post fixture 完成一次初始化、处理和值检查。
 * @note 无外部资源。
 */
int main(void) {
  static uint32_t value = 0u;
  static app_portability_dependencies_t dependencies = {
    .value = &value,
  };

  assert(app_portability_init(&dependencies));
  app_portability_process(123u);
  assert(value == 123u);
  assert(!app_portability_init(NULL));
  return 0;
}
