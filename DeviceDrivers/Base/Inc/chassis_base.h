//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_CHASSIS_BASE_H
#define LAYERED_EMBEDDED_C_CHASSIS_BASE_H

typedef struct chassis_base chassis_base_t;

typedef struct chassis_ops {
  void (*update)(chassis_base_t *this);
  void (*stop)  (chassis_base_t *this);
  void (*estop) (chassis_base_t *this);
} chassis_ops_t;

typedef struct chassis_base {
  const chassis_ops_t *ops;
  const char          *name;
} chassis_base_t;

void Chassis_Base_Init(chassis_base_t *this, const char *name, const chassis_ops_t *ops);
void Chassis_Base_Update(chassis_base_t *this);
void Chassis_Base_Stop(chassis_base_t *this);
void Chassis_Base_EStop(chassis_base_t *this);

#endif //LAYERED_EMBEDDED_C_CHASSIS_BASE_H
