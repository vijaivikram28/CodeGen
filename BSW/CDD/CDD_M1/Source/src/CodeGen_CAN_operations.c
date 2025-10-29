/**
 * @file CodeGen_CAN_operations.c
 * @brief CAN Communication Implementation for BMS on STM32H7
 */

#include "CodeGen_CAN_operations.h"
#include "stm32h7xx_hal.h"

/* Global CAN Configuration Instance */
BMS_CANConfigTypeDef bms_can_config = {0};

/**
 * @brief Initialize CAN Communication Peripheral
 * @param pCanConfig Pointer to CAN configuration structure
 * @return HAL Status of initialization
 */
HAL_StatusTypeDef BMS_CAN_Init(BMS_CANConfigTypeDef* pCanConfig) {
    HAL_StatusTypeDef status = HAL_ERROR;

    /* Validate input parameter */
    if (pCanConfig == NULL) {
        return HAL_ERROR;
    }

    /* Enable FDCAN1 Clock */
    __HAL_RCC_FDCAN_CLK_ENABLE();

    /* Set FDCAN1 Clock Source (Adjust as per system configuration) */
    __HAL_RCC_FDCAN_CONFIG(RCC_FDCANCLKSOURCE_PLL);

    /* Configure FDCAN1 Peripheral */
    pCanConfig->hfdcan.Instance = FDCAN1;
    pCanConfig->hfdcan.Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
    pCanConfig->hfdcan.Init.Mode = FDCAN_MODE_NORMAL;
    pCanConfig->hfdcan.Init.AutoRetransmission = ENABLE;
    pCanConfig->hfdcan.Init.TransmitPause = DISABLE;
    pCanConfig->hfdcan.Init.ProtocolException = DISABLE;

    /* Bit Timing Configuration */
    pCanConfig->hfdcan.Init.NominalPrescaler = 8;
    pCanConfig->hfdcan.Init.NominalSyncJumpWidth = 1;
    pCanConfig->hfdcan.Init.NominalTimeSeg1 = 6;
    pCanConfig->hfdcan.Init.NominalTimeSeg2 = 1;

    /* Data timing */
    pCanConfig->hfdcan.Init.DataPrescaler = 4;
    pCanConfig->hfdcan.Init.DataSyncJumpWidth = 1;
    pCanConfig->hfdcan.Init.DataTimeSeg1 = 6;
    pCanConfig->hfdcan.Init.DataTimeSeg2 = 1;

    /* Buffer */
    pCanConfig->hfdcan.Init.MessageRAMOffset = 0;
    pCanConfig->hfdcan.Init.StdFiltersNbr = 1;
    pCanConfig->hfdcan.Init.ExtFiltersNbr = 1;
    pCanConfig->hfdcan.Init.RxFifo0ElmtsNbr = 16;
    pCanConfig->hfdcan.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_16;
    pCanConfig->hfdcan.Init.RxFifo1ElmtsNbr = 16;
    pCanConfig->hfdcan.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_16;
    pCanConfig->hfdcan.Init.RxBuffersNbr = 16;
    pCanConfig->hfdcan.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
    pCanConfig->hfdcan.Init.TxEventsNbr = 8;
    pCanConfig->hfdcan.Init.TxBuffersNbr = 8;
    pCanConfig->hfdcan.Init.TxFifoQueueElmtsNbr = 8;
    pCanConfig->hfdcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    pCanConfig->hfdcan.Init.TxElmtSize = FDCAN_DATA_BYTES_8;

    /* Initialize FDCAN Peripheral */
    status = HAL_FDCAN_Init(&pCanConfig->hfdcan);
    if (status != HAL_OK) {
        return status;
    }

    /* Configure CAN Filters */
    status = BMS_CAN_ConfigureFilters(pCanConfig);
    if (status != HAL_OK) {
        return status;
    }

    /* Enable FDCAN Peripheral */
    status = HAL_FDCAN_Start(&pCanConfig->hfdcan);

    return status;
}

/**
 * @brief Configure CAN Message Filters
 * @param pCanConfig Pointer to CAN configuration structure
 * @return HAL Status of filter configuration
 */
HAL_StatusTypeDef BMS_CAN_ConfigureFilters(BMS_CANConfigTypeDef* pCanConfig) {
    if (pCanConfig == NULL) {
        return HAL_ERROR;
    }

    /* Configure Standard Filter */
    pCanConfig->sFilterConfig.IdType = FDCAN_EXTENDED_ID;
    pCanConfig->sFilterConfig.FilterIndex = BMS_CAN_FILTER_BANK;
    pCanConfig->sFilterConfig.FilterType = FDCAN_FILTER_MASK;
    pCanConfig->sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    pCanConfig->sFilterConfig.FilterID1 = 0x00000000;
    pCanConfig->sFilterConfig.FilterID2 = 0x00000000;

    return HAL_FDCAN_ConfigFilter(&pCanConfig->hfdcan, &pCanConfig->sFilterConfig);
}

/**
 * @brief Transmit CAN Message
 * @param pCanConfig Pointer to CAN configuration structure
 * @param id Message Identifier
 * @param data Pointer to data buffer
 * @param length Length of data
 * @return HAL Status of transmission
 */
HAL_StatusTypeDef BMS_CAN_Transmit(
    BMS_CANConfigTypeDef* pCanConfig, 
    uint32_t id, 
    uint8_t* data,
    uint32_t length
) {
    FDCAN_TxHeaderTypeDef txHeader = {0};

//    if ((pCanConfig == NULL) || (data == NULL)) {
//        return HAL_ERROR;
//    }

    /* Configure Tx Header */
    txHeader.Identifier = id;
    txHeader.IdType = FDCAN_EXTENDED_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.FDFormat = FDCAN_FD_CAN;
    txHeader.DataLength = FDCAN_DLC_BYTES_8;  // HAL expects length shifted
    return HAL_FDCAN_AddMessageToTxFifoQ(
        &pCanConfig->hfdcan, 
        &txHeader, 
        data
    );


}

/**
 * @brief Receive CAN Message
 * @param pCanConfig Pointer to CAN configuration structure
 * @param id Pointer to store message identifier
 * @param data Pointer to data buffer
 * @param length Pointer to store data length
 * @return HAL Status of reception
 */
HAL_StatusTypeDef BMS_CAN_Receive(
    BMS_CANConfigTypeDef* pCanConfig, 
    uint32_t* id, 
    uint8_t* data, 
    uint32_t* length
) {
    FDCAN_RxHeaderTypeDef rxHeader = {0};

    if ((pCanConfig == NULL) || (id == NULL) || (data == NULL) || (length == NULL)) {
        return HAL_ERROR;
    }

    /* Retrieve Message */
    HAL_StatusTypeDef status = HAL_FDCAN_GetRxMessage(
        &pCanConfig->hfdcan, 
        FDCAN_RX_FIFO0, 
        &rxHeader, 
        data
    );

    if (status == HAL_OK) {
        *id = rxHeader.Identifier;
        *length = rxHeader.DataLength >> 16;  // Convert back from HAL format
    }

    return status;
}

/**
 * @brief CAN Interrupt Handler
 * @param pCanConfig Pointer to CAN configuration structure
 */
void BMS_CAN_IRQHandler(BMS_CANConfigTypeDef* pCanConfig) {
    if (pCanConfig != NULL) {
        HAL_FDCAN_IRQHandler(&pCanConfig->hfdcan);
    }
}

/**
 * @brief Handle CAN Communication Timeouts
 * @param pCanConfig Pointer to CAN configuration structure
 * @param timeout_ms Timeout duration in milliseconds
 * @return HAL Status of timeout handling
 */
HAL_StatusTypeDef BMS_CAN_HandleTimeout(
    BMS_CANConfigTypeDef* pCanConfig, 
    uint32_t timeout_ms
) {
    /* Implement timeout mechanism with HAL_GetTick() comparison */
    uint32_t tickstart = HAL_GetTick();
    
    while ((HAL_GetTick() - tickstart) < timeout_ms) {
        /* Optional: Add specific timeout recovery logic */
    }

    return HAL_TIMEOUT;
}
