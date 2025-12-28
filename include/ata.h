#ifndef _MYOS_ATA_H
#define _MYOS_ATA_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ATA_ERR_NONE = 0,
    ATA_ERR_TIMEOUT = -1,
    ATA_ERR_DEVICE = -2,
    ATA_ERR_INVALID_PARAM = -3,
    ATA_ERR_NOT_AVAILABLE = -4,
    ATA_ERR_WRITE_PROTECTED = -5,
    ATA_ERR_BAD_SECTOR = -6,
    ATA_ERR_ABORT = -7
} ata_error_t;

typedef struct {
    ata_error_t error_code;
    uint8_t status_register;
    uint8_t error_register;
    uint32_t lba;
    uint16_t sector_count;
} ata_error_info_t;

void ata_init(void);
int ata_is_available(void);
int ata_read_sectors(uint32_t lba, uint16_t sector_count, void *buffer);
int ata_write_sectors(uint32_t lba, uint16_t sector_count, const void *buffer);
uint64_t ata_get_total_sectors(void);
const char *ata_get_model(void);
const char *ata_get_serial(void);
const char *ata_get_firmware(void);
int ata_get_last_error(void);
ata_error_t ata_get_last_error_code(void);
const ata_error_info_t *ata_get_last_error_info(void);
const char *ata_error_string(ata_error_t error);

#endif /* _MYOS_ATA_H */


