/*
 * AstraOS - Network Driver Header
 */

#ifndef _ASTRA_DRIVERS_NET_H
#define _ASTRA_DRIVERS_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pci.h"

extern const struct pci_driver net_pci_driver;

void net_init(void);
int net_probe(const struct pci_device *dev);
int net_device_count(void);
bool net_device_present(int index);
const uint8_t *net_get_mac(int index);
int net_send(int index, const void *frame, uint16_t length);
int net_poll(int index, void *buffer, uint16_t max_length);

#endif /* _ASTRA_DRIVERS_NET_H */
