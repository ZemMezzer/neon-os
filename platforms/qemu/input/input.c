#include <stdint.h>

#include "input.h"
#include "virtio_mmio.h"

#define KEY_CAPSLOCK 58
#define VIRTIO_INPUT_DEVICE_ID 18

#define VIRTIO_MMIO_MAGIC_VALUE       0x000
#define VIRTIO_MMIO_VERSION           0x004
#define VIRTIO_MMIO_DEVICE_ID         0x008
#define VIRTIO_MMIO_VENDOR_ID         0x00C
#define VIRTIO_MMIO_DEVICE_FEATURES   0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES   0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE   0x028
#define VIRTIO_MMIO_QUEUE_SEL         0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX     0x034
#define VIRTIO_MMIO_QUEUE_NUM         0x038
#define VIRTIO_MMIO_QUEUE_ALIGN       0x03C
#define VIRTIO_MMIO_QUEUE_PFN         0x040
#define VIRTIO_MMIO_QUEUE_NOTIFY      0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS  0x060
#define VIRTIO_MMIO_INTERRUPT_ACK     0x064
#define VIRTIO_MMIO_STATUS            0x070

#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FAILED      128

#define VIRTQ_DESC_F_NEXT  1
#define VIRTQ_DESC_F_WRITE 2

#define VIRTQ_SIZE 8
#define VIRTQ_ALIGN 4096

#define EV_SYN 0x00
#define EV_KEY 0x01

#define KEY_ESC        1
#define KEY_1          2
#define KEY_2          3
#define KEY_3          4
#define KEY_4          5
#define KEY_5          6
#define KEY_6          7
#define KEY_7          8
#define KEY_8          9
#define KEY_9          10
#define KEY_0          11
#define KEY_MINUS      12
#define KEY_EQUAL      13
#define KEY_BACKSPACE  14
#define KEY_TAB        15
#define KEY_Q          16
#define KEY_W          17
#define KEY_E          18
#define KEY_R          19
#define KEY_T          20
#define KEY_Y          21
#define KEY_U          22
#define KEY_I          23
#define KEY_O          24
#define KEY_P          25
#define KEY_ENTER      28
#define KEY_A          30
#define KEY_S          31
#define KEY_D          32
#define KEY_F          33
#define KEY_G          34
#define KEY_H          35
#define KEY_J          36
#define KEY_K          37
#define KEY_L          38
#define KEY_Z          44
#define KEY_X          45
#define KEY_C          46
#define KEY_V          47
#define KEY_B          48
#define KEY_N          49
#define KEY_M          50
#define KEY_SPACE      57
#define KEY_LEFTSHIFT  42
#define KEY_RIGHTSHIFT 54

#define ALIGN_UP_VALUE(value, align) (((value) + ((align) - 1)) & ~((align) - 1))

#define VIRTQ_DESC_OFFSET 0
#define VIRTQ_AVAIL_OFFSET (16 * VIRTQ_SIZE)
#define VIRTQ_AVAIL_SIZE (4 + 2 * VIRTQ_SIZE)
#define VIRTQ_USED_OFFSET ALIGN_UP_VALUE(VIRTQ_AVAIL_OFFSET + VIRTQ_AVAIL_SIZE, VIRTQ_ALIGN)

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) VirtqDesc;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTQ_SIZE];
} __attribute__((packed)) VirtqAvail;

typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) VirtqUsedElem;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    VirtqUsedElem ring[VIRTQ_SIZE];
} __attribute__((packed)) VirtqUsed;

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t value;
} __attribute__((packed)) VirtioInputEvent;

static uintptr_t keyboard_base = 0;
static int keyboard_ready = 0;
static int keyboard_init_tried = 0;

static uint8_t virtqueue_mem[8192] __attribute__((aligned(4096)));
static volatile VirtioInputEvent event_buffers[VIRTQ_SIZE] __attribute__((aligned(16)));

static uint16_t last_used_idx = 0;
static int shift_pressed = 0;
static int caps_lock_enabled = 0;

static int use_uppercase_for_letters(void) {
    return shift_pressed ^ caps_lock_enabled;
}

static uint32_t mmio_read32(uintptr_t addr) {
    return *(volatile uint32_t*)addr;
}

static void mmio_write32(uintptr_t addr, uint32_t value) {
    *(volatile uint32_t*)addr = value;
}

static void memory_barrier(void) {
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static volatile VirtqDesc* virtq_desc(void) {
    return (volatile VirtqDesc*)(void*)(virtqueue_mem + VIRTQ_DESC_OFFSET);
}

static volatile VirtqAvail* virtq_avail(void) {
    return (volatile VirtqAvail*)(void*)(virtqueue_mem + VIRTQ_AVAIL_OFFSET);
}

static volatile VirtqUsed* virtq_used(void) {
    return (volatile VirtqUsed*)(void*)(virtqueue_mem + VIRTQ_USED_OFFSET);
}

static void zero_memory(void* ptr, uint32_t size) {
    volatile uint8_t* p = (volatile uint8_t*)ptr;

    for (uint32_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}

static void virtio_set_status(uint32_t status) {
    mmio_write32(keyboard_base + VIRTIO_MMIO_STATUS, status);
}

static uint32_t virtio_get_status(void) {
    return mmio_read32(keyboard_base + VIRTIO_MMIO_STATUS);
}

static void virtio_add_status(uint32_t status) {
    virtio_set_status(virtio_get_status() | status);
}

static void virtqueue_add_buffer(uint16_t descriptor_id) {
    volatile VirtqAvail* avail = virtq_avail();

    uint16_t idx = avail->idx;

    avail->ring[idx % VIRTQ_SIZE] = descriptor_id;

    memory_barrier();

    avail->idx = idx + 1;

    memory_barrier();
}

static void virtqueue_notify(void) {
    mmio_write32(keyboard_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);
}

static char keycode_to_char(uint16_t code) {
    int upper = use_uppercase_for_letters();

    switch (code) {
        case KEY_A: return upper ? 'A' : 'a';
        case KEY_B: return upper ? 'B' : 'b';
        case KEY_C: return upper ? 'C' : 'c';
        case KEY_D: return upper ? 'D' : 'd';
        case KEY_E: return upper ? 'E' : 'e';
        case KEY_F: return upper ? 'F' : 'f';
        case KEY_G: return upper ? 'G' : 'g';
        case KEY_H: return upper ? 'H' : 'h';
        case KEY_I: return upper ? 'I' : 'i';
        case KEY_J: return upper ? 'J' : 'j';
        case KEY_K: return upper ? 'K' : 'k';
        case KEY_L: return upper ? 'L' : 'l';
        case KEY_M: return upper ? 'M' : 'm';
        case KEY_N: return upper ? 'N' : 'n';
        case KEY_O: return upper ? 'O' : 'o';
        case KEY_P: return upper ? 'P' : 'p';
        case KEY_Q: return upper ? 'Q' : 'q';
        case KEY_R: return upper ? 'R' : 'r';
        case KEY_S: return upper ? 'S' : 's';
        case KEY_T: return upper ? 'T' : 't';
        case KEY_U: return upper ? 'U' : 'u';
        case KEY_V: return upper ? 'V' : 'v';
        case KEY_W: return upper ? 'W' : 'w';
        case KEY_X: return upper ? 'X' : 'x';
        case KEY_Y: return upper ? 'Y' : 'y';
        case KEY_Z: return upper ? 'Z' : 'z';

        case KEY_1: return shift_pressed ? '!' : '1';
        case KEY_2: return shift_pressed ? '@' : '2';
        case KEY_3: return shift_pressed ? '#' : '3';
        case KEY_4: return shift_pressed ? '$' : '4';
        case KEY_5: return shift_pressed ? '%' : '5';
        case KEY_6: return shift_pressed ? '^' : '6';
        case KEY_7: return shift_pressed ? '&' : '7';
        case KEY_8: return shift_pressed ? '*' : '8';
        case KEY_9: return shift_pressed ? '(' : '9';
        case KEY_0: return shift_pressed ? ')' : '0';

        case KEY_MINUS: return shift_pressed ? '_' : '-';
        case KEY_EQUAL: return shift_pressed ? '+' : '=';
        case KEY_SPACE: return ' ';
        case KEY_ENTER: return '\n';
        case KEY_BACKSPACE: return '\b';
        case KEY_TAB: return '\t';

        default:
            return 0;
    }
}

static void handle_input_event(volatile VirtioInputEvent* event) {
    if (event->type != EV_KEY) {
        return;
    }

    uint16_t code = event->code;
    int32_t value = event->value;

    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
        shift_pressed = value != 0;
        return;
    }

    if (code == KEY_CAPSLOCK && value == 1) {
        caps_lock_enabled = !caps_lock_enabled;
        return;
    }

    if (value != 1 && value != 2) {
        return;
    }

    char ch = keycode_to_char(code);

    if (ch != 0) {
        input_push_char(ch);
    }
}

static int virtio_keyboard_init(void) {
    VirtioMMIODevice dev;

    if (!virtio_mmio_find_device(VIRTIO_INPUT_DEVICE_ID, &dev)) {
        return 0;
    }

    keyboard_base = dev.base;

    virtio_set_status(0);
    memory_barrier();

    virtio_add_status(VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_add_status(VIRTIO_STATUS_DRIVER);

    mmio_write32(keyboard_base + VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write32(keyboard_base + VIRTIO_MMIO_DRIVER_FEATURES, 0);

    mmio_write32(keyboard_base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

    mmio_write32(keyboard_base + VIRTIO_MMIO_QUEUE_SEL, 0);

    uint32_t max_queue_size = mmio_read32(keyboard_base + VIRTIO_MMIO_QUEUE_NUM_MAX);

    if (max_queue_size < VIRTQ_SIZE) {
        virtio_add_status(VIRTIO_STATUS_FAILED);
        return 0;
    }

    zero_memory(virtqueue_mem, sizeof(virtqueue_mem));

    volatile VirtqDesc* desc = virtq_desc();
    volatile VirtqAvail* avail = virtq_avail();

    avail->flags = 0;
    avail->idx = 0;

    for (uint16_t i = 0; i < VIRTQ_SIZE; i++) {
        desc[i].addr = (uint64_t)(uintptr_t)&event_buffers[i];
        desc[i].len = sizeof(VirtioInputEvent);
        desc[i].flags = VIRTQ_DESC_F_WRITE;
        desc[i].next = 0;
    }

    last_used_idx = 0;

    mmio_write32(keyboard_base + VIRTIO_MMIO_QUEUE_NUM, VIRTQ_SIZE);
    mmio_write32(keyboard_base + VIRTIO_MMIO_QUEUE_ALIGN, VIRTQ_ALIGN);
    mmio_write32(
        keyboard_base + VIRTIO_MMIO_QUEUE_PFN,
        (uint32_t)((uintptr_t)virtqueue_mem >> 12)
    );

    memory_barrier();

    for (uint16_t i = 0; i < VIRTQ_SIZE; i++) {
        virtqueue_add_buffer(i);
    }

    virtio_add_status(VIRTIO_STATUS_DRIVER_OK);

    memory_barrier();

    virtqueue_notify();

    return 1;
}

static void virtio_keyboard_update(void) {
    if (!keyboard_init_tried) {
        keyboard_init_tried = 1;
        keyboard_ready = virtio_keyboard_init();
    }

    if (!keyboard_ready) {
        return;
    }

    volatile VirtqUsed* used = virtq_used();

    memory_barrier();

    while (last_used_idx != used->idx) {
        uint16_t used_ring_index = last_used_idx % VIRTQ_SIZE;
        uint32_t descriptor_id = used->ring[used_ring_index].id;

        memory_barrier();

        if (descriptor_id < VIRTQ_SIZE) {
            handle_input_event(&event_buffers[descriptor_id]);

            virtqueue_add_buffer((uint16_t)descriptor_id);
        }

        last_used_idx++;
    }

    memory_barrier();

    virtqueue_notify();
}

void input_update(void) {
    virtio_keyboard_update();
}