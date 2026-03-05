
#ifndef BMS_RACK_ADDRESS_ASW_H
#define BMS_RACK_ADDRESS_ASW_H

#include "stm32h7xx_hal.h"
#include "bms_rack_address_asw_cfg.h"
#include <stdint.h>
#include <stdbool.h>

/* Public Data Types */
typedef enum
{
    ASW_OK = 0,
    ASW_ERROR,
    ASW_TIMEOUT,
    ASW_COLLISION
} ASW_StatusType;

typedef struct
{
    uint8_t assignedAddresses[BMS_MAX_SUPPORTED_RACKS];
    uint8_t totalAssigned;
    float   successRate;
} BmsAddressAssignmentResult;

/* Public Function Prototypes */
void     BMS_ASW_Init(void);
ASW_StatusType BMS_ASW_RunAssignment(void);
BmsAddressAssignmentResult BMS_ASW_GetResult(void);

/* Internal Helper APIs (Exposed for unit testing) */
ASW_StatusType BMS_ASW_RequestToken(uint8_t rackDefaultId);
ASW_StatusType BMS_ASW_AssignAddress(uint8_t assignedId);
ASW_StatusType BMS_ASW_ListenForRack(uint32_t timeoutMs);

#endif /* BMS_RACK_ADDRESS_ASW_H */


