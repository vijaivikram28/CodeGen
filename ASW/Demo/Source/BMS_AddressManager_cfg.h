
#ifndef BMS_ADDRESS_MANAGER_CFG_H
#define BMS_ADDRESS_MANAGER_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Maximum supported racks */
#define BMS_AM_MAX_RACKS               (64U)

/* Timeout for rack responses during initialization (ms) */
#define BMS_AM_RESPONSE_TIMEOUT_MS     (100U)

/* Maximum retry attempts */
#define BMS_AM_MAX_RETRIES             (5U)

/* Default address reserved for unconfigured racks */
#define BMS_AM_DEFAULT_ADDRESS         (0x00U)

/* Start address to assign (avoid 0x00 reserved) */
#define BMS_AM_START_ADDRESS           (0x01U)

/* Include guard end */
#ifdef __cplusplus
}
#endif

#endif /* BMS_ADDRESS_MANAGER_CFG_H */