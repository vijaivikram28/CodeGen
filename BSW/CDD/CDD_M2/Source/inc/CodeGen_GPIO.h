/**
 * @file bms_can_gpio_config.c
 * @brief GPIO configuration for CAN communication in Battery Management System
 */

#include "CodeGen_BMS_config.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_def.h"

/**
 * @brief Initialize GPIO pins for CAN communication
 * @return HAL status indicating configuration success or failure
 */
HAL_StatusTypeDef BMS_CAN_GPIO_Init(void);

/**
 * @brief Deinitialize CAN GPIO pins
 * @return HAL status indicating deinitialization success or failure
 */
HAL_StatusTypeDef BMS_CAN_GPIO_DeInit(void);

/**
 * @brief Configure CAN GPIO pin interrupt settings
 * @return HAL status indicating interrupt configuration success or failure
 */
HAL_StatusTypeDef BMS_CAN_GPIO_InterruptConfig(void);

