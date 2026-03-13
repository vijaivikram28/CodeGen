
#include "BMS_AddressManager.h"
#include "BMS_AddressManager_cfg.h"

#include <string.h> /* for memset */
#include <stdlib.h> /* for rand */
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_fdcan.h"

/* External FDCAN handle expected to be provided by top level application */
extern FDCAN_HandleTypeDef hfdcan1;

/* Local macros and message definitions */

/* Extended Identifier base for assignment commands and responses */
#define BMS_CAN_EXT_ID_BASE_ASSIGN   (0x18FF0000UL)
#define BMS_CAN_EXT_ID_BASE_RESPONSE (0x18FF1000UL)

/* Application-level command identifiers (payload[0]) */
#define BMS_CMD_ASSIGN_TOKEN   ((uint8_t)0xA1)
#define BMS_CMD_RACK_RESPONSE  ((uint8_t)0xB1)
#define BMS_CMD_ASSIGN_CONFIRM ((uint8_t)0xC1)

/* Payload sizes */
#define BMS_CAN_PAYLOAD_BYTES  8U

/* Local types */
typedef enum
{
    BMS_ASSIGN_STATUS_FREE = 0,
    BMS_ASSIGN_STATUS_ASSIGNED,
    BMS_ASSIGN_STATUS_UNRESOLVED
} BMS_AssignStatus_t;

/* Local (static) data */
static BMS_AssignedEntry_t g_assigned_list[BMS_MAX_RACKS];
static BMS_AssignStatus_t g_assign_status[BMS_MAX_RACKS];
static uint8_t g_assigned_count = 0U;

/* Forward declarations of internal helpers (static) */
static void BMS_AddressManager_ClearState(void);
static HAL_StatusTypeDef BMS_AddressManager_CANInit(void);
static HAL_StatusTypeDef BMS_AddressManager_CANDeInit(void);
static HAL_StatusTypeDef BMS_AddressManager_SendAssignToken(uint8_t candidate_address);
static int32_t BMS_AddressManager_CollectResponses(uint32_t timeout_ms, uint8_t responses[], uint8_t max_responses);
static uint8_t BMS_AddressManager_FindAssignedIndexByRackId(uint8_t rack_id);
static void BMS_AddressManager_RecordAssignment(uint8_t address, uint8_t rack_id);

/* Implementations */

HAL_StatusTypeDef BMS_AddressManager_Init(void)
{
    HAL_StatusTypeDef status;

    /* Initialize GPIO pins required for CAN (BSW-provided) */
    status = BMS_CAN_GPIO_Init();
    if (status != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Initialize CAN peripheral and start */
    status = BMS_AddressManager_CANInit();
    if (status != HAL_OK)
    {
        /* Attempt to deinit GPIO if CAN init fails */
        (void)BMS_CAN_GPIO_DeInit();
        return HAL_ERROR;
    }

    /* Configure GPIO interrupts if the BSW provides such capability */
    (void)BMS_CAN_GPIO_InterruptConfig();

    /* Clear internal state */
    BMS_AddressManager_ClearState();

    return HAL_OK;
}

HAL_StatusTypeDef BMS_AddressManager_DeInit(void)
{
    HAL_StatusTypeDef status;

    /* Deinitialize CAN peripheral */
    status = BMS_AddressManager_CANDeInit();
    if (status != HAL_OK)
    {
        /* continue to try deinitializing GPIO */
        (void)BMS_CAN_GPIO_DeInit();
        return HAL_ERROR;
    }

    /* Deinitialize CAN GPIO pins */
    status = BMS_CAN_GPIO_DeInit();
    if (status != HAL_OK)
    {
        return HAL_ERROR;
    }

    BMS_AddressManager_ClearState();

    return HAL_OK;
}

HAL_StatusTypeDef BMS_AddressManager_AssignAddresses(uint8_t expected_racks)
{
    uint8_t candidate;
    uint8_t expected = expected_racks;
    HAL_StatusTypeDef status;
    uint8_t responses[BMS_MAX_RACKS];
    int32_t num_resp;
    uint8_t retry;
    uint8_t i;
    uint8_t unresolved_mask = 0U;

    if (expected == 0U)
    {
        return HAL_ERROR;
    }

    if (expected > BMS_MAX_RACKS)
    {
        expected = BMS_MAX_RACKS;
    }

    /* Reset previous state */
    BMS_AddressManager_ClearState();

    /* Assign addresses 1..BMS_MAX_RACKS (0x00 reserved as default address) */
    for (candidate = 1U; candidate <= BMS_MAX_RACKS; candidate++)
    {
        /* Stop if we already assigned expected number of racks */
        if (g_assigned_count >= expected)
        {
            break;
        }

        /* Try several attempts for this candidate address */
        for (retry = 0U; retry < BMS_MAX_RETRIES; retry++)
        {
            status = BMS_AddressManager_SendAssignToken(candidate);
            if (status != HAL_OK)
            {
                /* Transmit failed; retry after small delay */
                HAL_Delay(2U);
                continue;
            }

            /* Wait and collect responses within timeout window */
            memset(responses, 0, sizeof(responses));
            num_resp = BMS_AddressManager_CollectResponses(BMS_RESPONSE_TIMEOUT_MS, responses, (uint8_t)BMS_MAX_RACKS);

            if (num_resp < 0)
            {
                /* Error while reading responses: try again */
                HAL_Delay(2U);
                continue;
            }

            if (num_resp == 0)
            {
                /* No rack responded to this candidate address -> free, continue to next */
                break;
            }

            if (num_resp == 1)
            {
                /* Single rack responded: accept assignment */
                BMS_AddressManager_RecordAssignment(candidate, responses[0]);
                break; /* proceed to next candidate */
            }

            /* Collision detected (more than one response). Perform simple backoff and retry. */
            /* Use jittered delay to attempt to separate responses in subsequent attempts. */
            {
                uint32_t jitter = (HAL_GetTick() & 0xFFU) % 10U;
                HAL_Delay((uint32_t)(10U + jitter));
                /* On to next retry */
            }
        } /* retry loop */

        /* If after retries collisions persist and no assignment, mark unresolved and continue */
        if (g_assigned_count >= expected)
        {
            break;
        }
    } /* candidate loop */

    /* Final status always HAL_OK for completed procedure; caller queries assigned list and success rate */
    (void)expected; /* suppress unused variable if compiled with different macros */

    return HAL_OK;
}

uint8_t BMS_AddressManager_GetAssignedList(BMS_AssignedEntry_t * out_list, uint8_t max_len)
{
    uint8_t to_copy;
    uint8_t i;

    if ((out_list == NULL) || (max_len == 0U))
    {
        return 0U;
    }

    to_copy = g_assigned_count;
    if (to_copy > max_len)
    {
        to_copy = max_len;
    }

    for (i = 0U; i < to_copy; i++)
    {
        out_list[i] = g_assigned_list[i];
    }

    return to_copy;
}

uint8_t BMS_AddressManager_GetSuccessRatePercent(uint8_t expected_racks)
{
    uint32_t rate;
    uint8_t expected;

    expected = expected_racks;
    if (expected == 0U)
    {
        return 0U;
    }

    if (expected > BMS_MAX_RACKS)
    {
        expected = BMS_MAX_RACKS;
    }

    rate = ((uint32_t)g_assigned_count * 100U) / (uint32_t)expected;

    if (rate > 100U)
    {
        rate = 100U;
    }

    return (uint8_t)rate;
}

/* ===== Internal helper implementations ===== */

static void BMS_AddressManager_ClearState(void)
{
    uint32_t i;
    for (i = 0U; i < (uint32_t)BMS_MAX_RACKS; i++)
    {
        g_assigned_list[i].address = 0U;
        g_assigned_list[i].rack_id = 0U;
        g_assign_status[i] = BMS_ASSIGN_STATUS_FREE;
    }
    g_assigned_count = 0U;
}

static HAL_StatusTypeDef BMS_AddressManager_CANInit(void)
{
    FDCAN_HandleTypeDef * phfdcan;
    FDCAN_GlobalTypeDef * pInit;

    /* Validate external handle pointer */
    phfdcan = &hfdcan1;
    if (phfdcan == NULL)
    {
        return HAL_ERROR;
    }

    /* HAL-level init should already be performed by user application (clock, pin, peripheral).
       Here we ensure the CAN peripheral is started and a basic filter is configured. */

    /* Start FDCAN */
    if (HAL_FDCAN_Start(phfdcan) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Configure a simple filter to accept all Extended IDs (so we can inspect responses) */
    {
        FDCAN_FilterTypeDef filter;

        /* Accept all messages: ID Mask filter with 0 mask */
        filter.IdType = FDCAN_EXTENDED_ID;
        filter.FilterIndex = 0U;
        filter.FilterType = FDCAN_FILTER_MASK;
        filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
        filter.FilterID1 = 0U;
        filter.FilterID2 = 0U;

        if (HAL_FDCAN_ConfigFilter(phfdcan, &filter) != HAL_OK)
        {
            /* attempt to stop peripheral to leave system in known state */
            (void)HAL_FDCAN_Stop(phfdcan);
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

static HAL_StatusTypeDef BMS_AddressManager_CANDeInit(void)
{
    FDCAN_HandleTypeDef * phfdcan;

    phfdcan = &hfdcan1;
    if (phfdcan == NULL)
    {
        return HAL_ERROR;
    }

    if (HAL_FDCAN_Stop(phfdcan) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef BMS_AddressManager_SendAssignToken(uint8_t candidate_address)
{
    FDCAN_TxHeaderTypeDef txHeader;
    uint8_t txData[BMS_CAN_PAYLOAD_BYTES];
    FDCAN_HandleTypeDef * phfdcan;
    HAL_StatusTypeDef hal_ret;

    phfdcan = &hfdcan1;
    if (phfdcan == NULL)
    {
        return HAL_ERROR;
    }

    /* Prepare payload: [CMD][candidate_address][reserved...] */
    txData[0] = BMS_CMD_ASSIGN_TOKEN;
    txData[1] = candidate_address;
    /* Clear remainder bytes for deterministic behavior */
    {
        uint8_t idx;
        for (idx = 2U; idx < BMS_CAN_PAYLOAD_BYTES; idx++)
        {
            txData[idx] = 0U;
        }
    }

    /* Prepare TX header for extended ID space dedicated to assignment */
    txHeader.Identifier = (uint32_t)(BMS_CAN_EXT_ID_BASE_ASSIGN | (uint32_t)candidate_address);
    txHeader.IdType = FDCAN_EXTENDED_ID;
    txHeader.TxFrameType = FDCAN_DATA_FRAME;
    txHeader.DataLength = FDCAN_DLC_BYTES_8;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch = FDCAN_BRS_OFF;
    txHeader.FDFormat = FDCAN_CLASSIC_CAN;
    txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker = 0U;

    /* Send frame to Tx FIFO Q (blocking attempt) */
    hal_ret = HAL_FDCAN_AddMessageToTxFifoQ(phfdcan, &txHeader, txData);
    if (hal_ret != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Optionally wait until TX completed or timeout â use short timeout */
    {
        uint32_t start = HAL_GetTick();
        while (HAL_FDCAN_IsTxBufferMessagePending(phfdcan) != 0U)
        {
            if ((HAL_GetTick() - start) > 10U)
            {
                /* Assume transmit took too long */
                break;
            }
        }
    }

    return HAL_OK;
}

/* Collect responses that match our response ID base within timeout_ms.
   Returns number of responses collected (>=0) or -1 on error. */
static int32_t BMS_AddressManager_CollectResponses(uint32_t timeout_ms, uint8_t responses[], uint8_t max_responses)
{
    FDCAN_HandleTypeDef * phfdcan;
    FDCAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[BMS_CAN_PAYLOAD_BYTES];
    uint32_t start;
    uint8_t count = 0U;

    if (responses == NULL)
    {
        return -1;
    }

    phfdcan = &hfdcan1;
    if (phfdcan == NULL)
    {
        return -1;
    }

    start = HAL_GetTick();

    /* Polling until timeout; collect messages from RX FIFO0 */
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        /* Check if there is a message pending in RX FIFO0 */
        if (HAL_FDCAN_GetRxMessage(phfdcan, FDCAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK)
        {
            /* Validate message: extended ID in expected response range and payload command */
            if ((rxHeader.IdType == FDCAN_EXTENDED_ID) &&
                (((rxHeader.Identifier & 0xFFFFF000UL) == BMS_CAN_EXT_ID_BASE_RESPONSE) ||
                 ((rxHeader.Identifier & 0xFFFFF000UL) == BMS_CAN_EXT_ID_BASE_ASSIGN) ))
            {
                /* Expecting rack response command in payload[0] */
                if (rxData[0] == BMS_CMD_RACK_RESPONSE)
                {
                    /* Extract rack identifier from payload[1] */
                    if (count < max_responses)
                    {
                        responses[count] = rxData[1];
                        count++;
                    }
                }
                else
                {
                    /* Other messages may be ignored for assignment process */
                }
            }
            else
            {
                /* Not a matching response: ignore */
            }
        }
        else
        {
            /* No message available right now; small delay to avoid busy spin */
            HAL_Delay(1U);
        }
    }

    return (int32_t)count;
}

/* Search assigned list by rack id, return index or 0xFF if not found */
static uint8_t BMS_AddressManager_FindAssignedIndexByRackId(uint8_t rack_id)
{
    uint8_t i;
    for (i = 0U; i < g_assigned_count; i++)
    {
        if (g_assigned_list[i].rack_id == rack_id)
        {
            return i;
        }
    }
    return 0xFFU;
}

/* Record assignment if rack not already recorded. */
static void BMS_AddressManager_RecordAssignment(uint8_t address, uint8_t rack_id)
{
    uint8_t idx;

    /* Prevent duplicates based on rack_id */
    idx = BMS_AddressManager_FindAssignedIndexByRackId(rack_id);
    if (idx != 0xFFU)
    {
        /* Already recorded: update address if needed */
        g_assigned_list[idx].address = address;
        g_assign_status[idx] = BMS_ASSIGN_STATUS_ASSIGNED;
        return;
    }

    /* Add new entry at end if space */
    if (g_assigned_count < BMS_MAX_RACKS)
    {
        g_assigned_list[g_assigned_count].address = address;
        g_assigned_list[g_assigned_count].rack_id = rack_id;
        g_assign_status[g_assigned_count] = BMS_ASSIGN_STATUS_ASSIGNED;
        g_assigned_count++;
    }
}

