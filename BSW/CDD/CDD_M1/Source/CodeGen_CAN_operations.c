/* ===== File: BSW/CDD/CDD_M1/Source/src/CodeGen_CAN_operations.c ===== */
/**
 * @file CodeGen_CAN_operations.c
 * @brief CAN Communication Implementation for BMS on STM32H7 (refactored for runtime performance)
 */

#include "CodeGen_CAN_operations.h"
#include "stm32h7xx_hal.h"
#include <stddef.h>

/* Global CAN Configuration Instance */
BMS_CANConfigTypeDef bms_can_config = {0};

/* Static template for transmit header to reduce repeated initialization overhead */
static const FDCAN_TxHeaderTypeDef s_tx_header_template = {
    .Identifier = 0U,
    .IdType = FDCAN_EXTENDED_ID,
    .TxFrameType = FDCAN_DATA_FRAME,
    .DataLength = FDCAN_DLC_BYTES_8,
    .FDFormat = FDCAN_FD_CAN,
    .BitRateSwitch = FDCAN_BRS_OFF,
    .MessageMarker = 0U
};

/* Small helper to map byte length to FDCAN DLC code for common short frames */
static inline uint32_t LengthToDlc(uint32_t len)
{
    /* Only common lengths 0..8 are mapped here; others use 8 by default to preserve behavior */
    static const uint32_t dlc_map[9] = {
        FDCAN_DLC_BYTES_0,
        FDCAN_DLC_BYTES_1,
        FDCAN_DLC_BYTES_2,
        FDCAN_DLC_BYTES_3,
        FDCAN_DLC_BYTES_4,
        FDCAN_DLC_BYTES_5,
        FDCAN_DLC_BYTES_6,
        FDCAN_DLC_BYTES_7,
        FDCAN_DLC_BYTES_8
    };

    return (len <= 8U) ? dlc_map[len] : FDCAN_DLC_BYTES_8;
}

/**
 * @brief Initialize CAN Communication Peripheral
 * @param pCanConfig Pointer to CAN configuration structure
 * @return HAL Status of initialization
 */
HAL_StatusTypeDef BMS_CAN_Init(BMS_CANConfigTypeDef* pCanConfig)
{
    HAL_StatusTypeDef status = HAL_ERROR;

    if (pCanConfig == NULL)
    {
        return HAL_ERROR;
    }

    /* Enable FDCAN1 Clock and configure source only once */
    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_FDCAN_CONFIG(RCC_FDCANCLKSOURCE_PLL);

    /* Local alias for performance */
    FDCAN_HandleTypeDef *hfdcan = &pCanConfig->hfdcan;

    /* Configure FDCAN1 Peripheral minimal assignments (only changing values) */
    hfdcan->Instance = FDCAN1;
    hfdcan->Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
    hfdcan->Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan->Init.AutoRetransmission = ENABLE;
    hfdcan->Init.TransmitPause = DISABLE;
    hfdcan->Init.ProtocolException = DISABLE;

    /* Bit Timing Configuration */
    hfdcan->Init.NominalPrescaler = 8U;
    hfdcan->Init.NominalSyncJumpWidth = 1U;
    hfdcan->Init.NominalTimeSeg1 = 6U;
    hfdcan->Init.NominalTimeSeg2 = 1U;

    /* Data timing */
    hfdcan->Init.DataPrescaler = 4U;
    hfdcan->Init.DataSyncJumpWidth = 1U;
    hfdcan->Init.DataTimeSeg1 = 6U;
    hfdcan->Init.DataTimeSeg2 = 1U;

    /* Buffer */
    hfdcan->Init.MessageRAMOffset = 0U;
    hfdcan->Init.StdFiltersNbr = 1U;
    hfdcan->Init.ExtFiltersNbr = 1U;
    hfdcan->Init.RxFifo0ElmtsNbr = 16U;
    hfdcan->Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_16;
    hfdcan->Init.RxFifo1ElmtsNbr = 16U;
    hfdcan->Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_16;
    hfdcan->Init.RxBuffersNbr = 16U;
    hfdcan->Init.RxBufferSize = FDCAN_DATA_BYTES_8;
    hfdcan->Init.TxEventsNbr = 8U;
    hfdcan->Init.TxBuffersNbr = 8U;
    hfdcan->Init.TxFifoQueueElmtsNbr = 8U;
    hfdcan->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    hfdcan->Init.TxElmtSize = FDCAN_DATA_BYTES_8;

    status = HAL_FDCAN_Init(hfdcan);
    if (status != HAL_OK)
    {
        return status;
    }

    /* Configure CAN Filters */
    status = BMS_CAN_ConfigureFilters(pCanConfig);
    if (status != HAL_OK)
    {
        return status;
    }

    /* Start FDCAN peripheral */
    status = HAL_FDCAN_Start(hfdcan);
    return status;
}

/**
 * @brief Configure CAN Message Filters
 * @param pCanConfig Pointer to CAN configuration structure
 * @return HAL Status of filter configuration
 */
HAL_StatusTypeDef BMS_CAN_ConfigureFilters(BMS_CANConfigTypeDef* pCanConfig)
{
    if (pCanConfig == NULL)
    {
        return HAL_ERROR;
    }

    /* Assign filter configuration in a single block to reduce repeated branching and store-to-memory ops */
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
 * @param length Length of data
 * @return HAL Status of transmission
 */
HAL_StatusTypeDef BMS_CAN_Transmit(
    BMS_CANConfigTypeDef* pCanConfig,
    uint32_t id,
    uint8_t* data,
    uint32_t length
)
{
    if ((pCanConfig == NULL) || (data == NULL))
    {
        return HAL_ERROR;
    }

    /* Limit length to 8 to match configured Tx element size and preserve deterministic behavior */
    if (length > 8U)
    {
        length = 8U;
    }

    /* Use local copy of the template to minimize field writes */
    FDCAN_TxHeaderTypeDef txHeader = s_tx_header_template;

    txHeader.Identifier = id;
    txHeader.DataLength = LengthToDlc(length);

    /* Local handle alias for faster access */
    FDCAN_HandleTypeDef *hfdcan = &pCanConfig->hfdcan;

    return HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &txHeader, data);
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
)
{
    if ((pCanConfig == NULL) || (id == NULL) || (data == NULL) || (length == NULL))
    {
        return HAL_ERROR;
    }

    FDCAN_RxHeaderTypeDef rxHeader;

    /* Local handle alias */
    FDCAN_HandleTypeDef *hfdcan = &pCanConfig->hfdcan;

    HAL_StatusTypeDef status = HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxHeader, data);
    if (status == HAL_OK)
    {
        *id = rxHeader.Identifier;
        /* Preserve original semantics: convert from HAL format (shift) */
        *length = (uint32_t)(rxHeader.DataLength >> 16);
    }

    return status;
}

/**
 * @brief CAN Interrupt Handler
 * @param pCanConfig Pointer to CAN configuration structure
 */
void BMS_CAN_IRQHandler(BMS_CANConfigTypeDef* pCanConfig)
{
    if (pCanConfig != NULL)
    {
        /* Directly call HAL ISR handler; keep ISR minimal and deterministic */
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
)
{
    (void)pCanConfig; /* Unused parameter preserved for interface compatibility */

    uint32_t tickstart = HAL_GetTick();

    /* Use subtraction-based timeout check to handle tick wrap-around efficiently */
    while ((HAL_GetTick() - tickstart) < timeout_ms)
    {
        /* Keep loop body empty to minimize overhead; allow compiler to optimize */
        __asm volatile ("nop");
    }

    return HAL_TIMEOUT;
}
