
#ifndef BMS_ADDRESS_MANAGER_H
#define BMS_ADDRESS_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"
#include "CodeGen_CAN_operations.h"
#include "BMS_AddressManager_cfg.h"

/* Version */
#define BMS_AM_VERSION_MAJOR  (1U)
#define BMS_AM_VERSION_MINOR  (0U)

/* Command bytes in CAN payload */
#define BMS_AM_CMD_TOKEN              (0xA0U)
#define BMS_AM_CMD_ACK                (0xA1U)
#define BMS_AM_CMD_ASSIGN             (0xA2U)
#define BMS_AM_CMD_ASSIGN_CONFIRM     (0xA3U)

/* Size limits */
#define BMS_AM_DEVICE_ID_SIZE_BYTES   (4U)
#define BMS_AM_MAX_PAYLOAD            (8U)

/* Typedefs */
typedef uint8_t BMS_AM_AddressType;
typedef uint32_t BMS_AM_DeviceIdType;

/* Result of address assignment per device */
typedef struct
{
    BMS_AM_DeviceIdType device_id;
    BMS_AM_AddressType  address;
    bool                confirmed;
} BMS_AM_AssignmentEntryType;

/* Public API */

/* Initialize the Address Manager and underlying CAN.
 * pCanConfig: pointer to CAN configuration structure (should be valid).
 * Returns HAL_OK on success, HAL_ERROR otherwise.
 */
HAL_StatusTypeDef BMS_AM_Init(BMS_CANConfigTypeDef * pCanConfig);

/* Perform token-based assignment for expected_racks devices.
 * pCanConfig: pointer to CAN configuration structure (should be valid).
 * expected_racks: number of racks to attempt to assign (<= BMS_AM_MAX_RACKS).
 * Returns HAL_OK when procedure completed (some assignments may still fail),
 * HAL_ERROR on fatal error (e.g. CAN init/config failure).
 */
HAL_StatusTypeDef BMS_AM_AssignAddresses(BMS_CANConfigTypeDef * pCanConfig,
                                         uint8_t expected_racks);

/* Returns number of successfully assigned devices. */
uint8_t BMS_AM_GetAssignedCount(void);

/* Retrieve assigned address by index (0..assigned_count-1).
 * pAddress: out parameter to receive address.
 * Returns HAL_OK on success, HAL_ERROR if index out of bounds.
 */
HAL_StatusTypeDef BMS_AM_GetAssignedAddress(uint8_t index, BMS_AM_AddressType * pAddress);

/* Compute success rate as percentage (0..100) based on last run.
 * expected_racks parameter used from last BMS_AM_AssignAddresses call.
 */
uint8_t BMS_AM_GetSuccessRatePercent(void);

/* Reset stored assignments (keeps CAN running). */
void BMS_AM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_ADDRESS_MANAGER_H */

