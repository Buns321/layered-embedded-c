//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file chassis_omni.c
 * @brief Omni 底盘实现。
 * @ingroup chassis
 */

#include "chassis_omni.h"
#include "system_math.h"
#include <assert.h>

/**
 * @brief 清零平动/自转命令，并对四轮依次下发零目标与制动请求。
 * @param chassis_h 底盘的 chassis_base 视图；非 NULL，且必须来自已初始化的 chassis_omni_t 首位成员。
 * @pre chassis_h 已由 Chassis_Omni_Init 初始化，四个电机指针非 NULL，且没有其他上下文并发改写底盘或电机。
 * @pre 只能在控制/安全任务上下文调用；本函数会写 v/omega 并进入 Dev_Motor_Base_Brake。
 * @post v 和 omega 被置为 0，dir 保持不变；每个电机的目标速度先被置 0，再调用 Dev_Motor_Base_Brake。
 * @post 电机处于 DISABLE 时，基类 Brake 只清除目标、当前速度和控制状态，不会输出硬件制动；处于 ENABLE/BRAKE 时会复位控制律并执行具体驱动的制动。
 * @note 本函数不清除 dir、IMU/策略指针或 imu_timeout_ms；不分配内存，也不直接访问 GPIO/PWM。
 * @warning 无锁且非 ISR 安全；逐个电机操作期间可被更高优先级上下文观察，必须由调用方保证底盘生命周期和控制独占。
 */
static void chassis_omni_stop(chassis_base_t *chassis_h);

/**
 * @brief 读取 IMU 快照，按 yaw 新鲜度解算并下发四轮目标速度。
 * @param chassis_h 底盘的 chassis_base 视图；非 NULL，且必须来自已初始化的 chassis_omni_t 首位成员。
 * @param now_ms 当前单调毫秒时间戳，单位 ms；用于计算 yaw 通道年龄，应来自同一时基并在每次控制周期递增。
 * @pre chassis_h 已初始化，imu_h/policy_h 与四个电机指针有效，且调用方按固定控制周期串行调用。
 * @pre 同一底盘对象不得由 ISR 或其他任务并发 Update/Stop/EStop/SetMotion。
 * @post IMU 快照不可用、yaw 无效或 yaw_age_ms 大于 imu_timeout_ms 时，调用底盘停机逻辑并直接返回，不运行策略解算。
 * @post 数据有效时把 yaw_deg 转为 yaw_rad，调用 Omni 策略得到四轮目标线速度（m/s），逐个写入电机目标；不使能电机，也不执行电机闭环 Update。
 * @note yaw 年龄使用无符号差值；正常单调时间回绕可处理，但两次调用间隔达到 2^32 ms 时会失去新鲜度语义。
 * @note 缺失或过期 IMU 会触发停车和制动请求，属于有意的失效安全副作用；停机函数本身不一定让 DISABLE 电机产生电气制动。
 * @warning 本函数没有锁；在控制任务之外调用会让底盘命令、电机目标与闭环更新发生竞态。
 */
static void chassis_omni_update(chassis_base_t *chassis_h, uint32_t now_ms) {
  assert(chassis_h);

  chassis_omni_t *chassis_omni_h = (chassis_omni_t *)chassis_h;
  assert(chassis_omni_h->imu_h && chassis_omni_h->policy_h);

  imu_sample_t imu_sample = {0};
  bool imu_available = Dev_IMU_Base_GetSample(
    chassis_omni_h->imu_h,
    &imu_sample
  );

  uint32_t yaw_age_ms =
    now_ms -
    imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW];

  if (
    !imu_available                                          ||
    (imu_sample.valid_mask & IMU_CHANNEL_YAW) == 0u         ||
    yaw_age_ms > chassis_omni_h->imu_timeout_ms
  ) {
    chassis_omni_stop(chassis_h);
    return;
  }

  const float deg_to_rad = 3.14159265358979323846f / 180.0f;
  float yaw_rad = imu_sample.yaw_deg * deg_to_rad;
  float wheel_speed[CHASSIS_OMNI_WHEEL_COUNT] = {0};

  Policy_Omni_Base_Solve(
    chassis_omni_h->policy_h,
    chassis_omni_h->v,
    chassis_omni_h->dir,
    yaw_rad,
    chassis_omni_h->omega,
    wheel_speed
  );

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(chassis_omni_h->motor_h[i]);
    Dev_Motor_Base_SetTargetSpeed(chassis_omni_h->motor_h[i], wheel_speed[i]);
  }
}

/**
 * @brief 清零平动/自转命令，并对四轮依次下发零目标与制动请求。
 * @param chassis_h 底盘的 chassis_base 视图；非 NULL，且必须来自已初始化的 chassis_omni_t 首位成员。
 * @pre chassis_h 已由 Chassis_Omni_Init 初始化，四个电机指针非 NULL，且没有其他上下文并发改写底盘或电机。
 * @pre 只能在控制/安全任务上下文调用；本函数会写 v/omega 并进入 Dev_Motor_Base_Brake。
 * @post v 和 omega 被置为 0，dir 保持不变；每个电机的目标速度先被置 0，再调用 Dev_Motor_Base_Brake。
 * @post 电机处于 DISABLE 时，基类 Brake 只清除目标、当前速度和控制状态，不会输出硬件制动；处于 ENABLE/BRAKE 时会复位控制律并执行具体驱动的制动。
 * @note 本函数不清除 dir、IMU/策略指针或 imu_timeout_ms；不分配内存，也不直接访问 GPIO/PWM。
 * @warning 无锁且非 ISR 安全；逐个电机操作期间可被更高优先级上下文观察，必须由调用方保证底盘生命周期和控制独占。
 */
static void chassis_omni_stop(chassis_base_t *chassis_h) {
  assert(chassis_h);

  chassis_omni_t *chassis_omni_h = (chassis_omni_t *)chassis_h;

  chassis_omni_h->v     = 0.0f;
  chassis_omni_h->omega = 0.0f;

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(chassis_omni_h->motor_h[i]);
    Dev_Motor_Base_SetTargetSpeed(chassis_omni_h->motor_h[i], 0.0f);
    Dev_Motor_Base_Brake(chassis_omni_h->motor_h[i]);
  }
}

/**
 * @brief 清零底盘命令并立即禁用四个轮电机输出。
 * @param chassis_h 底盘的 chassis_base 视图；非 NULL，且必须来自已初始化的 chassis_omni_t 首位成员。
 * @pre chassis_h 已由 Chassis_Omni_Init 初始化，四个电机指针非 NULL，且无其他上下文并发控制这些电机。
 * @pre 仅供 Chassis_Base_EStop 通过 chassis_ops_omni 在控制/安全上下文调用。
 * @post v 和 omega 被置为 0，dir 保持不变；每个电机经 Dev_Motor_Base_Disable 写零功率、拉低 STBY，并清除目标/当前速度及控制器状态。
 * @post 本函数不执行短路制动；被禁用电机需要显式 Dev_Motor_Base_Enable 才能恢复输出。
 * @note 禁用会作用于具体驱动共享的 STBY 组，可能同时封锁未直接传入本底盘的电机；不修改 IMU 超时或策略配置。
 * @warning 无锁且非 ISR 安全；急停无法原子覆盖四轮，且共享 STBY 的最终电平取决于所有组内写者的调用顺序。
 */
static void chassis_omni_estop(chassis_base_t *chassis_h) {
  assert(chassis_h);

  chassis_omni_t *chassis_omni_h = (chassis_omni_t *)chassis_h;

  chassis_omni_h->v     = 0.0f;
  chassis_omni_h->omega = 0.0f;

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(chassis_omni_h->motor_h[i]);
    Dev_Motor_Base_Disable(chassis_omni_h->motor_h[i]);
  }
}

static const chassis_ops_t chassis_ops_omni = {
  .update = chassis_omni_update,
  .stop   = chassis_omni_stop,
  .estop  = chassis_omni_estop,
};

/**
 * @brief 初始化 Omni 底盘并绑定电机、IMU、策略和生命周期操作表。
 * @param chassis_h 待初始化对象；非 NULL，生命周期和独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 chassis_base.name 保存指针而不复制。
 * @param motor_h 四轮电机基类指针数组；四个元素均非 NULL、彼此不同，数组本身不会被保存。
 * @param imu_h 已初始化的 IMU 基类指针；不拥有所有权，必须提供有效 yaw 通道，生命周期覆盖底盘。
 * @param policy_h 已初始化的 Omni 策略基类指针；不拥有所有权，轮序必须与 motor_h 一致。
 * @param imu_timeout_ms yaw 通道最大允许年龄，单位 ms；必须大于 0，超过该年龄的 yaw 会触发底盘停机。
 * @pre chassis_h、name、motor_h 中四个元素、imu_h、policy_h 非 NULL；仅调试构建会断言空指针和重复电机指针。
 * @pre 电机、IMU 和策略均已完成各自初始化，且没有控制任务并发读取该底盘对象。
 * @post chassis_base 已绑定 update/stop/estop 操作表，四个电机指针和 IMU/策略指针已保存，v/dir/omega 均为 0，imu_timeout_ms 已保存。
 * @note 不分配内存，不复制 name 或任何被指向对象；初始化本身不读取 IMU、不启动电机、不改变 GPIO/PWM 输出。
 * @warning 非 ISR 安全；只允许在设备装配阶段、控制任务启动前调用。发布构建会信任调用方满足非空和去重前置条件。
 */
void Chassis_Omni_Init(
  chassis_omni_t     *chassis_h,
  const char         *name,
  motor_base_t *const motor_h[CHASSIS_OMNI_WHEEL_COUNT],
  imu_base_t         *imu_h,
  policy_omni_base_t *policy_h,
  uint32_t            imu_timeout_ms
) {
  assert(
    chassis_h            &&
    name            &&
    motor_h           &&
    imu_h             &&
    policy_h          &&
    imu_timeout_ms > 0u
  );

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(motor_h[i]);

    for (uint8_t j = 0; j < i; j++) {
      assert(motor_h[i] != motor_h[j]);
    }
  }

  Chassis_Base_Init(&chassis_h->base, name, &chassis_ops_omni);

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    chassis_h->motor_h[i] = motor_h[i];
  }

  chassis_h->imu_h    = imu_h;
  chassis_h->policy_h = policy_h;
  chassis_h->v      = 0.0f;
  chassis_h->dir    = 0.0f;
  chassis_h->omega  = 0.0f;
  chassis_h->imu_timeout_ms = imu_timeout_ms;
}

/**
 * @brief 保存 Omni 底盘的世界系平动与自转命令。
 * @param chassis_h 已初始化的 Omni 底盘对象；非 NULL。
 * @param v 世界系平动速度，单位 m/s；必须有限，本函数不施加速度幅值上限。
 * @param dir 世界系行进方向，单位 rad，逆时针为正；必须有限，不要求归一化到固定周期。
 * @param omega 自转角速度，单位 rad/s，逆时针为正；必须有限，本函数不施加角速度幅值上限。
 * @pre chassis_h 已由 Chassis_Omni_Init 初始化，且与 Update/Stop/EStop 在同一控制上下文串行执行。
 * @post 三个命令字段被逐个写入；非有限值在调试构建触发断言并中止，在定义 NDEBUG 的构建中被逐项替换为 0。
 * @note 只保存命令，不读取 IMU、不解算轮速、不改变电机目标或硬件输出；实际解算在 Chassis_Base_Update 中完成。
 * @note 不对 v/dir/omega 做归一化或限幅，超出底盘能力的命令由策略、控制律和电机 max_duty 后续约束。
 * @warning 三字段非原子更新；从 ISR 或其他任务并发调用会让 Update 观察到混合命令，必须由调用方互斥。
 */
void Chassis_Omni_SetMotion(chassis_omni_t *chassis_h, float v, float dir, float omega) {
  assert(chassis_h);

  assert(
    SYS_ISFINITE(v)     &&
    SYS_ISFINITE(dir)   &&
    SYS_ISFINITE(omega)
  );

  if (!SYS_ISFINITE(v))     v     = 0.0f;
  if (!SYS_ISFINITE(dir))   dir   = 0.0f;
  if (!SYS_ISFINITE(omega)) omega = 0.0f;

  chassis_h->v     = v;
  chassis_h->dir   = dir;
  chassis_h->omega = omega;
}
