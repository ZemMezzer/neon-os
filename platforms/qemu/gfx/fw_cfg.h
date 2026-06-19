#pragma once

#include <stdint.h>

int fw_cfg_find_file(const char* name, uint16_t* selector_out, uint32_t* size_out);
int fw_cfg_dma_write(uint16_t selector, const void* data, uint32_t size);