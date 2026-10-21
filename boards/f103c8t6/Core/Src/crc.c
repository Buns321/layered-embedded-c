/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    crc.c
  * @brief   This file provides code for the CRC configuration.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "crc.h"

CRC_HandleTypeDef hcrc;

void MX_CRC_Init(void)
{
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_CRC_MspInit(CRC_HandleTypeDef* crcHandle)
{
  if (crcHandle->Instance == CRC)
  {
    __HAL_RCC_CRC_CLK_ENABLE();
  }
}
