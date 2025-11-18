#include <ata.h>
#include <io.h>

#define ATA_PRIMARY_IO         0x1F0
#define ATA_PRIMARY_CTRL       0x3F6

#define ATA_REG_DATA           (ATA_PRIMARY_IO + 0)
#define ATA_REG_ERROR          (ATA_PRIMARY_IO + 1)
#define ATA_REG_SECCOUNT0      (ATA_PRIMARY_IO + 2)
#define ATA_REG_LBA0           (ATA_PRIMARY_IO + 3)
#define ATA_REG_LBA1           (ATA_PRIMARY_IO + 4)
#define ATA_REG_LBA2           (ATA_PRIMARY_IO + 5)
#define ATA_REG_HDDEVSEL       (ATA_PRIMARY_IO + 6)
#define ATA_REG_COMMAND        (ATA_PRIMARY_IO + 7)
#define ATA_REG_STATUS         (ATA_PRIMARY_IO + 7)

#define ATA_REG_CONTROL        (ATA_PRIMARY_CTRL)

#define ATA_CMD_READ_PIO       0x20
#define ATA_CMD_WRITE_PIO      0x30
#define ATA_CMD_CACHE_FLUSH    0xE7
#define ATA_CMD_IDENTIFY       0xEC

#define ATA_SR_ERR             0x01
#define ATA_SR_DRQ             0x08
#define ATA_SR_DF              0x20
#define ATA_SR_DRDY            0x40
#define ATA_SR_BSY             0x80

static int ata_present = 0;

static int ata_wait_busy_clear(void) {
    uint8_t status;
    do {
        status = inb(ATA_REG_STATUS);
    } while (status & ATA_SR_BSY);
    if (status & (ATA_SR_ERR | ATA_SR_DF)) {
        return -1;
    }
    return 0;
}

static int ata_wait_drq(void) {
    uint8_t status;
    do {
        status = inb(ATA_REG_STATUS);
        if (status & (ATA_SR_ERR | ATA_SR_DF)) {
            return -1;
        }
    } while (!(status & ATA_SR_DRQ));
    return 0;
}

static void ata_select_drive(uint32_t lba) {
    outb(ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
}

void ata_init(void) {
    ata_present = 0;
    outb(ATA_REG_CONTROL, 0x00);
    ata_select_drive(0);
    outb(ATA_REG_SECCOUNT0, 0);
    outb(ATA_REG_LBA0, 0);
    outb(ATA_REG_LBA1, 0);
    outb(ATA_REG_LBA2, 0);
    outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_REG_STATUS);
    if (status == 0) {
        return;
    }

    while (status & ATA_SR_BSY) {
        status = inb(ATA_REG_STATUS);
    }

    uint8_t lba1 = inb(ATA_REG_LBA1);
    uint8_t lba2 = inb(ATA_REG_LBA2);
    if (lba1 != 0 || lba2 != 0) {
        return; /* Not ATA */
    }

    while (!(status & ATA_SR_DRQ) && !(status & ATA_SR_ERR)) {
        status = inb(ATA_REG_STATUS);
    }

    if (status & ATA_SR_ERR) {
        return;
    }

    uint16_t buffer[256];
    insw(ATA_REG_DATA, buffer, 256);
    (void)buffer;
    ata_present = 1;
}

int ata_is_available(void) {
    return ata_present;
}

static int ata_transfer(uint32_t lba, uint16_t sector_count, void *buffer, int write) {
    if (!ata_present || sector_count == 0 || buffer == NULL) {
        return -1;
    }

    uint32_t remaining = sector_count;
    uint8_t *byte_buffer = (uint8_t *)buffer;

    while (remaining > 0) {
        uint16_t chunk = (remaining > 256) ? 256 : (uint16_t)remaining;
        uint8_t sector_value = (chunk == 256) ? 0 : (uint8_t)chunk;

        ata_select_drive(lba);
        outb(ATA_REG_SECCOUNT0, sector_value);
        outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
        outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
        outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
        outb(ATA_REG_COMMAND, write ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO);

        uint16_t sectors_to_process = (chunk == 256) ? 256 : chunk;
        for (uint16_t i = 0; i < sectors_to_process; ++i) {
            if (write) {
                if (ata_wait_busy_clear() != 0 || ata_wait_drq() != 0) {
                    return -1;
                }
                outsw(ATA_REG_DATA, byte_buffer, 256);
            } else {
                if (ata_wait_busy_clear() != 0 || ata_wait_drq() != 0) {
                    return -1;
                }
                insw(ATA_REG_DATA, byte_buffer, 256);
            }
            byte_buffer += 512;
        }

        if (write) {
            outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
            ata_wait_busy_clear();
        }

        lba += sectors_to_process;
        remaining -= sectors_to_process;
    }

    return 0;
}

int ata_read_sectors(uint32_t lba, uint16_t sector_count, void *buffer) {
    return ata_transfer(lba, sector_count, buffer, 0);
}

int ata_write_sectors(uint32_t lba, uint16_t sector_count, const void *buffer) {
    return ata_transfer(lba, sector_count, (void *)buffer, 1);
}


