
#ifndef BMS_ADDRESS_MANAGER_H
#define BMS_ADDRESS_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* HAL and BSW includes */
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_fdcan.h"
#include "CodeGen_GPIO.h"

/* Configuration header */
#include "BMS_AddressManager_cfg.h"

/* Public types */

/* Entry holding assigned address and the rack identifier reported by the rack */
typedef struct
{
    uint8_t address;       /* Assigned CAN address (1..BMS_MAX_RACKS) */
    uint8_t rack_id;       /* Rack's unique reported ID (application-defined) */
} BMS_AssignedEntry_t;

/* Public API */

/**
 * @brief Initialize Address Manager subsystem and CAN GPIO/peripheral.
 * @return HAL_OK on success, HAL_ERROR on failure.
 */
HAL_StatusTypeDef BMS_AddressManager_Init(void);

/**
 * @brief Deinitialize Address Manager subsystem and CAN peripheral.
 * @return HAL_OK on success, HAL_ERROR on failure.
 */
HAL_StatusTypeDef BMS_AddressManager_DeInit(void);

/**
 * @brief Perform token-based dynamic address assignment for expected_racks devices.
 *        This is blocking and will attempt to assign up to BMS_MAX_RACKS addresses.
 * @param expected_racks Number of racks expected to be assigned (<= BMS_MAX_RACKS).
 * @return HAL_OK if assignment procedure executed successfully, HAL_ERROR otherwise.
 *
 * After the call, assigned entries are available via BMS_AddressManager_GetAssignedList.
 */
HAL_StatusTypeDef BMS_AddressManager_AssignAddresses(uint8_t expected_racks);

/**
 * @brief Retrieve the assigned address list.
 * @param out_list Pointer to buffer of BMS_AssignedEntry_t elements to be filled.
 * @param max_len Maximum number of entries the buffer can hold.
 * @return Number of entries written to out_list.
 */
uint8_t BMS_AddressManager_GetAssignedList(BMS_AssignedEntry_t * out_list, uint8_t max_len);

/**
 * @brief Get success rate in percent (0..100).
 * @param expected_racks Number of racks expected to be assigned.
 * @return Success rate percentage (rounded down).
 */
uint8_t BMS_AddressManager_GetSuccessRatePercent(uint8_t expected_racks);

#ifdef __cplusplus
}
#endif

#endif /* BMS_ADDRESS_MANAGER_H */

