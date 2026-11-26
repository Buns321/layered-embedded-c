/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rng.h
  * @brief   This file contains the prototypes for the RNG configuration.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __RNG_H__
#define __RNG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern RNG_HandleTypeDef hrng;

void MX_RNG_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __RNG_H__ */
