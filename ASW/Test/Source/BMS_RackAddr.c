
#include "BMS_RackAddr.h"
#include "BMS_RackAddr_cfg.h"

/* Standard headers */
#include <string.h>
#include <stdlib.h>

/* HAL headers for STM32H7 - required by technical requirements.
 * These headers are typically provided by CubeMX generated project.
 * The actual CAN handle must be defined in BSW/hardware layer and linked with project.
 */
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_can.h"

/* ===== Internal BSW-like CAN interface (simple wrapper) =====
 * These functions act as Basic Software (BSW) CAN interface used by the ASW.
 * If an actual BSW exists, these can be replaced by the real implementations
 * without changing the ASW interface functions above.
 */

/* External CAN handle provided by board support package */
extern CAN_HandleTypeDef hcan1;

/* Callback prototype for reception */
typedef void (*CanIf_RxCallback_t)(uint32_t ext_id, const uint8_t *data, uint8_t dlc);

/* Static storage for registered callback */
static CanIf_RxCallback_t CanIf_RxCb = NULL;

/* Send a CAN Extended ID message.
 * id: 29-bit extended ID
 * data: payload pointer
 * dlc: 0..8
 * Returns true on acceptance, false otherwise.
 */
static bool CanIf_SendExtMsg(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef txh;
    uint32_t txmailbox;
    uint8_t txdata[8] = {0};

    if (dlc > 8U)
    {
        return false;
    }

    /* Fill header */
    txh.StdId = 0U;
    txh.ExtId = (id & 0x1FFFFFFFU);
    txh.IDE = CAN_ID_EXT;
    txh.RTR = CAN_RTR_DATA;
    txh.DLC = dlc;
    txh.TransmitGlobalTime = DISABLE;

    (void)memcpy(txdata, data, dlc);

    if (HAL_CAN_AddTxMessage(&hcan1, &txh, txdata, &txmailbox) != HAL_OK)
    {
        return false;
    }

    /* Optionally wait for mailbox empty or immediate return (non-blocking). */
    return true;
}

/* This function should be called by the underlying BSW CAN IRQ/Rx handler
 * when a new extended frame is received. Many HAL projects call HAL_CAN_RxFifo0MsgPendingCallback().
 * Here we provide a simple wrapper that ASW can rely upon. Integration: call CanIf_ReceiveIndication()
 * from the HAL callback with received parameters.
 */
void CanIf_ReceiveIndication(uint32_t ext_id, const uint8_t *data, uint8_t dlc)
{
    if (CanIf_RxCb != NULL)
    {
        CanIf_RxCb(ext_id, data, dlc);
    }
}

/* Register reception callback */
static void CanIf_RegisterRxCallback(CanIf_RxCallback_t cb)
{
    CanIf_RxCb = cb;
}

/* ===== End BSW-like CAN interface ===== */

/* Internal definitions and structures */

typedef enum
{
    TOKEN_MSG = 0x01U,   /* Master -> Bus: Token containing candidate address */
    CLAIM_MSG  = 0x02U,  /* Rack -> Bus: Claim containing HW_ID */
    ACK_MSG    = 0x03U   /* Master -> Bus: Acknowledge assignment */
} BMS_RackAddr_MsgType_t;

/* Message ID allocation (29-bit extended IDs)
 * We define a base extended ID and use different low bits for message types.
 */
#define BMS_RACKADDR_CAN_BASEID     (0x1A000000UL)  /* Arbitrary chosen 29-bit base ID */
#define BMS_RACKADDR_CAN_TOKEN_ID   (BMS_RACKADDR_CAN_BASEID | 0x10U)
#define BMS_RACKADDR_CAN_CLAIM_ID   (BMS_RACKADDR_CAN_BASEID | 0x20U)
#define BMS_RACKADDR_CAN_ACK_ID     (BMS_RACKADDR_CAN_BASEID | 0x30U)

/* Internal mailbox for collecting claims during a token window */
typedef struct
{
    uint32_t hw_id;
    uint32_t timestamp_ms;
} claim_entry_t;

/* Static state */
static BMS_RackInfo_t rack_table[BMS_RACKADDR_MAX_RACKS];
static claim_entry_t claim_buffer[BMS_RACKADDR_MAX_RACKS];
static uint8_t claim_count = 0U;
static uint8_t assigned_count = 0U;
static uint16_t last_success_rate_hundredths = 0U;
static bool initialized = false;

/* Forward declarations */
static void local_rx_callback(uint32_t ext_id, const uint8_t *data, uint8_t dlc);
static void clear_claim_buffer(void);
static uint32_t get_tick_ms(void);

/* Public APIs */

BMS_RackAddr_Status_t BMS_RackAddr_Init(void)
{
    uint8_t i;

    if (initialized)
    {
        return BMS_RACKADDR_OK;
    }

    /* Initialize local rack table to default address values */
    for (i = 0U; i < BMS_RACKADDR_MAX_RACKS; i++)
    {
        rack_table[i].assigned_addr = BMS_RACKADDR_DEFAULT_ADDR;
        rack_table[i].hw_id = 0U;
        rack_table[i].active = false;
    }

    /* register CAN RX callback */
    CanIf_RegisterRxCallback(local_rx_callback);

    clear_claim_buffer();

    assigned_count = 0U;
    last_success_rate_hundredths = 0U;
    initialized = true;

    return BMS_RACKADDR_OK;
}

BMS_RackAddr_Status_t BMS_RackAddr_AssignAddresses(uint8_t expected_racks)
{
    uint8_t addr_candidate;
    uint8_t retries;
    uint32_t token_payload[2]; /* [0]=msg type + address, [1]=reserved */
    uint32_t start_ms;
    uint8_t i;
    uint8_t local_assigned = 0U;

    if (!initialized)
    {
        return BMS_RACKADDR_E_NOT_OK;
    }
    if (expected_racks == 0U || expected_racks > BMS_RACKADDR_MAX_RACKS)
    {
        return BMS_RACKADDR_E_INVALID_PARAM;
    }

    /* Reset prior assignments if any */
    for (i = 0U; i < BMS_RACKADDR_MAX_RACKS; i++)
    {
        rack_table[i].assigned_addr = BMS_RACKADDR_DEFAULT_ADDR;
        rack_table[i].hw_id = 0U;
        rack_table[i].active = false;
    }
    assigned_count = 0U;
    clear_claim_buffer();

    /* Token based assignment:
     * Iterate candidate addresses 1..expected_racks (or up to max supported racks).
     * For each candidate, send TOKEN and wait for claims. If exactly one claim -> assign.
     * If multiple claims -> collision resolution: random backoff and retries until attempts exhausted.
     * If no claims -> move on.
     */

    for (addr_candidate = 1U; (addr_candidate <= expected_racks) && (assigned_count < expected_racks); addr_candidate++)
    {
        retries = 0U;
        bool assigned_this_slot = false;

        while (retries < BMS_RACKADDR_MAX_RETRIES)
        {
            /* Clear claim buffer for fresh token window */
            clear_claim_buffer();

            /* Build token payload: [0] : 0x01 (TOKEN_MSG) | addr_candidate in low byte */
            token_payload[0] = (uint32_t)TOKEN_MSG << 24U; /* high byte: msg type for clarity */
            token_payload[0] |= (uint32_t)addr_candidate;

            /* Transmit TOKEN as extended CAN frame */
            (void)CanIf_SendExtMsg(BMS_RACKADDR_CAN_TOKEN_ID, (uint8_t *)&token_payload[0], 4U);

            /* Wait for responses during configured timeout */
            start_ms = get_tick_ms();
            while ((get_tick_ms() - start_ms) < BMS_RACKADDR_RESPONSE_TIMEOUT_MS)
            {
                /* Busy wait small amount - in real system use event/OS wait */
                /* Claim frames are captured in interrupt via local_rx_callback */
            }

            /* Analyze claims */
            if (claim_count == 0U)
            {
                /* No rack claimed this candidate address */
                assigned_this_slot = false;
                break; /* move to next candidate address */
            }
            else if (claim_count == 1U)
            {
                /* Single claimant -> assign address */
                uint32_t claimant_hw = claim_buffer[0].hw_id;
                /* Store assignment in table: find next free slot */
                for (i = 0U; i < BMS_RACKADDR_MAX_RACKS; i++)
                {
                    if (rack_table[i].assigned_addr == BMS_RACKADDR_DEFAULT_ADDR)
                    {
                        rack_table[i].assigned_addr = addr_candidate;
                        rack_table[i].hw_id = claimant_hw;
                        rack_table[i].active = true;
                        assigned_count++;
                        local_assigned++;
                        assigned_this_slot = true;
                        break;
                    }
                }

                /* Send ACK to notify rack of assignment (optional): include assigned addr and HW_ID */
                {
                    uint8_t ack_data[8] = {0};
                    ack_data[0] = (uint8_t)ACK_MSG;
                    ack_data[1] = addr_candidate;
                    ack_data[2] = (uint8_t)(claimant_hw & 0xFFU);
                    ack_data[3] = (uint8_t)((claimant_hw >> 8U) & 0xFFU);
                    ack_data[4] = (uint8_t)((claimant_hw >> 16U) & 0xFFU);
                    ack_data[5] = (uint8_t)((claimant_hw >> 24U) & 0xFFU);
                    (void)CanIf_SendExtMsg(BMS_RACKADDR_CAN_ACK_ID, ack_data, 6U);
                }

                break; /* assigned, proceed to next candidate */
            }
            else
            {
                /* Collision detected: multiple claims. Try resolution */
                retries++;
                /* Informational: optionally broadcast collision notice - omitted to keep payload small.
                 * Randomized backoff before retry to reduce repeated collisions.
                 */
                uint32_t backoff_ms = (uint32_t)( (rand() % 50U) + 10U ); /* 10..59 ms randomized small backoff */
                uint32_t t0 = get_tick_ms();
                while ((get_tick_ms() - t0) < backoff_ms)
                {
                    /* wait */
                }
                /* retry loop continues */
            }
        } /* retries loop */

        /* If unassigned and retries exhausted due to collisions, mark as collision and continue */
        if ((retries >= BMS_RACKADDR_MAX_RETRIES) && (!assigned_this_slot) && (claim_count > 1U))
        {
            /* Record none assigned for this candidate; continue to next candidate */
            /* Optionally log collision - not implemented here */
            continue;
        }
    } /* for each candidate */

    /* Compute success rate in hundredths of percent */
    if (expected_racks != 0U)
    {
        last_success_rate_hundredths = (uint16_t)((10000U * (uint32_t)assigned_count) / (uint32_t)expected_racks);
    }
    else
    {
        last_success_rate_hundredths = 0U;
    }

    /* return OK if at least one assigned, otherwise indicate timeout/no devices */
    if (assigned_count > 0U)
    {
        return BMS_RACKADDR_OK;
    }

    return BMS_RACKADDR_E_TIMEOUT;
}

BMS_RackAddr_Status_t BMS_RackAddr_GetAssignedList(uint8_t *assigned_addrs, uint8_t max_out, uint8_t *out_count)
{
    uint8_t i;
    uint8_t idx = 0U;

    if ((assigned_addrs == NULL) || (out_count == NULL) || (max_out == 0U))
    {
        return BMS_RACKADDR_E_INVALID_PARAM;
    }

    for (i = 0U; i < BMS_RACKADDR_MAX_RACKS; i++)
    {
        if (rack_table[i].assigned_addr != BMS_RACKADDR_DEFAULT_ADDR)
        {
            if (idx < max_out)
            {
                assigned_addrs[idx] = rack_table[i].assigned_addr;
                idx++;
            }
            else
            {
                /* buffer full - stop filling */
                break;
            }
        }
    }

    *out_count = idx;
    return BMS_RACKADDR_OK;
}

BMS_RackAddr_Status_t BMS_RackAddr_GetSuccessRate(uint16_t *success_rate_hundredths_percent)
{
    if (success_rate_hundredths_percent == NULL)
    {
        return BMS_RACKADDR_E_INVALID_PARAM;
    }

    *success_rate_hundredths_percent = last_success_rate_hundredths;
    return BMS_RACKADDR_OK;
}

void BMS_RackAddr_Reset(void)
{
    uint8_t i;
    for (i = 0U; i < BMS_RACKADDR_MAX_RACKS; i++)
    {
        rack_table[i].assigned_addr = BMS_RACKADDR_DEFAULT_ADDR;
        rack_table[i].hw_id = 0U;
        rack_table[i].active = false;
    }
    assigned_count = 0U;
    clear_claim_buffer();
    last_success_rate_hundredths = 0U;
}

/* ===== Internal helper implementations ===== */

/* Local reception callback invoked by CanIf_ReceiveIndication */
static void local_rx_callback(uint32_t ext_id, const uint8_t *data, uint8_t dlc)
{
    uint32_t msg_type;
    (void)dlc;

    if (data == NULL)
    {
        return;
    }

    /* Determine message type from data or ID:
     * We defined separate CAN IDs for TOKEN, CLAIM, ACK.
     * We only process CLAIM frames here (racks claiming address).
     */

    if (ext_id == BMS_RACKADDR_CAN_CLAIM_ID)
    {
        /* Expected payload: [0]=msg type (CLAIM_MSG), [1..4]=hw_id (LSB first) */
        msg_type = (uint32_t)data[0];
        if ((uint8_t)msg_type != (uint8_t)CLAIM_MSG)
        {
            return;
        }

        /* Extract hw_id safely */
        uint32_t hwid = 0U;
        hwid  = (uint32_t)data[1];
        hwid |= ((uint32_t)data[2]) << 8U;
        hwid |= ((uint32_t)data[3]) << 16U;
        hwid |= ((uint32_t)data[4]) << 24U;

        /* Store claim in claim_buffer if capacity available.
         * Protect against concurrent access by disabling interrupts briefly.
         */
        __disable_irq();
        if (claim_count < BMS_RACKADDR_MAX_RACKS)
        {
            claim_buffer[claim_count].hw_id = hwid;
            claim_buffer[claim_count].timestamp_ms = get_tick_ms();
            claim_count++;
        }
        __enable_irq();
    }
    /* Ignore other message IDs for ASW */
}

/* Clear claim buffer */
static void clear_claim_buffer(void)
{
    __disable_irq();
    (void)memset(claim_buffer, 0, sizeof(claim_buffer));
    claim_count = 0U;
    __enable_irq();
}

/* Get current tick in ms using HAL function */
static uint32_t get_tick_ms(void)
{
    return HAL_GetTick();
}


