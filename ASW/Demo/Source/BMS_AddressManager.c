
#include "BMS_AddressManager.h"
#include <string.h>

/* Internal static storage - MISRA: file scope static variables */
static BMS_AM_AssignmentEntryType BMS_AM_Assignments[BMS_AM_MAX_RACKS];
static uint8_t BMS_AM_AssignedCount = 0U;
static uint8_t BMS_AM_ExpectedRacks = 0U;
static BMS_CANConfigTypeDef * BMS_AM_pCan = NULL;

/* Local helper prototypes */
static HAL_StatusTypeDef BMS_AM_SendToken(BMS_CANConfigTypeDef * pCan, BMS_AM_AddressType proposed_addr);
static HAL_StatusTypeDef BMS_AM_WaitForResponses(BMS_CANConfigTypeDef * pCan,
                                                 uint32_t timeout_ms,
                                                 BMS_AM_DeviceIdType * responders,
                                                 uint8_t * responder_count);
static HAL_StatusTypeDef BMS_AM_SendAssign(BMS_CANConfigTypeDef * pCan,
                                           BMS_AM_DeviceIdType device_id,
                                           BMS_AM_AddressType address);
static HAL_StatusTypeDef BMS_AM_WaitForAssignConfirm(BMS_CANConfigTypeDef * pCan,
                                                     BMS_AM_DeviceIdType device_id,
                                                     uint32_t timeout_ms);
static uint32_t BMS_AM_ParseDeviceId(const uint8_t * data, uint32_t len);

/* Public API implementations */

HAL_StatusTypeDef BMS_AM_Init(BMS_CANConfigTypeDef * pCanConfig)
{
    HAL_StatusTypeDef status;

    if (pCanConfig == NULL)
    {
        return HAL_ERROR;
    }

    /* Initialize CAN peripheral using BSW APIs */
    status = BMS_CAN_Init(pCanConfig);
    if (status != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Configure CAN bit timing */
    status = BMS_CAN_ConfigureBitTiming(pCanConfig);
    if (status != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Configure filters to receive assignment responses */
    status = BMS_CAN_ConfigureFilters(pCanConfig);
    if (status != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Store pointer for subsequent calls */
    BMS_AM_pCan = pCanConfig;

    /* Reset internal assignment storage */
    BMS_AM_Reset();

    return HAL_OK;
}

HAL_StatusTypeDef BMS_AM_AssignAddresses(BMS_CANConfigTypeDef * pCanConfig,
                                         uint8_t expected_racks)
{
    uint8_t proposed_addr = BMS_AM_START_ADDRESS;
    uint8_t attempts;
    uint8_t progress_made;
    uint8_t pass;
    HAL_StatusTypeDef status;

    if (pCanConfig == NULL)
    {
        return HAL_ERROR;
    }

    if (expected_racks == 0U)
    {
        return HAL_OK;
    }

    if (expected_racks > BMS_AM_MAX_RACKS)
    {
        expected_racks = BMS_AM_MAX_RACKS;
    }

    BMS_AM_ExpectedRacks = expected_racks;

    /* start from 1..MAX - 0x00 reserved as default address */
    proposed_addr = BMS_AM_START_ADDRESS;

    /* We'll attempt multiple passes to try to resolve collisions and hot-plugging */
    for (pass = 0U; pass < 8U; ++pass) /* limit passes to avoid infinite loops */
    {
        progress_made = 0U;

        for (proposed_addr = BMS_AM_START_ADDRESS;
             (proposed_addr <= BMS_AM_MAX_RACKS) &&
             (BMS_AM_AssignedCount < BMS_AM_ExpectedRacks);
             ++proposed_addr)
        {
            /* skip addresses already assigned */
            bool already_assigned = false;
            uint8_t i;
            for (i = 0U; i < BMS_AM_AssignedCount; ++i)
            {
                if (BMS_AM_Assignments[i].address == proposed_addr)
                {
                    already_assigned = true;
                    break;
                }
            }
            if (already_assigned)
            {
                continue;
            }

            /* Try to obtain exactly one responder for this proposed address */
            for (attempts = 0U; attempts < BMS_AM_MAX_RETRIES; ++attempts)
            {
                BMS_AM_DeviceIdType responders[BMS_AM_MAX_RACKS];
                uint8_t responder_count = 0U;

                status = BMS_AM_SendToken(pCanConfig, proposed_addr);
                if (status != HAL_OK)
                {
                    /* Try next attempt */
                    continue;
                }

                status = BMS_AM_WaitForResponses(pCanConfig,
                                                BMS_AM_RESPONSE_TIMEOUT_MS,
                                                responders,
                                                &responder_count);
                if (status != HAL_OK)
                {
                    /* No response or CAN error - retry */
                    continue;
                }

                if (responder_count == 0U)
                {
                    /* nobody responded - nothing to assign */
                    break;
                }
                else if (responder_count == 1U)
                {
                    /* single device responded - attempt to assign and confirm */
                    status = BMS_AM_SendAssign(pCanConfig, responders[0], proposed_addr);
                    if (status != HAL_OK)
                    {
                        /* assignment send failed - retry assignment attempts */
                        continue;
                    }

                    status = BMS_AM_WaitForAssignConfirm(pCanConfig, responders[0],
                                                         BMS_AM_RESPONSE_TIMEOUT_MS);
                    if (status == HAL_OK)
                    {
                        /* Record assignment */
                        if (BMS_AM_AssignedCount < BMS_AM_MAX_RACKS)
                        {
                            BMS_AM_Assignments[BMS_AM_AssignedCount].device_id = responders[0];
                            BMS_AM_Assignments[BMS_AM_AssignedCount].address = proposed_addr;
                            BMS_AM_Assignments[BMS_AM_AssignedCount].confirmed = true;
                            ++BMS_AM_AssignedCount;
                        }
                        progress_made = 1U;
                        break; /* move to next address */
                    }
                    else
                    {
                        /* assignment not confirmed - retry */
                        continue;
                    }
                }
                else
                {
                    /* Collision detected (multiple responders)
                     * Use random backoff before retrying to reduce collision likelihood.
                     */
                    uint32_t backoff_ms = (HAL_GetTick() & 0x1FU) + 10U;
                    HAL_Delay((uint32_t)backoff_ms);
                    /* then retry */
                    continue;
                }
            } /* attempts */

            /* short delay between sequential addresses to allow bus settle */
            HAL_Delay(2U);
        } /* for proposed_addr */

        if ((BMS_AM_AssignedCount >= BMS_AM_ExpectedRacks) || (progress_made == 0U))
        {
            /* either done or no progress in this pass */
            break;
        }
    } /* passes */

    /* Completed attempt - success or partial success */
    (void) pCanConfig;
    return HAL_OK;
}

uint8_t BMS_AM_GetAssignedCount(void)
{
    return BMS_AM_AssignedCount;
}

HAL_StatusTypeDef BMS_AM_GetAssignedAddress(uint8_t index, BMS_AM_AddressType * pAddress)
{
    if (pAddress == NULL)
    {
        return HAL_ERROR;
    }
    if (index >= BMS_AM_AssignedCount)
    {
        return HAL_ERROR;
    }
    *pAddress = BMS_AM_Assignments[index].address;
    return HAL_OK;
}

uint8_t BMS_AM_GetSuccessRatePercent(void)
{
    if (BMS_AM_ExpectedRacks == 0U)
    {
        return 0U;
    }

    return (uint8_t)((uint32_t)BMS_AM_AssignedCount * 100U / (uint32_t)BMS_AM_ExpectedRacks);
}

void BMS_AM_Reset(void)
{
    uint8_t i;
    for (i = 0U; i < BMS_AM_MAX_RACKS; ++i)
    {
        BMS_AM_Assignments[i].device_id = 0U;
        BMS_AM_Assignments[i].address = BMS_AM_DEFAULT_ADDRESS;
        BMS_AM_Assignments[i].confirmed = false;
    }
    BMS_AM_AssignedCount = 0U;
    BMS_AM_ExpectedRacks = 0U;
}

/* Internal helper implementations */

static HAL_StatusTypeDef BMS_AM_SendToken(BMS_CANConfigTypeDef * pCan, BMS_AM_AddressType proposed_addr)
{
    uint8_t payload[BMS_AM_MAX_PAYLOAD];
    uint32_t id;
    HAL_StatusTypeDef status;

    /* Format: [CMD_TOKEN][proposed_addr][reserved..][timestamp LSBs optional] */
    (void) memset(payload, 0, sizeof(payload));
    payload[0] = BMS_AM_CMD_TOKEN;
    payload[1] = proposed_addr;

    id = BMS_CAN_BROADCAST_ID;

    status = BMS_CAN_Transmit(pCan, id, payload, 2U);
    return status;
}

static HAL_StatusTypeDef BMS_AM_WaitForResponses(BMS_CANConfigTypeDef * pCan,
                                                 uint32_t timeout_ms,
                                                 BMS_AM_DeviceIdType * responders,
                                                 uint8_t * responder_count)
{
    uint32_t start;
    HAL_StatusTypeDef status;
    uint32_t rx_id;
    uint8_t rx_data[BMS_AM_MAX_PAYLOAD];
    uint32_t rx_len;
    uint8_t count = 0U;

    if ((responders == NULL) || (responder_count == NULL))
    {
        return HAL_ERROR;
    }

    start = HAL_GetTick();

    /* Wait until timeout and collect all responses on response ID */
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        rx_len = (uint32_t)BMS_AM_MAX_PAYLOAD;
        status = BMS_CAN_Receive(pCan, &rx_id, rx_data, &rx_len);
        if (status == HAL_OK)
        {
            /* only process expected response ID and ACK command */
            if (rx_id == BMS_CAN_RESPONSE_ID)
            {
                if (rx_len >= 1U)
                {
                    if (rx_data[0] == BMS_AM_CMD_ACK)
                    {
                        /* parse device id */
                        if (rx_len >= (1U + BMS_AM_DEVICE_ID_SIZE_BYTES))
                        {
                            BMS_AM_DeviceIdType dev = BMS_AM_ParseDeviceId(&rx_data[1], (uint32_t)BMS_AM_DEVICE_ID_SIZE_BYTES);
                            /* store unique responders (avoid duplicates) */
                            bool already = false;
                            uint8_t i;
                            for (i = 0U; i < count; ++i)
                            {
                                if (responders[i] == dev)
                                {
                                    already = true;
                                    break;
                                }
                            }
                            if (already == false)
                            {
                                if (count < BMS_AM_MAX_RACKS)
                                {
                                    responders[count] = dev;
                                    ++count;
                                }
                            }
                        }
                    }
                    else if (rx_data[0] == BMS_AM_CMD_ASSIGN_CONFIRM)
                    {
                        /* assignment confirm may be received here - ignore for collection */
                    }
                    else
                    {
                        /* ignore other commands */
                    }
                }
            }
            /* else ignore other IDs */
        }
        else if (status == HAL_TIMEOUT)
        {
            /* no message at this instant - small wait and continue */
            HAL_Delay(1U);
        }
        else
        {
            /* CAN error - abort */
            return HAL_ERROR;
        }

        /* If we already saw more than one responder and want early exit, we could, but collect within timeout helps */
    }

    *responder_count = count;
    return HAL_OK;
}

static HAL_StatusTypeDef BMS_AM_SendAssign(BMS_CANConfigTypeDef * pCan,
                                           BMS_AM_DeviceIdType device_id,
                                           BMS_AM_AddressType address)
{
    uint8_t payload[BMS_AM_MAX_PAYLOAD];
    uint32_t id;
    HAL_StatusTypeDef status;

    (void) memset(payload, 0, sizeof(payload));
    payload[0] = BMS_AM_CMD_ASSIGN;
    /* device id encoded to ensure only targeted device acts on it */
    payload[1] = (uint8_t)((device_id >> 24) & 0xFFU);
    payload[2] = (uint8_t)((device_id >> 16) & 0xFFU);
    payload[3] = (uint8_t)((device_id >> 8) & 0xFFU);
    payload[4] = (uint8_t)(device_id & 0xFFU);
    payload[5] = address;

    /* Broadcast assign on bus - targeted device detects its device_id in payload */
    id = BMS_CAN_BROADCAST_ID;

    status = BMS_CAN_Transmit(pCan, id, payload, 6U);
    return status;
}

static HAL_StatusTypeDef BMS_AM_WaitForAssignConfirm(BMS_CANConfigTypeDef * pCan,
                                                     BMS_AM_DeviceIdType device_id,
                                                     uint32_t timeout_ms)
{
    uint32_t start;
    HAL_StatusTypeDef status;
    uint32_t rx_id;
    uint8_t rx_data[BMS_AM_MAX_PAYLOAD];
    uint32_t rx_len;

    start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        rx_len = (uint32_t)BMS_AM_MAX_PAYLOAD;
        status = BMS_CAN_Receive(pCan, &rx_id, rx_data, &rx_len);
        if (status == HAL_OK)
        {
            if (rx_id == BMS_CAN_RESPONSE_ID)
            {
                if (rx_len >= 1U)
                {
                    if (rx_data[0] == BMS_AM_CMD_ASSIGN_CONFIRM)
                    {
                        /* verify device id matches */
                        if (rx_len >= (1U + BMS_AM_DEVICE_ID_SIZE_BYTES))
                        {
                            BMS_AM_DeviceIdType dev = BMS_AM_ParseDeviceId(&rx_data[1], (uint32_t)BMS_AM_DEVICE_ID_SIZE_BYTES);
                            if (dev == device_id)
                            {
                                return HAL_OK;
                            }
                        }
                    }
                }
            }
            /* else ignore */
        }
        else if (status == HAL_TIMEOUT)
        {
            HAL_Delay(1U);
        }
        else
        {
            return HAL_ERROR;
        }
    }

    return HAL_TIMEOUT;
}

static uint32_t BMS_AM_ParseDeviceId(const uint8_t * data, uint32_t len)
{
    uint32_t id = 0U;
    uint32_t i;
    if ((data == NULL) || (len == 0U))
    {
        return 0U;
    }

    for (i = 0U; i < len; ++i)
    {
        id <<= 8U;
        id |= (uint32_t)(data[i] & 0xFFU);
    }
    return id;
}

