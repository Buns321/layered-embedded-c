//
// Created by Buns_ on 2026/8/15.
//

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DEF_H
#define LAYERED_EMBEDDED_C_PLATFORM_DEF_H

typedef enum {
  Platform_OK       = 0x00U,
  Platform_ERROR    = 0x01U,
  Platform_BUSY     = 0x02U,
  Platform_TIMEOUT  = 0x03U
} Platform_StatusTypeDef;

#endif //LAYERED_EMBEDDED_C_PLATFORM_DEF_H
