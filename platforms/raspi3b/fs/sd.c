#include <stdint.h>
#include <stddef.h>

#include "sd.h"
#include "arch.h"

#define PERIPHERAL_BASE 0x3F000000UL
#define EMMC_BASE       (PERIPHERAL_BASE + 0x300000UL)

#define EMMC_ARG2       0x00
#define EMMC_BLKSIZECNT 0x04
#define EMMC_ARG1       0x08
#define EMMC_CMDTM      0x0C
#define EMMC_RESP0      0x10
#define EMMC_RESP1      0x14
#define EMMC_RESP2      0x18
#define EMMC_RESP3      0x1C
#define EMMC_DATA       0x20
#define EMMC_STATUS     0x24
#define EMMC_CONTROL0   0x28
#define EMMC_CONTROL1   0x2C
#define EMMC_INTERRUPT  0x30
#define EMMC_IRPT_MASK  0x34
#define EMMC_IRPT_EN    0x38
#define EMMC_CONTROL2   0x3C
#define EMMC_CAP0       0x40

#define STATUS_CMD_INHIBIT  (1u << 0)
#define STATUS_DATA_INHIBIT (1u << 1)
#define STATUS_CARD_INSERTED (1u << 16)

#define CONTROL1_CLK_INTLEN (1u << 0)
#define CONTROL1_CLK_STABLE (1u << 1)
#define CONTROL1_CLK_EN     (1u << 2)
#define CONTROL1_SRST_HC    (1u << 24)
#define CONTROL1_TIMEOUT_MASK (0xFu << 16)
#define CONTROL1_CLOCK_MASK ((0xFFu << 8) | (3u << 6))

#define INT_CMD_DONE    (1u << 0)
#define INT_DATA_DONE   (1u << 1)
#define INT_WRITE_RDY   (1u << 4)
#define INT_READ_RDY    (1u << 5)
#define INT_ERROR       (1u << 15)
#define INT_ERROR_MASK  0xFFFF0000u

#define CMD_RSPNS_NONE  0x00000000u
#define CMD_RSPNS_136   0x00010000u
#define CMD_RSPNS_48    0x00020000u
#define CMD_RSPNS_48B   0x00030000u
#define CMD_CRCCHK_EN   0x00080000u
#define CMD_IXCHK_EN    0x00100000u
#define CMD_ISDATA      0x00200000u
#define CMD_INDEX(index) ((uint32_t)(index) << 24)

#define TM_DAT_DIR_READ (1u << 4)

#define SD_BLOCK_SIZE 512u
#define SD_TIMEOUT 10000000u

static int sd_ready = 0;
static int sd_high_capacity = 0;
static uint32_t sd_rca = 0;
static uint64_t sd_capacity_sectors = 0;

static inline uint32_t mmio_read(uint32_t offset) {
    return *(volatile uint32_t*)(EMMC_BASE + offset);
}

static inline void mmio_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(EMMC_BASE + offset) = value;
}

static void memory_barrier(void) {
    arch_data_sync_barrier();
    arch_instruction_sync_barrier();
}

static int wait_status_clear(uint32_t mask) {
    for (uint32_t timeout = SD_TIMEOUT; timeout != 0; timeout--) {
        if ((mmio_read(EMMC_STATUS) & mask) == 0) {
            return 0;
        }
    }

    return -1;
}

static int wait_interrupt(uint32_t wanted) {
    for (uint32_t timeout = SD_TIMEOUT; timeout != 0; timeout--) {
        uint32_t interrupt = mmio_read(EMMC_INTERRUPT);

        if (interrupt & (INT_ERROR | INT_ERROR_MASK)) {
            mmio_write(EMMC_INTERRUPT, interrupt);
            return -1;
        }

        if (interrupt & wanted) {
            mmio_write(EMMC_INTERRUPT, wanted);
            return 0;
        }
    }

    return -1;
}

static int reset_host(void) {
    uint32_t control1 = mmio_read(EMMC_CONTROL1);

    mmio_write(EMMC_CONTROL1, control1 | CONTROL1_SRST_HC);

    for (uint32_t timeout = SD_TIMEOUT; timeout != 0; timeout--) {
        if ((mmio_read(EMMC_CONTROL1) & CONTROL1_SRST_HC) == 0) {
            return 0;
        }
    }

    return -1;
}

static uint32_t clock_divider_for(uint32_t target_hz) {
    uint32_t capabilities = mmio_read(EMMC_CAP0);
    uint32_t base_mhz = (capabilities >> 8) & 0xFFu;
    uint32_t base_hz;
    uint32_t divider;

    /* QEMU and Pi 3 hardware both report a usable host clock; 41 MHz is a
       conservative fallback for old firmware that leaves CAP0 at zero. */
    if (base_mhz == 0) {
        base_mhz = 41;
    }

    base_hz = base_mhz * 1000000u;

    if (target_hz >= base_hz) {
        return 0;
    }

    divider = (base_hz + (2u * target_hz) - 1u) / (2u * target_hz);

    if (divider == 0) {
        divider = 1;
    }

    if (divider > 0x3FFu) {
        divider = 0x3FFu;
    }

    return divider;
}

static int set_clock(uint32_t target_hz) {
    uint32_t control1;
    uint32_t divider = clock_divider_for(target_hz);
    uint32_t encoded_divider = ((divider & 0xFFu) << 8) |
                               ((divider & 0x300u) >> 2);

    control1 = mmio_read(EMMC_CONTROL1);
    control1 &= ~(CONTROL1_CLK_EN | CONTROL1_CLOCK_MASK);
    mmio_write(EMMC_CONTROL1, control1);

    control1 |= CONTROL1_CLK_INTLEN | (7u << 16) | encoded_divider;
    mmio_write(EMMC_CONTROL1, control1);

    for (uint32_t timeout = SD_TIMEOUT; timeout != 0; timeout--) {
        if (mmio_read(EMMC_CONTROL1) & CONTROL1_CLK_STABLE) {
            control1 |= CONTROL1_CLK_EN;
            mmio_write(EMMC_CONTROL1, control1);
            return 0;
        }
    }

    return -1;
}

static int send_command(uint32_t command, uint32_t argument) {
    uint32_t inhibit_mask = STATUS_CMD_INHIBIT;

    if (command & CMD_ISDATA) {
        inhibit_mask |= STATUS_DATA_INHIBIT;
    }

    if (wait_status_clear(inhibit_mask) != 0) {
        return -1;
    }

    mmio_write(EMMC_INTERRUPT, 0xFFFFFFFFu);
    mmio_write(EMMC_ARG1, argument);
    memory_barrier();
    mmio_write(EMMC_CMDTM, command);

    return wait_interrupt(INT_CMD_DONE);
}

static int send_app_command(uint32_t command, uint32_t argument) {
    uint32_t cmd55 = CMD_INDEX(55) | CMD_RSPNS_48 | CMD_CRCCHK_EN | CMD_IXCHK_EN;

    if (send_command(cmd55, sd_rca << 16) != 0) {
        return -1;
    }

    return send_command(command, argument);
}

static int read_csd_capacity(void) {
    uint32_t resp0;
    uint32_t resp1;
    uint32_t resp2;
    uint32_t resp3;
    uint32_t csd_structure;
    uint64_t sectors = 0;

    /*
     * CMD9 (SEND_CSD) is valid while the card is in the standby state.
     * Call this after CMD3 assigned the RCA, but before CMD7 selects the
     * card and moves it to transfer state.
     *
     * For an R2 response on BCM2835/SDHCI, RESP3 contains the highest word
     * and RESP0 the lowest. The controller strips the command index and CRC,
     * so CSD fields appear shifted down by eight bits.
     */
    if (send_command(CMD_INDEX(9) | CMD_RSPNS_136 | CMD_CRCCHK_EN,
                     sd_rca << 16) != 0) {
        return -1;
    }

    resp0 = mmio_read(EMMC_RESP0);
    resp1 = mmio_read(EMMC_RESP1);
    resp2 = mmio_read(EMMC_RESP2);
    resp3 = mmio_read(EMMC_RESP3);

    (void)resp0;

    /* CSD_STRUCTURE is original bits 127:126, shifted to RESP3 bits 23:22. */
    csd_structure = (resp3 >> 22) & 0x3u;

    if (csd_structure == 1u) {
        /* SDHC / SDXC: C_SIZE is 22 bits in RESP1 bits 29:8. */
        uint32_t c_size = (resp1 >> 8) & 0x3FFFFFu;
        sectors = ((uint64_t)c_size + 1u) * 1024u;
    } else if (csd_structure == 0u) {
        /*
         * SDSC: capacity = (C_SIZE + 1) * 2^(C_SIZE_MULT + 2) *
         *            2^READ_BL_LEN bytes.
         * Convert the result to the 512-byte sectors used by block.c/FatFs.
         */
        uint32_t read_bl_len = (resp2 >> 8) & 0xFu;
        uint32_t c_size = ((resp2 & 0x3u) << 10) |
                          ((resp1 >> 22) & 0x3FFu);
        uint32_t c_size_mult = (resp1 >> 7) & 0x7u;
        uint32_t capacity_shift = read_bl_len + c_size_mult + 2u;
        uint64_t capacity_bytes;

        if (capacity_shift >= 63u) {
            return -1;
        }

        capacity_bytes = ((uint64_t)c_size + 1u) << capacity_shift;
        sectors = capacity_bytes / SD_BLOCK_SIZE;
    } else {
        return -1;
    }

    if (sectors == 0) {
        return -1;
    }

    sd_capacity_sectors = sectors;
    return 0;
}
int sd_init(void) {
    uint32_t response;
    uint32_t acmd41 = CMD_INDEX(41) | CMD_RSPNS_48;

    if (sd_ready) {
        return 0;
    }

    if ((mmio_read(EMMC_STATUS) & STATUS_CARD_INSERTED) == 0) {
        return -1;
    }

    if (reset_host() != 0) {
        return -1;
    }

    mmio_write(EMMC_CONTROL0, 0);
    mmio_write(EMMC_IRPT_EN, 0xFFFFFFFFu);
    mmio_write(EMMC_IRPT_MASK, 0xFFFFFFFFu);
    mmio_write(EMMC_INTERRUPT, 0xFFFFFFFFu);

    if (set_clock(400000u) != 0) {
        return -1;
    }

    if (send_command(CMD_INDEX(0) | CMD_RSPNS_NONE, 0) != 0) {
        return -1;
    }

    /* CMD8 distinguishes SD v2 cards and sets the requested voltage/check pattern. */
    if (send_command(CMD_INDEX(8) | CMD_RSPNS_48 | CMD_CRCCHK_EN | CMD_IXCHK_EN, 0x1AAu) != 0) {
        return -1;
    }

    response = mmio_read(EMMC_RESP0);
    if ((response & 0xFFFu) != 0x1AAu) {
        return -1;
    }

    for (uint32_t attempts = 0; attempts < 1000u; attempts++) {
        if (send_app_command(acmd41, 0x40FF8000u) != 0) {
            return -1;
        }

        response = mmio_read(EMMC_RESP0);
        if (response & 0x80000000u) {
            sd_high_capacity = (response & 0x40000000u) != 0;
            break;
        }
    }

    if ((response & 0x80000000u) == 0) {
        return -1;
    }

    if (send_command(CMD_INDEX(2) | CMD_RSPNS_136 | CMD_CRCCHK_EN, 0) != 0) {
        return -1;
    }

    if (send_command(CMD_INDEX(3) | CMD_RSPNS_48 | CMD_CRCCHK_EN | CMD_IXCHK_EN, 0) != 0) {
        return -1;
    }

    sd_rca = mmio_read(EMMC_RESP0) >> 16;
    if (sd_rca == 0) {
        return -1;
    }

    /* CMD9 must happen before CMD7: CMD7 moves the card into transfer state. */
    if (read_csd_capacity() != 0) {
        return -1;
    }

    if (send_command(CMD_INDEX(7) | CMD_RSPNS_48B | CMD_CRCCHK_EN | CMD_IXCHK_EN,
                     sd_rca << 16) != 0) {
        return -1;
    }

    if (wait_status_clear(STATUS_DATA_INHIBIT) != 0) {
        return -1;
    }

    if (!sd_high_capacity) {
        if (send_command(CMD_INDEX(16) | CMD_RSPNS_48 | CMD_CRCCHK_EN | CMD_IXCHK_EN,
                         SD_BLOCK_SIZE) != 0) {
            return -1;
        }
    }

    if (set_clock(25000000u) != 0) {
        return -1;
    }

    /* Do not report a usable disk until its capacity was read successfully. */
    sd_ready = 1;
    return 0;
}

static uint32_t sector_argument(uint64_t sector) {
    if (sd_high_capacity) {
        return (uint32_t)sector;
    }

    return (uint32_t)(sector * SD_BLOCK_SIZE);
}

int sd_read_sector(uint64_t sector, void* buffer) {
    uint32_t* words = (uint32_t*)buffer;
    uint32_t command = CMD_INDEX(17) | CMD_RSPNS_48 | CMD_CRCCHK_EN |
                       CMD_IXCHK_EN | CMD_ISDATA | TM_DAT_DIR_READ;

    if (sd_init() != 0 || buffer == NULL || sector > 0xFFFFFFFFu) {
        return -1;
    }

    mmio_write(EMMC_BLKSIZECNT, SD_BLOCK_SIZE);

    if (send_command(command, sector_argument(sector)) != 0) {
        return -1;
    }

    if (wait_interrupt(INT_READ_RDY) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < SD_BLOCK_SIZE / sizeof(uint32_t); i++) {
        words[i] = mmio_read(EMMC_DATA);
    }

    return wait_interrupt(INT_DATA_DONE);
}

int sd_write_sector(uint64_t sector, const void* buffer) {
    const uint32_t* words = (const uint32_t*)buffer;
    uint32_t command = CMD_INDEX(24) | CMD_RSPNS_48 | CMD_CRCCHK_EN |
                       CMD_IXCHK_EN | CMD_ISDATA;

    if (sd_init() != 0 || buffer == NULL || sector > 0xFFFFFFFFu) {
        return -1;
    }

    mmio_write(EMMC_BLKSIZECNT, SD_BLOCK_SIZE);

    if (send_command(command, sector_argument(sector)) != 0) {
        return -1;
    }

    if (wait_interrupt(INT_WRITE_RDY) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < SD_BLOCK_SIZE / sizeof(uint32_t); i++) {
        mmio_write(EMMC_DATA, words[i]);
    }

    return wait_interrupt(INT_DATA_DONE);
}

uint64_t sd_sector_count(void) {
    if (sd_init() != 0) {
        return 0;
    }

    return sd_capacity_sectors;
}
