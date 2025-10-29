/**
 * @file CodeGen_Logging.c
 * @brief Battery Management System Event Logging Implementation
 */

#include "CodeGen_Logging.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* Private Typedefs */
typedef struct {
	BmsLogEntry_t   logBuffer;
//    volatile uint16_t currentIndex;
//    volatile bool    isStorageFull;
} BmsLogManager_t;

/* Static Variables */
static BmsLogManager_t s_LogManager = {0};
static FLASH_EraseInitTypeDef s_FlashEraseConfig = {0};

/* Critical Section Semaphore */
static volatile bool s_LogCriticalSectionLock = false;

/* Private Function Prototypes */
static HAL_StatusTypeDef prv_AcquireLogLock(void);
static void prv_ReleaseLogLock(void);
static HAL_StatusTypeDef prv_WriteLogToFlash(void);
//static bool prv_IsValidLogEntry(const BmsLogEntry_t* entry);

/**
 * @brief Initialize BMS logging system
 * @return HAL Status
 */
HAL_StatusTypeDef BMS_LogInitialize(void) {
    /* Reset Log Manager */
    memset(&s_LogManager, 0, sizeof(BmsLogManager_t));
    
    /* Configure Flash Erase Settings */
    s_FlashEraseConfig.TypeErase     = FLASH_TYPEERASE_SECTORS;
    s_FlashEraseConfig.Banks		 = FLASH_BANK_2;
    s_FlashEraseConfig.Sector        = BMS_LOG_FLASH_SECTOR;
    s_FlashEraseConfig.NbSectors     = 1;
    s_FlashEraseConfig.VoltageRange  = FLASH_VOLTAGE_RANGE_2;

    /* Unlock Flash for Writing */
    HAL_FLASH_Unlock();
    
    return HAL_OK;
}

/**
 * @brief Log an event with interrupt-safe mechanism
 * @param eventType Event classification
 * @param severity Log severity level
 * @param eventData Contextual event data
 * @return HAL Status
 */
HAL_StatusTypeDef BMS_LogEventSafe(BmsEventType_t eventType, 
                                   BmsLogLevel_t severity, 
                                   int32_t eventData) {
    HAL_StatusTypeDef status = HAL_ERROR;
    
    if (prv_AcquireLogLock() == HAL_OK) {
        /* Check storage capacity */
        if (!BMS_IsLogStorageFull()) {
            BmsLogEntry_t newEntry = {
                .timestamp   = HAL_GetTick(),
                .eventType   = eventType,
                .severity    = severity,
                .eventData   = eventData
            };

            /* Store log entry */
            s_LogManager.logBuffer = newEntry;
            
            /* Periodic flash write or on buffer full */
            status = prv_WriteLogToFlash();
            
            status = HAL_OK;
        }
        
        prv_ReleaseLogLock();
    }
    
    return status;
}

/**
 * @brief Write log entries to flash memory
 * @return HAL Status
 */
//static HAL_StatusTypeDef prv_WriteLogToFlash(void) {
//    HAL_StatusTypeDef status = HAL_ERROR;
//    uint32_t flashAddress = BMS_LOG_STORAGE_BASE_ADDR;
//
//    /* Erase sector before writing */
//    uint32_t sectorError = 0;
//    if (HAL_FLASHEx_Erase(&s_FlashEraseConfig, &sectorError) == HAL_OK) {
//        for (uint16_t i = 0; i < s_LogManager.currentIndex; i++) {
//            if (prv_IsValidLogEntry(&s_LogManager.logBuffer[i])) {
//                status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
//                                           flashAddress,
//                                           (uint32_t)&s_LogManager.logBuffer[i]);
//                uint8_t s=sizeof(BmsLogEntry_t);
//                flashAddress += 32;
//            }
//        }
//    }
//
//    return status;
//}

static HAL_StatusTypeDef prv_WriteLogToFlash(void) {
    HAL_StatusTypeDef status = HAL_ERROR;
    uint32_t flashAddress = BMS_LOG_STORAGE_BASE_ADDR;
    
    /* Erase sector before writing */
    uint32_t sectorError;
//    uint8_t size=sizeof(s_LogManager.logBuffer);

    if (HAL_FLASHEx_Erase(&s_FlashEraseConfig, &sectorError) == HAL_OK) {
                status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, 
                                           flashAddress, 
                                           (uint32_t)&s_LogManager.logBuffer);
//                uint8_t s=sizeof(BmsLogEntry_t);

    }
    return status;
}

/**
 * @brief Check if log entry is valid
 * @param entry Log entry pointer
 * @return Validation result
 */
//static bool prv_IsValidLogEntry(const BmsLogEntry_t* entry) {
//    return (entry->severity <= BMS_LOG_LEVEL_CRITICAL) &&
//           (entry->eventType < BMS_EVENT_HARDWARE_FAULT);
//}

/**
 * @brief Acquire log critical section lock
 * @return HAL Status
 */
static HAL_StatusTypeDef prv_AcquireLogLock(void) {
    uint32_t timeout = HAL_GetTick() + BMS_RESPONSE_TIMEOUT_MS;
    
    while (s_LogCriticalSectionLock && (HAL_GetTick() < timeout)) {
        /* Wait or timeout */
    }
    
    if (!s_LogCriticalSectionLock) {
        s_LogCriticalSectionLock = true;
        return HAL_OK;
    }
    
    return HAL_TIMEOUT;
}

/**
 * @brief Release log critical section lock
 */
static void prv_ReleaseLogLock(void) {
    s_LogCriticalSectionLock = false;
}

/**
 * @brief Check if log storage is full
 * @return Storage status
 */
bool BMS_IsLogStorageFull(void) {
    return 1;
}
