/* ===== File: ASW/Test/Source/BMS_AddressManager.h ===== */
#ifndef BMS_ADDRESS_MANAGER_H
#define BMS_ADDRESS_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* HAL and BSW includes - preserved for interface compatibility */
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_fdcan.h"
#include "CodeGen_GPIO.h"

/* Configuration header - must provide BMS_MAX_RACKS */
#include "BMS_AddressManager_cfg.h"

/* Public types */

/**
 * @brief Entry holding assigned address and the rack identifier reported by the rack.
 *
 * address: Assigned CAN address in the range [1 .. BMS_MAX_RACKS].
 *          Value 0 denotes an unassigned entry.
 * rack_id: Application-defined rack identifier as reported by the rack.
 */
typedef struct
{
    uint8_t address;       /* Assigned CAN address (1..BMS_MAX_RACKS), 0 = unassigned */
    uint8_t rack_id;       /* Rack's unique reported ID (application-defined) */
} BMS_AssignedEntry_t;

/* Public API
 * Note: Interfaces are preserved. Callers must respect the documented parameters.
 */

/**
 * @brief Initialize Address Manager subsystem and CAN GPIO/peripheral.
 *
 * This function prepares the address manager state and configures any required
 * CAN or GPIO resources. It is safe to call only once during system bring-up.
 *
 * @return HAL_OK on success, HAL_ERROR on failure.
 */
HAL_StatusTypeDef BMS_AddressManager_Init(void);

/**
 * @brief Deinitialize Address Manager subsystem and CAN peripheral.
 *
 * Releases resources allocated by BMS_AddressManager_Init. After deinitialization,
 * the subsystem must be re-initialized before use.
 *
 * @return HAL_OK on success, HAL_ERROR on failure.
 */
HAL_StatusTypeDef BMS_AddressManager_DeInit(void);

/**
 * @brief Perform token-based dynamic address assignment for expected_racks devices.
 *
 * Blocking call that will attempt to assign up to BMS_MAX_RACKS addresses.
 * The implementation performs deterministic retries and validates responses to
 * ensure robust assignment in the presence of transient errors.
 *
 * @param expected_racks Number of racks expected to be assigned (0 .. BMS_MAX_RACKS).
 *                       Passing 0 results in an immediate HAL_OK and no assignments.
 * @return HAL_OK if the assignment procedure executed (even if some racks did not
 *         receive an address), HAL_ERROR on internal failure (e.g. peripheral error).
 *
 * After success, assigned entries are available via BMS_AddressManager_GetAssignedList.
 */
HAL_StatusTypeDef BMS_AddressManager_AssignAddresses(uint8_t expected_racks);

/**
 * @brief Retrieve the assigned address list.
 *
 * Copies up to max_len assigned entries into out_list in ascending address order.
 * If out_list is NULL or max_len is 0, the function returns 0 and does not write.
 *
 * @param out_list Pointer to buffer of BMS_AssignedEntry_t elements to be filled.
 * @param max_len Maximum number of entries the buffer can hold.
 * @return Number of entries written to out_list (0 .. BMS_MAX_RACKS).
 */
uint8_t BMS_AddressManager_GetAssignedList(BMS_AssignedEntry_t * out_list, uint8_t max_len);

/**
 * @brief Get success rate in percent (0..100).
 *
 * This returns the integer percentage of successfully assigned racks relative to
 * expected_racks. For expected_racks == 0 the function returns 0.
 *
 * @param expected_racks Number of racks expected to be assigned.
 * @return Success rate percentage (0..100).
 */
uint8_t BMS_AddressManager_GetSuccessRatePercent(uint8_t expected_racks);

/* Helper macros and validation helpers
 * - Preserve compatibility while providing small, safe utilities for callers
 * - Avoid complex macros that could introduce undefined behavior
 */

#ifndef BMS_MAX_RACKS
/* If configuration is missing, provide a conservative default to avoid build errors.
 * Real systems should supply BMS_MAX_RACKS via BMS_AddressManager_cfg.h
 */
#define BMS_MAX_RACKS (8U)
#endif

/**
 * @brief Validate a candidate address for the system.
 * @param addr Candidate address value (integer type)
 * @return true if addr is within valid assigned range, false otherwise
 */
static inline bool BMS_Address_IsValid(uint8_t addr)
{
    return (addr != 0U) && (addr <= (uint8_t)BMS_MAX_RACKS);
}

#ifdef __cplusplus
}
#endif

#endif /* BMS_ADDRESS_MANAGER_H */
