#ifndef BMS_EVENT_LOGGING_H
#define BMS_EVENT_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CodeGen_BMS_config.h"
#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Event Logging Configuration */
#define BMS_LOG_MAX_ENTRIES                 (1U)
#define BMS_LOG_ENTRY_SIZE                  (16U)
#define BMS_LOG_FLASH_SECTOR                (FLASH_SECTOR_7)
#define BMS_LOG_STORAGE_BASE_ADDR           (0x081E0000UL)

/* Event Severity Levels */
typedef enum {
    BMS_LOG_LEVEL_INFO     = 0x01U,
    BMS_LOG_LEVEL_WARNING  = 0x02U,
    BMS_LOG_LEVEL_ERROR    = 0x04U,
    BMS_LOG_LEVEL_CRITICAL = 0x08U
} BmsLogLevel_t;

/* Event Type Classification */
typedef enum {
    BMS_EVENT_SYSTEM_INIT,
    BMS_EVENT_VOLTAGE_THRESHOLD,
    BMS_EVENT_CURRENT_THRESHOLD,
    BMS_EVENT_TEMPERATURE_THRESHOLD,
    BMS_EVENT_COMMUNICATION_ERROR,
    BMS_EVENT_HARDWARE_FAULT
} BmsEventType_t;

/* Log Entry Structure */
typedef struct {
    uint32_t    timestamp; //4 bytes
    uint16_t    eventId; //2 bytes
    BmsEventType_t   eventType; // 1 byte
    BmsLogLevel_t    severity;// 1 byte
    int32_t     eventData; // 4 bytes
    uint8_t     reserved[4]; // 1 bytes*4 = 4 bytes
//    uint8_t     reserved1[16];
} BmsLogEntry_t;

/* Logging Management Function Prototypes */
HAL_StatusTypeDef BMS_LogInitialize(void);
HAL_StatusTypeDef BMS_LogEvent(BmsEventType_t eventType, 
                                BmsLogLevel_t severity, 
                                int32_t eventData);
HAL_StatusTypeDef BMS_RetrieveLogEntries(BmsLogEntry_t* logBuffer, 
                                          uint16_t* entryCount);
HAL_StatusTypeDef BMS_ClearLogEntries(void);
HAL_StatusTypeDef BMS_CompressAndArchiveLogs(void);

/* Error Handling Function Prototypes */
void BMS_HandleFatalError(BmsLogEntry_t* errorLog);
bool BMS_IsLogStorageFull(void);

/* Interrupt-Safe Logging Wrapper */
HAL_StatusTypeDef BMS_LogEventSafe(BmsEventType_t eventType, 
                                    BmsLogLevel_t severity, 
                                    int32_t eventData);

#ifdef __cplusplus
}
#endif

#endif /* BMS_EVENT_LOGGING_H */
