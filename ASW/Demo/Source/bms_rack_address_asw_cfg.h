
#ifndef BMS_RACK_ADDRESS_ASW_CFG_H
#define BMS_RACK_ADDRESS_ASW_CFG_H

#include <stdint.h>

/* Configurable Macros */
#define BMS_TOKEN_TIMEOUT_MS        100u
#define BMS_MAX_RETRY_ATTEMPTS      5u
#define BMS_DEFAULT_RACK_ID         0x00u
#define BMS_MAX_SUPPORTED_RACKS     64u

/* CAN Message IDs */
#define CAN_MSG_TOKEN_REQUEST       0x18FF01A0u
#define CAN_MSG_TOKEN_RESPONSE      0x18FF02A0u
#define CAN_MSG_ASSIGN_ADDRESS      0x18FF03A0u
#define CAN_MSG_ASSIGN_ACK          0x18FF04A0u
#define CAN_MSG_RACK_ANNOUNCE       0x18FF05A0u

#endif /* BMS_RACK_ADDRESS_ASW_CFG_H */
```