//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file controller_compose.c
 * @brief 控制器级联组合实现。
 * @ingroup controller
 */

#include "controller_compose.h"
#include "system_math.h"
#include <assert.h>

/**
 * @brief 用相同输入执行全部子控制律并求输出和。
 * @param controller_h 已初始化的 compose 控制律基对象，不能为 NULL。
 * @param target 参考量；单位由子控制律决定，轮速环为 m/s，必须为有限值。
 * @param measure 实测量；单位必须与 target 一致，必须为有限值。
 * @param dt 时间步，单位 s；必须有限且大于 0。
 * @param out 调用方拥有的输出存储，不能为 NULL；写入子控制律输出之和。
 * @retval CONTROLLER_STATUS_OK 所有子控制律成功且输出和有限。
 * @retval CONTROLLER_STATUS_SATURATED 至少一个子控制律报告饱和，但输出和仍有效。
 * @retval CONTROLLER_STATUS_ERROR 子控制律失败或输出和不为有限值；out 被清零。
 * @pre stage_count 在 1..CONTROLLER_COMPOSE_CAPACITY 内，且每个表项已初始化。
 * @post 无子控制律失败时，全部子控制律按调用顺序更新一次且 out 为有限输出和；遇到 ERROR 时立即停止，out 清零，后续子控制律不更新。
 * @note 子控制律按顺序执行，非线程安全；不分配动态内存。
 */
static controller_status_t ctrl_compose_update(
  controller_base_t *controller_h,
  float target,
  float measure,
  float dt,
  float *out
) {
  assert(controller_h && out);

  controller_compose_t *controller_compose_h = (controller_compose_t *)controller_h;
  assert(controller_compose_h->stage_count > 0u);
  assert(controller_compose_h->stage_count <= CONTROLLER_COMPOSE_CAPACITY);

  float sum = 0.0f;
  controller_status_t status = CONTROLLER_STATUS_OK;

  for (uint8_t i = 0; i < controller_compose_h->stage_count; i++) {
    assert(controller_compose_h->controller_stage_h[i]);

    float stage_out = 0.0f;
    controller_status_t stage_status = Ctrl_Base_Update(
      controller_compose_h->controller_stage_h[i],
      target,
      measure,
      dt,
      &stage_out
    );

    if (stage_status == CONTROLLER_STATUS_ERROR) {
      *out = 0.0f;
      return CONTROLLER_STATUS_ERROR;
    }

    if (stage_status == CONTROLLER_STATUS_SATURATED) {
      status = CONTROLLER_STATUS_SATURATED;
    }

    sum += stage_out;
  }

  if (!SYS_ISFINITE(sum)) {
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  *out = sum;
  return status;
}

/**
 * @brief 重置并联求和控制器中的全部子控制律。
 * @param controller_h 已初始化的 compose 控制律基对象，不能为 NULL。
 * @pre stage_count 与子控制律表有效，且当前没有并发更新。
 * @post 每个子控制律均执行一次 reset；compose 自身配置和表项不变。
 * @note 按表顺序同步调用，非线程安全；不分配动态内存。
 */
static void ctrl_compose_reset(controller_base_t *controller_h) {
  assert(controller_h);

  controller_compose_t *controller_compose_h = (controller_compose_t *)controller_h;
  assert(controller_compose_h->stage_count > 0u);
  assert(controller_compose_h->stage_count <= CONTROLLER_COMPOSE_CAPACITY);

  for (uint8_t i = 0; i < controller_compose_h->stage_count; i++) {
    assert(controller_compose_h->controller_stage_h[i]);
    Ctrl_Base_Reset(controller_compose_h->controller_stage_h[i]);
  }
}

static const controller_ops_t controller_ops_compose = {
  .update = ctrl_compose_update,
  .reset  = ctrl_compose_reset,
};

/**
 * @brief 初始化并联求和控制器并复制子控制律表。
 * @param controller_h 待初始化的控制器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖控制器对象。
 * @param controller_stage_h 非空子控制律指针数组；前 stage_count 项必须非空、已初始化且互不重复。
 * @param stage_count 子控制律数量，范围 1..CONTROLLER_COMPOSE_CAPACITY。
 * @pre 子控制律均不能指向 controller_h->base；所有指针表项在对象使用期间保持有效。
 * @post 子控制律表复制入对象、剩余槽位清零、数量已设置，并对全部子控制器执行 reset。
 * @note Debug 构建用 assert 检查自引用、重复项和空回调；仅允许初始化阶段单线程调用。
 */
void Ctrl_Compose_Init(
  controller_compose_t      *controller_h,
  const char                *name,
  controller_base_t *const  *controller_stage_h,
  uint8_t                    stage_count
) {
  assert(
    controller_h                                 &&
    name                                 &&
    controller_stage_h                               &&
    stage_count > 0u                     &&
    stage_count <= CONTROLLER_COMPOSE_CAPACITY
  );

  for (uint8_t i = 0; i < stage_count; i++) {
    assert(
      controller_stage_h[i]                       &&
      controller_stage_h[i] != &controller_h->base        &&
      controller_stage_h[i]->ops                  &&
      controller_stage_h[i]->ops->update          &&
      controller_stage_h[i]->ops->reset
    );

    for (uint8_t j = 0; j < i; j++) {
      assert(controller_stage_h[i] != controller_stage_h[j]);
    }
  }

  Ctrl_Base_Init(&controller_h->base, name, &controller_ops_compose);

  for (uint8_t i = 0; i < CONTROLLER_COMPOSE_CAPACITY; i++) {
    controller_h->controller_stage_h[i] = NULL;
  }

  for (uint8_t i = 0; i < stage_count; i++) {
    controller_h->controller_stage_h[i] = controller_stage_h[i];
  }

  controller_h->stage_count = stage_count;

  Ctrl_Base_Reset(&controller_h->base);
}
