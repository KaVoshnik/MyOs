#ifndef _MYOS_PCI_H
#define _MYOS_PCI_H

#include <stdint.h>

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
} pci_device_info_t;

/* Сканирует PCI и печатает найденные устройства в терминал. */
void pci_scan_and_print(void);

/* Низкоуровневый доступ к PCI config space (I/O ports 0xCF8/0xCFC). */
uint32_t pci_cfg_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_cfg_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

/* Утилита: найти первое устройство по vendor/device. Возвращает 1, если найдено. */
int pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_info_t *out);

#endif /* _MYOS_PCI_H */

