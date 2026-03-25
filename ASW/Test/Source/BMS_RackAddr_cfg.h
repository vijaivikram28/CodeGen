
#ifndef BMS_RACKADDR_CFG_H
#define BMS_RACKADDR_CFG_H

/* Configuration macros for BMS_RackAddr module
 * Adjust as necessary for target deployment.
 */

/* Maximum supported racks */
#ifndef BMS_RACKADDR_MAX_RACKS
#define BMS_RACKADDR_MAX_RACKS         (64U)
#endif

/* Timeout for rack responses during initialization (milliseconds) */
#ifndef BMS_RACKADDR_RESPONSE_TIMEOUT_MS
#define BMS_RACKADDR_RESPONSE_TIMEOUT_MS    (100U)
#endif

/* Maximum retry attempts on collision */
#ifndef BMS_RACKADDR_MAX_RETRIES
#define BMS_RACKADDR_MAX_RETRIES      (5U)
#endif

/* Default address for uninitialized racks */
#ifndef BMS_RACKADDR_DEFAULT_ADDR
#define BMS_RACKADDR_DEFAULT_ADDR     (0x00U)
#endif

/* Ensure RAND seed - user may override or call srand() in system init */
#ifndef BMS_RACKADDR_RAND_SEED
#define BMS_RACKADDR_RAND_SEED        (12345U)
#endif

/* Provide shorthand for TOKEN_MSG macro used in implementation */
#ifndef TOKEN_MSG
#define TOKEN_MSG    (1U)
#endif

/* Ensure inclusion of stdlib for rand seed usage in ASW init (seed once externally if desired). */
#include <stdlib.h>

#endif /* BMS_RACKADDR_CFG_H */