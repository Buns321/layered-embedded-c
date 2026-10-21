/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    crc.h
  * @brief   This file contains the prototypes for the CRC configuration.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __CRC_H__
#define __CRC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern CRC_HandleTypeDef hcrc;

void MX_CRC_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __CRC_H__ */
