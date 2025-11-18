#ifndef _MYOS_ATA_H
#define _MYOS_ATA_H

#include <stddef.h>
#include <stdint.h>

void ata_init(void);
int ata_is_available(void);
int ata_read_sectors(uint32_t lba, uint16_t sector_count, void *buffer);
int ata_write_sectors(uint32_t lba, uint16_t sector_count, const void *buffer);

#endif /* _MYOS_ATA_H */


