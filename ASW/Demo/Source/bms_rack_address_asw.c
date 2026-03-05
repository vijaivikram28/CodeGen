
#include "bms_rack_address_asw.h"
#include <string.h>

/* Global Variables */
static BmsAddressAssignmentResult aswResult;
static uint8_t addressPool[BMS_MAX_SUPPORTED_RACKS];

/* Simulated BSW APIs (Create if missing) */
__attribute__((weak)) HAL_StatusTypeDef BSW_CAN_Send(uint32_t id, uint8_t *data, uint8_t len)
{
    return HAL_OK;
}

__attribute__((weak)) HAL_StatusTypeDef BSW_CAN_Receive(uint32_t *id, uint8_t *data, uint8_t *len, uint32_t timeout)
{
    return HAL_TIMEOUT;
}

/* Internal local functions */
static void BMS_ASW_ClearData(void)
{
    memset(addressPool, 0x00, sizeof(addressPool));
    memset(&aswResult, 0x00, sizeof(aswResult));
}

void BMS_ASW_Init(void)
{
    BMS_ASW_ClearData();
}

ASW_StatusType BMS_ASW_RequestToken(uint8_t rackDefaultId)
{
    uint8_t txData[1];
    txData[0] = rackDefaultId;

    if (BSW_CAN_Send(CAN_MSG_TOKEN_REQUEST, txData, 1u) != HAL_OK)
    {
        return ASW_ERROR;
    }

    uint32_t rxId;
    uint8_t rxData[8];
    uint8_t rxLen;

    if (BSW_CAN_Receive(&rxId, rxData, &rxLen, BMS_TOKEN_TIMEOUT_MS) == HAL_OK)
    {
        if (rxId == CAN_MSG_TOKEN_RESPONSE)
        {
            return ASW_OK;
        }
        return ASW_COLLISION;
    }

    return ASW_TIMEOUT;
}

ASW_StatusType BMS_ASW_AssignAddress(uint8_t assignedId)
{
    uint8_t txData[1];
    txData[0] = assignedId;

    if (BSW_CAN_Send(CAN_MSG_ASSIGN_ADDRESS, txData, 1u) != HAL_OK)
    {
        return ASW_ERROR;
    }

    uint32_t rxId;
    uint8_t rxData[8];
    uint8_t rxLen;

    if (BSW_CAN_Receive(&rxId, rxData, &rxLen, BMS_TOKEN_TIMEOUTMS) == HAL_OK)
    {
        if (rxId == CAN_MSG_ASSIGN_ACK)
        {
            return ASW_OK;
        }
    }

    return ASW_TIMEOUT;
}

ASW_StatusType BMS_ASW_ListenForRack(uint32_t timeoutMs)
{
    uint32_t rxId;
    uint8_t rxData[8];
    uint8_t rxLen;

    if (BSW_CAN_Receive(&rxId, rxData, &rxLen, timeoutMs) != HAL_OK)
    {
        return ASW_TIMEOUT;
    }

    if (rxId != CAN_MSG_RACK_ANNOUNCE)
    {
        return ASW_ERROR;
    }

    return ASW_OK;
}

ASW_StatusType BMS_ASW_RunAssignment(void)
{
    uint8_t nextAddress = 1;
    uint8_t retries = 0;

    while (nextAddress <= BMS_MAX_SUPPORTED_RACKS)
    {
        ASW_StatusType st;

        /* Wait for rack announcement */
        st = BMS_ASW_ListenForRack(BMS_TOKEN_TIMEOUT_MS);
        if (st == ASW_TIMEOUT)
        {
            retries++;
            if (retries > BMS_MAX_RETRY_ATTEMPTS)
            {
                break;
            }
            continue;
        }

        /* Request token */
        st = BMS_ASW_RequestToken(BMS_DEFAULT_RACK_ID);
        if (st == ASW_COLLISION)
        {
            continue;
        }
        else if (st != ASW_OK)
        {
            continue;
        }

        /* Assign Address */
        st = BMS_ASW_AssignAddress(nextAddress);
        if (st == ASW_OK)
        {
            addressPool[nextAddress - 1u] = nextAddress;
            aswResult.totalAssigned++;
            nextAddress++;
        }
    }

    if (BMS_MAX_SUPPORTED_RACKS > 0u)
    {
        aswResult.successRate =
            (float)aswResult.totalAssigned / (float)BMS_MAX_SUPPORTED_RACKS;
    }
    else
    {
        aswResult.successRate = 0.0f;
    }

    memcpy(aswResult.assignedAddresses, addressPool, sizeof(addressPool));

    return ASW_OK;
}

BmsAddressAssignmentResult BMS_ASW_GetResult(void)
{
    return aswResult;
}


