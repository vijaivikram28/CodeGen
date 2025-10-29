#ifndef BMS_CAN_CONFIG_H
#define BMS_CAN_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
//#include "CodeGen_BMS_config.h"

/* CAN Message Buffer Configuration */
#define BMS_CAN_TX_BUFFERS                  (3U)
#define BMS_CAN_RX_BUFFERS                  (5U)
#define BMS_CAN_TOTAL_BUFFERS               (BMS_CAN_TX_BUFFERS + BMS_CAN_RX_BUFFERS)

/* CAN Message Identifiers */
#define BMS_CAN_BASE_ID                     (0x18FFFF00UL)
#define BMS_CAN_BROADCAST_ID                (0x18FFFF00UL)
#define BMS_CAN_RESPONSE_ID                 (0x18FFFF01UL)

/* Typedef for CAN Configuration Structure */
typedef struct {
    FDCAN_HandleTypeDef hfdcan;
    FDCAN_FilterTypeDef sFilterConfig;
    uint32_t tx_buffer_index;
    uint32_t rx_buffer_index;
} BMS_CANConfigTypeDef;

/* CAN Initialization Prototypes */
HAL_StatusTypeDef BMS_CAN_Init(BMS_CANConfigTypeDef* pCanConfig);
HAL_StatusTypeDef BMS_CAN_ConfigureFilters(BMS_CANConfigTypeDef* pCanConfig);
HAL_StatusTypeDef BMS_CAN_ConfigureBitTiming(BMS_CANConfigTypeDef* pCanConfig);

/* CAN Communication Prototypes */
HAL_StatusTypeDef BMS_CAN_Transmit(
    BMS_CANConfigTypeDef* pCanConfig, 
    uint32_t id, 
    uint8_t* data, 
    uint32_t length
);

HAL_StatusTypeDef BMS_CAN_Receive(
    BMS_CANConfigTypeDef* pCanConfig, 
    uint32_t* id, 
    uint8_t* data, 
    uint32_t* length
);

/* Interrupt Handlers */
void BMS_CAN_IRQHandler(BMS_CANConfigTypeDef* pCanConfig);
void BMS_CAN_ErrorHandler(BMS_CANConfigTypeDef* pCanConfig);

/* Utility Functions */
HAL_StatusTypeDef BMS_CAN_HandleTimeout(
    BMS_CANConfigTypeDef* pCanConfig, 
    uint32_t timeout_ms
);

/* Extern Global Configuration */
extern BMS_CANConfigTypeDef bms_can_config;

#ifdef __cplusplus
}
#endif

#endif /* BMS_CAN_CONFIG_H */
