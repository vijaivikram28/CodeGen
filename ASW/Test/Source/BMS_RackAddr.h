
#ifndef BMS_RACKADDR_H
#define BMS_RACKADDR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BMS_RackAddr_cfg.h"
#include <stdint.h>
#include <stdbool.h>

/* Return codes */
typedef enum
{
    BMS_RACKADDR_OK = 0U,
    BMS_RACKADDR_E_NOT_OK = 1U,
    BMS_RACKADDR_E_TIMEOUT = 2U,
    BMS_RACKADDR_E_INVALID_PARAM = 3U,
    BMS_RACKADDR_E_COLLISION = 4U
} BMS_RackAddr_Status_t;

/* Rack descriptor */
typedef struct
{
    uint8_t  assigned_addr;   /* 0x00 means unassigned (default) */
    uint32_t hw_id;           /* unique hardware identifier supplied by rack in claim */
    bool     active;
} BMS_RackInfo_t;

/* Initialization of the Rack Address Manager
 * Must be called once at system start.
 */
BMS_RackAddr_Status_t BMS_RackAddr_Init(void);

/* Perform dynamic address assignment for up to expected_racks racks.
 * This call performs Initialization -> Assignment -> Collision Resolution -> Success Computation.
 * expected_racks: number of racks expected to be present (<= BMS_RACKADDR_MAX_RACKS)
 * Returns BMS_RACKADDR_OK on success (some or all addresses assigned), specific error codes otherwise.
 */
BMS_RackAddr_Status_t BMS_RackAddr_AssignAddresses(uint8_t expected_racks);

/* Retrieve assigned address list.
 * assigned_addrs: buffer to be filled with assigned addresses (non-zero values).
 * max_out: size of buffer in entries
 * out_count: pointer to number of addresses written
 * Returns BMS_RACKADDR_OK or error code.
 */
BMS_RackAddr_Status_t BMS_RackAddr_GetAssignedList(uint8_t *assigned_addrs, uint8_t max_out, uint8_t *out_count);

/* Get success rate as fixed-point percentage (0..10000) representing 0.01% units (e.g., 7500 == 75.00%).
 * This avoids floating point in constrained systems.
 */
BMS_RackAddr_Status_t BMS_RackAddr_GetSuccessRate(uint16_t *success_rate_hundredths_percent);

/* Force reset of internal state (keeps configuration macros) */
void BMS_RackAddr_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_RACKADDR_H */


