/**
 * @file bms_can_gpio_config.c
 * @brief GPIO configuration for CAN communication in Battery Management System
 */

#include "CodeGen_BMS_config.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_def.h"
#include "CodeGen_GPIO.h"

/**
 * @brief Initialize GPIO pins for CAN communication
 * @return HAL status indicating configuration success or failure
 */
HAL_StatusTypeDef BMS_CAN_GPIO_Init(void) {
    /* Enable GPIO peripheral clocks */
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* GPIO Configuration for FDCAN1 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;  // FDCAN1 TX/RX pins
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;         // Alternate function push-pull
    GPIO_InitStruct.Pull = GPIO_NOPULL;             // No pull-up/down resistors
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;   // High-speed GPIO
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;    // FDCAN1 alternate function

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    return HAL_OK;
}

/**
 * @brief Deinitialize CAN GPIO pins
 * @return HAL status indicating deinitialization success or failure
 */
HAL_StatusTypeDef BMS_CAN_GPIO_DeInit(void) {
    /* Reset GPIO pins to default state */
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_0 | GPIO_PIN_1);

    return HAL_OK;
}

/**
 * @brief Configure CAN GPIO pin interrupt settings
 * @return HAL status indicating interrupt configuration success or failure
 */
HAL_StatusTypeDef BMS_CAN_GPIO_InterruptConfig(void) {
    /* Configure EXTI interrupt for CAN pins if needed */
    HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    return HAL_OK;
}


