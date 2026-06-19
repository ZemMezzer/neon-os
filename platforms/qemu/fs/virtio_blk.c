#include <stdint.h>
#include <stddef.h>

#include "virtio_blk.h"

#define VIRTIO_MMIO_BASE_START  0x0A000000UL
#define VIRTIO_MMIO_DEVICE_SIZE 0x00000200UL
#define VIRTIO_MMIO_DEVICE_COUNT 32

#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00C

#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024

#define VIRTIO_MMIO_GUEST_PAGE_SIZE     0x028

#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_ALIGN         0x03C
#define VIRTIO_MMIO_QUEUE_PFN           0x040
#define VIRTIO_MMIO_QUEUE_READY         0x044

#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070

#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW     0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH    0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW      0x0A0
#define VIRTIO_MMIO_QUEUE_USED_HIGH     0x0A4

#define VIRTIO_MMIO_CONFIG              0x100

#define VIRTIO_MAGIC                    0x74726976
#define VIRTIO_DEVICE_ID_BLOCK          2

#define VIRTIO_STATUS_ACKNOWLEDGE       1
#define VIRTIO_STATUS_DRIVER            2
#define VIRTIO_STATUS_DRIVER_OK         4
#define VIRTIO_STATUS_FEATURES_OK       8
#define VIRTIO_STATUS_FAILED            128

#define VIRTIO_F_VERSION_1              32

#define VIRTQ_DESC_F_NEXT               1
#define VIRTQ_DESC_F_WRITE              2

#define VIRTIO_BLK_T_IN                 0
#define VIRTIO_BLK_T_OUT                1

#define VIRTIO_BLK_S_OK                 0

#define VIRTIO_BLK_QUEUE_SIZE           8
#define VIRTIO_BLK_QUEUE_ALIGN          4096
#define VIRTIO_BLK_QUEUE_MEMORY_SIZE    8192
#define VIRTIO_BLK_SECTOR_SIZE          512

#define VIRTIO_BLK_CONFIG_CAPACITY      0x00

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_BLK_QUEUE_SIZE];
};

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
};

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[VIRTIO_BLK_QUEUE_SIZE];
};

struct virtio_blk_req_header {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

static volatile uint8_t* virtio_blk_mmio = 0;

static uint32_t virtio_blk_version = 0;
static uint64_t virtio_blk_capacity = 0;

static uint8_t virtio_blk_queue_memory[VIRTIO_BLK_QUEUE_MEMORY_SIZE]
    __attribute__((aligned(VIRTIO_BLK_QUEUE_ALIGN)));

static struct virtq_desc* virtio_blk_desc = 0;
static struct virtq_avail* virtio_blk_avail = 0;
static volatile struct virtq_used* virtio_blk_used = 0;

static struct virtio_blk_req_header virtio_blk_req
    __attribute__((aligned(16)));

static volatile uint8_t virtio_blk_status
    __attribute__((aligned(16)));

static inline void memory_barrier(void) {
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static uint32_t align_up_u32(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint32_t mmio_read32(uint32_t offset) {
    volatile uint32_t* ptr = (volatile uint32_t*)(virtio_blk_mmio + offset);
    return *ptr;
}

static void mmio_write32(uint32_t offset, uint32_t value) {
    volatile uint32_t* ptr = (volatile uint32_t*)(virtio_blk_mmio + offset);
    *ptr = value;
    memory_barrier();
}

static uint64_t mmio_read64_config(uint32_t offset) {
    volatile uint32_t* low_ptr;
    volatile uint32_t* high_ptr;

    uint32_t low;
    uint32_t high;

    low_ptr = (volatile uint32_t*)(virtio_blk_mmio + VIRTIO_MMIO_CONFIG + offset);
    high_ptr = (volatile uint32_t*)(virtio_blk_mmio + VIRTIO_MMIO_CONFIG + offset + 4);

    low = *low_ptr;
    high = *high_ptr;

    return ((uint64_t)high << 32) | low;
}

static void zero_queue_memory(void) {
    volatile uint8_t* ptr = (volatile uint8_t*)virtio_blk_queue_memory;


    for (uint32_t i = 0; i < VIRTIO_BLK_QUEUE_MEMORY_SIZE; i++) {
        ptr[i] = 0;
    }

}

static int virtio_blk_find_device(void) {

    for (uint32_t i = 0; i < VIRTIO_MMIO_DEVICE_COUNT; i++) {
        uintptr_t base = VIRTIO_MMIO_BASE_START + i * VIRTIO_MMIO_DEVICE_SIZE;

        virtio_blk_mmio = (volatile uint8_t*)base;

        uint32_t magic = mmio_read32(VIRTIO_MMIO_MAGIC_VALUE);

        if (magic != VIRTIO_MAGIC) {
            continue;
        }

        uint32_t device_id = mmio_read32(VIRTIO_MMIO_DEVICE_ID);

        if (device_id == VIRTIO_DEVICE_ID_BLOCK) {
            virtio_blk_version = mmio_read32(VIRTIO_MMIO_VERSION);


            if (virtio_blk_version == 1) {
            } else if (virtio_blk_version == 2) {
            } else {
            }

            return 0;
        }
    }


    virtio_blk_mmio = 0;
    return -1;
}

static int virtio_blk_setup_queue_legacy(void) {
    uint32_t queue_num_max;
    uint32_t desc_offset;
    uint32_t avail_offset;
    uint32_t used_offset;
    uintptr_t queue_addr;
    uint32_t queue_pfn;


    mmio_write32(VIRTIO_MMIO_QUEUE_SEL, 0);


    queue_num_max = mmio_read32(VIRTIO_MMIO_QUEUE_NUM_MAX);


    if (queue_num_max == 0) {
        return -1;
    }

    if (queue_num_max < VIRTIO_BLK_QUEUE_SIZE) {
        return -1;
    }

    zero_queue_memory();

    desc_offset = 0;

    avail_offset = desc_offset
        + sizeof(struct virtq_desc) * VIRTIO_BLK_QUEUE_SIZE;

    used_offset = align_up_u32(
        avail_offset
        + sizeof(uint16_t) * 2
        + sizeof(uint16_t) * VIRTIO_BLK_QUEUE_SIZE,
        VIRTIO_BLK_QUEUE_ALIGN
    );

    if (used_offset + sizeof(struct virtq_used) > VIRTIO_BLK_QUEUE_MEMORY_SIZE) {
        return -1;
    }

    virtio_blk_desc = (struct virtq_desc*)(virtio_blk_queue_memory + desc_offset);
    virtio_blk_avail = (struct virtq_avail*)(virtio_blk_queue_memory + avail_offset);
    virtio_blk_used = (volatile struct virtq_used*)(virtio_blk_queue_memory + used_offset);


    mmio_write32(VIRTIO_MMIO_GUEST_PAGE_SIZE, VIRTIO_BLK_QUEUE_ALIGN);


    mmio_write32(VIRTIO_MMIO_QUEUE_NUM, VIRTIO_BLK_QUEUE_SIZE);


    mmio_write32(VIRTIO_MMIO_QUEUE_ALIGN, VIRTIO_BLK_QUEUE_ALIGN);

    queue_addr = (uintptr_t)virtio_blk_queue_memory;
    queue_pfn = (uint32_t)(queue_addr / VIRTIO_BLK_QUEUE_ALIGN);


    mmio_write32(VIRTIO_MMIO_QUEUE_PFN, queue_pfn);


    return 0;
}

static int virtio_blk_setup_queue_modern(void) {
    uint32_t queue_num_max;
    uint32_t desc_offset;
    uint32_t avail_offset;
    uint32_t used_offset;


    mmio_write32(VIRTIO_MMIO_QUEUE_SEL, 0);

    queue_num_max = mmio_read32(VIRTIO_MMIO_QUEUE_NUM_MAX);

    if (queue_num_max == 0) {
        return -1;
    }

    if (queue_num_max < VIRTIO_BLK_QUEUE_SIZE) {
        return -1;
    }

    zero_queue_memory();

    desc_offset = 0;

    avail_offset = desc_offset
        + sizeof(struct virtq_desc) * VIRTIO_BLK_QUEUE_SIZE;

    used_offset = align_up_u32(
        avail_offset
        + sizeof(uint16_t) * 2
        + sizeof(uint16_t) * VIRTIO_BLK_QUEUE_SIZE,
        VIRTIO_BLK_QUEUE_ALIGN
    );

    virtio_blk_desc = (struct virtq_desc*)(virtio_blk_queue_memory + desc_offset);
    virtio_blk_avail = (struct virtq_avail*)(virtio_blk_queue_memory + avail_offset);
    virtio_blk_used = (volatile struct virtq_used*)(virtio_blk_queue_memory + used_offset);

    mmio_write32(VIRTIO_MMIO_QUEUE_NUM, VIRTIO_BLK_QUEUE_SIZE);

    mmio_write32(
        VIRTIO_MMIO_QUEUE_DESC_LOW,
        (uint32_t)((uint64_t)(uintptr_t)virtio_blk_desc)
    );

    mmio_write32(
        VIRTIO_MMIO_QUEUE_DESC_HIGH,
        (uint32_t)(((uint64_t)(uintptr_t)virtio_blk_desc) >> 32)
    );

    mmio_write32(
        VIRTIO_MMIO_QUEUE_AVAIL_LOW,
        (uint32_t)((uint64_t)(uintptr_t)virtio_blk_avail)
    );

    mmio_write32(
        VIRTIO_MMIO_QUEUE_AVAIL_HIGH,
        (uint32_t)(((uint64_t)(uintptr_t)virtio_blk_avail) >> 32)
    );

    mmio_write32(
        VIRTIO_MMIO_QUEUE_USED_LOW,
        (uint32_t)((uint64_t)(uintptr_t)virtio_blk_used)
    );

    mmio_write32(
        VIRTIO_MMIO_QUEUE_USED_HIGH,
        (uint32_t)(((uint64_t)(uintptr_t)virtio_blk_used) >> 32)
    );

    mmio_write32(VIRTIO_MMIO_QUEUE_READY, 1);


    return 0;
}

static int virtio_blk_setup_queue(void) {
    if (virtio_blk_version == 1) {
        return virtio_blk_setup_queue_legacy();
    }

    if (virtio_blk_version == 2) {
        return virtio_blk_setup_queue_modern();
    }

    return -1;
}

int virtio_blk_init(void) {
    uint32_t status;
    uint32_t device_features_low;
    uint32_t device_features_high;
    uint32_t driver_features_low;
    uint32_t driver_features_high;


    if (virtio_blk_mmio != 0 && virtio_blk_desc != 0) {
        return 0;
    }


    if (virtio_blk_find_device() != 0) {
        return -1;
    }


    mmio_write32(VIRTIO_MMIO_STATUS, 0);

    status = VIRTIO_STATUS_ACKNOWLEDGE;


    mmio_write32(VIRTIO_MMIO_STATUS, status);

    status |= VIRTIO_STATUS_DRIVER;


    mmio_write32(VIRTIO_MMIO_STATUS, status);

    driver_features_low = 0;
    driver_features_high = 0;

    if (virtio_blk_version == 2) {

        mmio_write32(VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
        device_features_low = mmio_read32(VIRTIO_MMIO_DEVICE_FEATURES);


        mmio_write32(VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
        device_features_high = mmio_read32(VIRTIO_MMIO_DEVICE_FEATURES);

        (void)device_features_low;

        if (device_features_high & (1u << (VIRTIO_F_VERSION_1 - 32))) {
            driver_features_high |= (1u << (VIRTIO_F_VERSION_1 - 32));
        } else {
            mmio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
            return -1;
        }

        mmio_write32(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
        mmio_write32(VIRTIO_MMIO_DRIVER_FEATURES, driver_features_low);

        mmio_write32(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
        mmio_write32(VIRTIO_MMIO_DRIVER_FEATURES, driver_features_high);

        status |= VIRTIO_STATUS_FEATURES_OK;


        mmio_write32(VIRTIO_MMIO_STATUS, status);

        if ((mmio_read32(VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0) {
            mmio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
            return -1;
        }
    } else {

        mmio_write32(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
        mmio_write32(VIRTIO_MMIO_DRIVER_FEATURES, 0);
    }


    if (virtio_blk_setup_queue() != 0) {
        mmio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }


    virtio_blk_capacity = mmio_read64_config(VIRTIO_BLK_CONFIG_CAPACITY);


    status |= VIRTIO_STATUS_DRIVER_OK;


    mmio_write32(VIRTIO_MMIO_STATUS, status);


    return 0;
}

static int virtio_blk_request(uint32_t type, uint64_t sector, void* buffer) {
    uint16_t avail_index;
    uint16_t used_index_before;
    uint32_t timeout;


    if (virtio_blk_init() != 0) {
        return -1;
    }

    virtio_blk_status = 0xFF;

    virtio_blk_req.type = type;
    virtio_blk_req.reserved = 0;
    virtio_blk_req.sector = sector;

    virtio_blk_desc[0].addr = (uint64_t)(uintptr_t)&virtio_blk_req;
    virtio_blk_desc[0].len = sizeof(virtio_blk_req);
    virtio_blk_desc[0].flags = VIRTQ_DESC_F_NEXT;
    virtio_blk_desc[0].next = 1;

    virtio_blk_desc[1].addr = (uint64_t)(uintptr_t)buffer;
    virtio_blk_desc[1].len = VIRTIO_BLK_SECTOR_SIZE;

    if (type == VIRTIO_BLK_T_IN) {
        virtio_blk_desc[1].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    } else {
        virtio_blk_desc[1].flags = VIRTQ_DESC_F_NEXT;
    }

    virtio_blk_desc[1].next = 2;

    virtio_blk_desc[2].addr = (uint64_t)(uintptr_t)&virtio_blk_status;
    virtio_blk_desc[2].len = 1;
    virtio_blk_desc[2].flags = VIRTQ_DESC_F_WRITE;
    virtio_blk_desc[2].next = 0;

    used_index_before = virtio_blk_used->idx;

    avail_index = virtio_blk_avail->idx % VIRTIO_BLK_QUEUE_SIZE;
    virtio_blk_avail->ring[avail_index] = 0;

    memory_barrier();

    virtio_blk_avail->idx++;

    memory_barrier();


    mmio_write32(VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    timeout = 100000000;


    while (virtio_blk_used->idx == used_index_before) {
        if (timeout == 0) {
            return -1;
        }

        timeout--;
    }

    memory_barrier();


    if (virtio_blk_status != VIRTIO_BLK_S_OK) {
        return -1;
    }


    return 0;
}

int virtio_blk_read_sector(uint64_t sector, void* buffer) {
    return virtio_blk_request(VIRTIO_BLK_T_IN, sector, buffer);
}

int virtio_blk_write_sector(uint64_t sector, const void* buffer) {
    return virtio_blk_request(VIRTIO_BLK_T_OUT, sector, (void*)buffer);
}

uint64_t virtio_blk_sector_count(void) {

    if (virtio_blk_init() != 0) {
        return 0;
    }


    return virtio_blk_capacity;
}
