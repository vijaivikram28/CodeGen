
#ifndef BMS_ADDRESS_MANAGER_CFG_H
#define BMS_ADDRESS_MANAGER_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration macros for BMS Address Manager */

/* Timeout (ms) for rack responses during initialization (per token) */
#ifndef BMS_RESPONSE_TIMEOUT_MS
#define BMS_RESPONSE_TIMEOUT_MS    (100U)
#endif

/* Maximum retry attempts for a conflicting token assignment */
#ifndef BMS_MAX_RETRIES
#define BMS_MAX_RETRIES            (5U)
#endif

/* Default address reserved (0x00) */
#ifndef BMS_DEFAULT_ADDRESS
#define BMS_DEFAULT_ADDRESS        (0x00U)
#endif

/* Maximum supported racks for this ASW */
#ifndef BMS_MAX_RACKS
#define BMS_MAX_RACKS              (64U)
#endif

/* Sanity checks */
#if (BMS_MAX_RACKS == 0U)
#error "BMS_MAX_RACKS must be at least 1"
#endif

#ifdef __cplusplus
}
#endif

#endif /* BMS_ADDRESS_MANAGER_CFG_H */