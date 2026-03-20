#ifndef DISK_H
#define DISK_H

#include "../types.h"

void disk_init();
uint8_t disk_read_sector(uint32_t lba, uint8_t *buffer);
uint8_t disk_write_sector(uint32_t lba, const uint8_t *buffer);
int disk_detect();

#endif

