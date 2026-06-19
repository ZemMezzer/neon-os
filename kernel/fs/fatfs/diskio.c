#include <stdint.h>

#include "diskio.h"
#include "block.h"

DSTATUS disk_initialize(BYTE pdrv) {
    (void)pdrv;

    if (block_init() != 0) {
        return STA_NOINIT;
    }

    return 0;
}

DSTATUS disk_status(BYTE pdrv) {
    (void)pdrv;

    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    (void)pdrv;

    if (block_read((uint64_t)sector, buff, (uint32_t)count) != 0) {
        return RES_ERROR;
    }

    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    (void)pdrv;

    if (block_write((uint64_t)sector, buff, (uint32_t)count) != 0) {
        return RES_ERROR;
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    (void)pdrv;

    if (cmd == CTRL_SYNC) {
        return RES_OK;
    }

    if (cmd == GET_SECTOR_COUNT) {
        if (!buff) {
            return RES_PARERR;
        }

        *(LBA_t*)buff = (LBA_t)block_sector_count();

        return RES_OK;
    }

    if (cmd == GET_SECTOR_SIZE) {
        if (!buff) {
            return RES_PARERR;
        }

        *(WORD*)buff = BLOCK_SECTOR_SIZE;

        return RES_OK;
    }

    if (cmd == GET_BLOCK_SIZE) {
        if (!buff) {
            return RES_PARERR;
        }

        *(DWORD*)buff = 1;

        return RES_OK;
    }

    return RES_PARERR;
}

DWORD get_fattime(void) {
    return ((DWORD)(2026 - 1980) << 25)
         | ((DWORD)1 << 21)
         | ((DWORD)1 << 16)
         | ((DWORD)0 << 11)
         | ((DWORD)0 << 5)
         | ((DWORD)0 >> 1);
}