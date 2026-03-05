/* ===== File: CodeGen_CAN_operations.c ===== */
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
HAL_StatusTypeDef BMS_CAN_Init(BMS_CANConfigTypeDef* pCanConfig)
{
    HAL_StatusTypeDef status = HAL_ERROR;

    if (pCanConfig == NULL) {
        return HAL_ERROR;
    }

    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_FDCAN_CONFIG(RCC_FDCANCLKSOURCE_PLL);

    pCanConfig->hfdcan.Instance = FDCAN1;
    pCanConfig->hfdcan.Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
    pCanConfig->hfdcan.Init.Mode = FDCAN_MODE_NORMAL;
    pCanConfig->hfdcan.Init.AutoRetransmission = ENABLE;
    pCanConfig->hfdcan.Init.TransmitPause = DISABLE;
    pCanConfig->hfdcan.Init.ProtocolException = DISABLE;

    pCanConfig->hfdcan.Init.NominalPrescaler = 8U;
    pCanConfig->hfdcan.Init.NominalSyncJumpWidth = 1U;
    pCanConfig->hfdcan.Init.NominalTimeSeg1 = 6U;
    pCanConfig->hfdcan.Init.NominalTimeSeg2 = 1U;

    pCanConfig->hfdcan.Init.DataPrescaler = 4U;
    pCanConfig->hfdcan.Init.DataSyncJumpWidth = 1U;
    pCanConfig->hfdcan.Init.DataTimeSeg1 = 6U;
    pCanConfig->hfdcan.Init.DataTimeSeg2 = 1U;

    pCanConfig->hfdcan.Init.MessageRAMOffset = 0U;
    pCanConfig->hfdcan.Init.StdFiltersNbr = 1U;
    pCanConfig->hfdcan.Init.ExtFiltersNbr = 1U;
    pCanConfig->hfdcan.Init.RxFifo0ElmtsNbr = 16U;
    pCanConfig->hfdcan.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_16;
    pCanConfig->hfdcan.Init.RxFifo1ElmtsNbr = 16U;
    pCanConfig->hfdcan.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_16;
    pCanConfig->hfdcan.Init.RxBuffersNbr = 16U;
    pCanConfig->hfdcan.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
    pCanConfig->hfdcan.Init.TxEventsNbr = 8U;
    pCanConfig->hfdcan.Init.TxBuffersNbr = 8U;
    pCanConfig->hfdcan.Init.TxFifoQueueElmtsNbr = 8U;
    pCanConfig->hfdcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    pCanConfig->hfdcan.Init.TxElmtSize = FDCAN_DATA_BYTES_8;

    status = HAL_FDCAN_Init(&pCanConfig->hfdcan);
    if (status != HAL_OK) {
        return status;
    }

    status = BMS_CAN_ConfigureFilters(pCanConfig);
    if (status != HAL_OK) {
        return status;
    }

    return HAL_FDCAN_Start(&pCanConfig->hfdcan);
}

/**
 * @brief Configure CAN Message Filters
 * @param pCanConfig Pointer to CAN configuration structure
 * @return HAL Status
 */
HAL_StatusTypeDef BMS_CAN_ConfigureFilters(BMS_CANConfigTypeDef* pCanConfig)
{
    if (pCanConfig == NULL) {
        return HAL_ERROR;
    }

    pCanConfig->sFilterConfig.IdType = FDCAN_EXTENDED_ID;
    pCanConfig->sFilterConfig.FilterIndex = BMS_CAN_FILTER_BANK;
    pCanConfig->sFilterConfig.FilterType = FDCAN_FILTER_MASK;
    pCanConfig->sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    pCanConfig->sFilterConfig.FilterID1 = 0U;
    pCanConfig->sFilterConfig.FilterID2 = 0U;

    return HAL_FDCAN_ConfigFilter(&pCanConfig->hfdcan, &pCanConfig->sFilterConfig);
}

/**
 * @brief Transmit CAN Message
 * @param pCanConfig Pointer to CAN configuration structure
 * @param id Message Identifier
 * @param data Pointer to data buffer
 * @param length Data length in bytes
 * @return HAL Status
 */
HAL_StatusTypeDef BMS_CAN_Transmit(
    BMS_CANConfigTypeDef* pCanConfig,
    uint32_t id,
    uint8_t* data,
    uint32_t length)
{
    FDCAN_TxHeaderTypeDef txHeader;

    if ((pCanConfig == NULL) || (data == NULL)) {
        return HAL_ERROR;
    }

    if (length != 8U) {
        return HAL_ERROR;
    }

    (void)memset(&txHeader, 0, sizeof(FDCAN_TxHeaderTypeDef));

    txHeader.Identifier = id;
    txHeader.IdType = FDCAN_EXTENDED_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.FDFormat = FDCAN_FD_CAN;
    txHeader.DataLength = FDCAN_DLC_BYTES_8;

    return HAL_FDCAN_AddMessageToTxFifoQ(&pCanConfig->hfdcan, &txHeader, data);
}

/**
 * @brief Receive CAN Message
 * @param pCanConfig Pointer to CAN configuration structure
 * @param id Pointer to store message identifier
 * @param data Pointer to store message data
 * @param length Pointer to store data length
 * @return HAL Status
 */
HAL_StatusTypeDef BMS_CAN_Receive(
    BMS_CANConfigTypeDef* pCanConfig,
    uint32_t* id,
    uint8_t* data,
    uint32_t* length)
{
    FDCAN_RxHeaderTypeDef rxHeader;
    HAL_StatusTypeDef status;

    if ((pCanConfig == NULL) || (id == NULL) || (data == NULL) || (length == NULL)) {
        return HAL_ERROR;
    }

    (void)memset(&rxHeader, 0, sizeof(FDCAN_RxHeaderTypeDef));

    status = HAL_FDCAN_GetRxMessage(&pCanConfig->hfdcan, FDCAN_RX_FIFO0, &rxHeader, data);
    if (status == HAL_OK) {
        *id = rxHeader.Identifier;
        *length = (rxHeader.DataLength >> 16);
    }

    return status;
}

/**
 * @brief CAN Interrupt Handler
 * @param pCanConfig Pointer to CAN configuration structure
 */
void BMS_CAN_IRQHandler(BMS_CANConfigTypeDef* pCanConfig)
{
    if (pCanConfig != NULL) {
        HAL_FDCAN_IRQHandler(&pCanConfig->hfdcan);
    }
}

/**
 * @brief Handle CAN Communication Timeouts
 * @param pCanConfig Pointer to CAN configuration structure
 * @param timeout_ms Timeout duration
 * @return HAL Status
 */
HAL_StatusTypeDef BMS_CAN_HandleTimeout(
    BMS_CANConfigTypeDef* pCanConfig,
    uint32_t timeout_ms)
{
    uint32_t tickstart = HAL_GetTick();
    (void)pCanConfig;

    while ((HAL_GetTick() - tickstart) < timeout_ms) {
    }

    return HAL_TIMEOUT;
}