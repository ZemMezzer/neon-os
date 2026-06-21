#include <stdint.h>

#include "input.h"
#include "uart.h"
#include "virtio_mmio.h"

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

#define KEY_LEFTBRACE  26
#define KEY_RIGHTBRACE 27
#define KEY_ENTER      28
#define KEY_LEFTCTRL   29

#define KEY_A          30
#define KEY_S          31
#define KEY_D          32
#define KEY_F          33
#define KEY_G          34
#define KEY_H          35
#define KEY_J          36
#define KEY_K          37
#define KEY_L          38

#define KEY_SEMICOLON  39
#define KEY_APOSTROPHE 40
#define KEY_GRAVE      41

#define KEY_LEFTSHIFT  42
#define KEY_BACKSLASH  43

#define KEY_Z          44
#define KEY_X          45
#define KEY_C          46
#define KEY_V          47
#define KEY_B          48
#define KEY_N          49
#define KEY_M          50

#define KEY_COMMA      51
#define KEY_DOT        52
#define KEY_SLASH      53

#define KEY_RIGHTSHIFT 54
#define KEY_LEFTALT    56
#define KEY_SPACE      57
#define KEY_CAPSLOCK   58
#define KEY_F1         59
#define KEY_F2         60
#define KEY_F3         61
#define KEY_F4         62
#define KEY_F5         63
#define KEY_F6         64
#define KEY_F7         65
#define KEY_F8         66
#define KEY_F9         67
#define KEY_F10        68

#define KEY_102ND      86
#define KEY_F11        87
#define KEY_F12        88
#define KEY_RIGHTCTRL  97
#define KEY_RIGHTALT   100

#define KEY_HOME       102
#define KEY_UP         103
#define KEY_PAGEUP     104
#define KEY_LEFT       105
#define KEY_RIGHT      106
#define KEY_END        107
#define KEY_DOWN       108
#define KEY_PAGEDOWN   109
#define KEY_INSERT     110
#define KEY_DELETE     111

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

#define ALIGN_UP_VALUE(value, align) \
    (((value) + ((align) - 1)) & ~((align) - 1))

#define VIRTQ_DESC_OFFSET 0
#define VIRTQ_AVAIL_OFFSET (16 * VIRTQ_SIZE)
#define VIRTQ_AVAIL_SIZE (4 + 2 * VIRTQ_SIZE)
#define VIRTQ_USED_OFFSET \
    ALIGN_UP_VALUE(VIRTQ_AVAIL_OFFSET + VIRTQ_AVAIL_SIZE, VIRTQ_ALIGN)

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
static volatile VirtioInputEvent
    event_buffers[VIRTQ_SIZE] __attribute__((aligned(16)));

static uint16_t last_used_idx = 0;

static int left_shift_pressed = 0;
static int right_shift_pressed = 0;
static int left_ctrl_pressed = 0;
static int right_ctrl_pressed = 0;
static int left_alt_pressed = 0;
static int right_alt_pressed = 0;
static int caps_lock_enabled = 0;

static volatile int global_close_requested = 0;

static int input_shift_pressed(void) {
    return left_shift_pressed || right_shift_pressed;
}

static uint8_t input_current_modifiers(void) {
    uint8_t modifiers = INPUT_MOD_NONE;

    if (input_shift_pressed()) {
        modifiers |= INPUT_MOD_SHIFT;
    }

    if (left_ctrl_pressed || right_ctrl_pressed) {
        modifiers |= INPUT_MOD_CTRL;
    }

    if (left_alt_pressed || right_alt_pressed) {
        modifiers |= INPUT_MOD_ALT;
    }

    return modifiers;
}

static int use_uppercase_for_letters(void) {
    return input_shift_pressed() ^ caps_lock_enabled;
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
    return (volatile VirtqDesc*)(void*)
        (virtqueue_mem + VIRTQ_DESC_OFFSET);
}

static volatile VirtqAvail* virtq_avail(void) {
    return (volatile VirtqAvail*)(void*)
        (virtqueue_mem + VIRTQ_AVAIL_OFFSET);
}

static volatile VirtqUsed* virtq_used(void) {
    return (volatile VirtqUsed*)(void*)
        (virtqueue_mem + VIRTQ_USED_OFFSET);
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
    int shift = input_shift_pressed();

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

        case KEY_1: return shift ? '!' : '1';
        case KEY_2: return shift ? '@' : '2';
        case KEY_3: return shift ? '#' : '3';
        case KEY_4: return shift ? '$' : '4';
        case KEY_5: return shift ? '%' : '5';
        case KEY_6: return shift ? '^' : '6';
        case KEY_7: return shift ? '&' : '7';
        case KEY_8: return shift ? '*' : '8';
        case KEY_9: return shift ? '(' : '9';
        case KEY_0: return shift ? ')' : '0';

        case KEY_MINUS: return shift ? '_' : '-';
        case KEY_EQUAL: return shift ? '+' : '=';
        case KEY_LEFTBRACE: return shift ? '{' : '[';
        case KEY_RIGHTBRACE: return shift ? '}' : ']';
        case KEY_SEMICOLON: return shift ? ':' : ';';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        case KEY_GRAVE: return shift ? '~' : '`';
        case KEY_BACKSLASH: return shift ? '|' : '\\';
        case KEY_COMMA: return shift ? '<' : ',';
        case KEY_DOT: return shift ? '>' : '.';
        case KEY_SLASH: return shift ? '?' : '/';
        case KEY_102ND: return shift ? '>' : '<';

        case KEY_SPACE: return ' ';
        case KEY_ENTER: return '\n';
        case KEY_BACKSPACE: return '\b';
        case KEY_TAB: return '\t';

        default:
            return 0;
    }
}

static int handle_special_key(uint16_t code, uint8_t modifiers) {
    if (code == KEY_ESC) {
        input_push_key_with_modifiers(INPUT_KEY_ESCAPE, modifiers);
        return 1;
    }

    if (code == KEY_LEFT) {
        input_push_key_with_modifiers(INPUT_KEY_LEFT, modifiers);
        return 1;
    }

    if (code == KEY_RIGHT) {
        input_push_key_with_modifiers(INPUT_KEY_RIGHT, modifiers);
        return 1;
    }

    if (code == KEY_UP) {
        input_push_key_with_modifiers(INPUT_KEY_UP, modifiers);
        return 1;
    }

    if (code == KEY_DOWN) {
        input_push_key_with_modifiers(INPUT_KEY_DOWN, modifiers);
        return 1;
    }

    if (code == KEY_HOME) {
        input_push_key_with_modifiers(INPUT_KEY_HOME, modifiers);
        return 1;
    }

    if (code == KEY_END) {
        input_push_key_with_modifiers(INPUT_KEY_END, modifiers);
        return 1;
    }

    if (code == KEY_DELETE) {
        input_push_key_with_modifiers(INPUT_KEY_DELETE, modifiers);
        return 1;
    }

    if (code == KEY_PAGEUP) {
        input_push_key_with_modifiers(INPUT_KEY_PAGE_UP, modifiers);
        return 1;
    }

    if (code == KEY_PAGEDOWN) {
        input_push_key_with_modifiers(INPUT_KEY_PAGE_DOWN, modifiers);
        return 1;
    }

    if (code == KEY_INSERT) {
        input_push_key_with_modifiers(INPUT_KEY_INSERT, modifiers);
        return 1;
    }

    if (code >= KEY_F1 && code <= KEY_F10) {
        InputKey key = (InputKey)(
            INPUT_KEY_F1 + (code - KEY_F1)
        );

        input_push_key_with_modifiers(key, modifiers);
        return 1;
    }

    if (code == KEY_F11) {
        input_push_key_with_modifiers(INPUT_KEY_F11, modifiers);
        return 1;
    }

    if (code == KEY_F12) {
        input_push_key_with_modifiers(INPUT_KEY_F12, modifiers);
        return 1;
    }

    return 0;
}

static void handle_modifier_key(uint16_t code, int32_t value) {
    int pressed = value != 0;

    if (code == KEY_LEFTSHIFT) {
        left_shift_pressed = pressed;
    } else if (code == KEY_RIGHTSHIFT) {
        right_shift_pressed = pressed;
    } else if (code == KEY_LEFTCTRL) {
        left_ctrl_pressed = pressed;
    } else if (code == KEY_RIGHTCTRL) {
        right_ctrl_pressed = pressed;
    } else if (code == KEY_LEFTALT) {
        left_alt_pressed = pressed;
    } else if (code == KEY_RIGHTALT) {
        right_alt_pressed = pressed;
    }
}

static int is_modifier_key(uint16_t code) {
    return code == KEY_LEFTSHIFT ||
           code == KEY_RIGHTSHIFT ||
           code == KEY_LEFTCTRL ||
           code == KEY_RIGHTCTRL ||
           code == KEY_LEFTALT ||
           code == KEY_RIGHTALT;
}

static void handle_input_event(volatile VirtioInputEvent* event) {
    uint16_t code;
    int32_t value;
    uint8_t modifiers;
    char ch;

    if (event->type != EV_KEY) {
        return;
    }

    code = event->code;
    value = event->value;

    if (is_modifier_key(code)) {
        handle_modifier_key(code, value);
        return;
    }

    if (code == KEY_CAPSLOCK && value == 1) {
        caps_lock_enabled = !caps_lock_enabled;
        return;
    }

    /*
        0 = released
        1 = pressed
        2 = auto-repeat
    */
    if (value != 1 && value != 2) {
        return;
    }

    modifiers = input_current_modifiers();

    if (
        code == KEY_F4 &&
        value == 1 &&
        (modifiers & INPUT_MOD_ALT) != 0
    ) {
        global_close_requested = 1;
        return;
    }

    if (handle_special_key(code, modifiers)) {
        return;
    }

    ch = keycode_to_char(code);

    if (ch != 0) {
        input_push_char_with_modifiers(ch, modifiers);
    }
}

static int virtio_keyboard_init(void) {
    VirtioMMIODevice dev;
    uint32_t max_queue_size;
    volatile VirtqDesc* desc;
    volatile VirtqAvail* avail;

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

    max_queue_size = mmio_read32(
        keyboard_base + VIRTIO_MMIO_QUEUE_NUM_MAX
    );

    if (max_queue_size < VIRTQ_SIZE) {
        virtio_add_status(VIRTIO_STATUS_FAILED);
        return 0;
    }

    zero_memory(virtqueue_mem, sizeof(virtqueue_mem));

    desc = virtq_desc();
    avail = virtq_avail();

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
    volatile VirtqUsed* used;

    if (!keyboard_init_tried) {
        keyboard_init_tried = 1;
        keyboard_ready = virtio_keyboard_init();
    }

    if (!keyboard_ready) {
        return;
    }

    used = virtq_used();

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

/*
    QEMU's "-serial stdio" connection delivers host terminal input through
    the PL011 UART.  Feed it into the same queue used by the VirtIO keyboard,
    so the shell and Lua programs can accept either keyboard source.
*/
#define SERIAL_INPUT_BYTES_PER_UPDATE 64

typedef enum SerialEscapeState {
    SERIAL_ESCAPE_NONE = 0,
    SERIAL_ESCAPE_GOT_ESC,
    SERIAL_ESCAPE_CSI
} SerialEscapeState;

static SerialEscapeState serial_escape_state = SERIAL_ESCAPE_NONE;
static unsigned int serial_csi_number = 0;
static int serial_skip_lf = 0;

static void serial_reset_escape(void) {
    serial_escape_state = SERIAL_ESCAPE_NONE;
    serial_csi_number = 0;
}

static void serial_finish_csi(unsigned char final) {
    if (final == 'A') {
        input_push_key(INPUT_KEY_UP);
    } else if (final == 'B') {
        input_push_key(INPUT_KEY_DOWN);
    } else if (final == 'C') {
        input_push_key(INPUT_KEY_RIGHT);
    } else if (final == 'D') {
        input_push_key(INPUT_KEY_LEFT);
    } else if (final == 'H') {
        input_push_key(INPUT_KEY_HOME);
    } else if (final == 'F') {
        input_push_key(INPUT_KEY_END);
    } else if (final == '~') {
        if (serial_csi_number == 1 || serial_csi_number == 7) {
            input_push_key(INPUT_KEY_HOME);
        } else if (serial_csi_number == 2) {
            input_push_key(INPUT_KEY_INSERT);
        } else if (serial_csi_number == 3) {
            input_push_key(INPUT_KEY_DELETE);
        } else if (serial_csi_number == 4 || serial_csi_number == 8) {
            input_push_key(INPUT_KEY_END);
        } else if (serial_csi_number == 5) {
            input_push_key(INPUT_KEY_PAGE_UP);
        } else if (serial_csi_number == 6) {
            input_push_key(INPUT_KEY_PAGE_DOWN);
        }
    }

    serial_reset_escape();
}

static void serial_process_byte(unsigned char byte) {
    if (serial_escape_state == SERIAL_ESCAPE_GOT_ESC) {
        if (byte == '[') {
            serial_escape_state = SERIAL_ESCAPE_CSI;
            serial_csi_number = 0;
            return;
        }

        /* A normal character followed a standalone Escape key. */
        input_push_key(INPUT_KEY_ESCAPE);
        serial_reset_escape();
        serial_process_byte(byte);
        return;
    }

    if (serial_escape_state == SERIAL_ESCAPE_CSI) {
        if (byte >= '0' && byte <= '9') {
            if (serial_csi_number < 999) {
                serial_csi_number =
                    serial_csi_number * 10u + (unsigned int)(byte - '0');
            }
            return;
        }

        /* Ignore modifier parameters, e.g. ESC [ 1 ; 5 D (Ctrl+Left). */
        if (byte == ';' || byte == '?' || byte == '>') {
            return;
        }

        serial_finish_csi(byte);
        return;
    }

    if (byte == 0x1B) {
        serial_escape_state = SERIAL_ESCAPE_GOT_ESC;
        return;
    }

    /* Host terminals may send CR, LF, or CRLF for Enter. */
    if (byte == '\r') {
        serial_skip_lf = 1;
        input_push_char('\n');
        return;
    }

    if (byte == '\n') {
        if (serial_skip_lf) {
            serial_skip_lf = 0;
            return;
        }

        input_push_char('\n');
        return;
    }

    serial_skip_lf = 0;

    /* Backspace may be sent as BS (8) or DEL (127). */
    if (byte == '\b' || byte == 127 || (byte >= 32 && byte <= 126)) {
        input_push_char((char)byte);
    }
}

static void uart_input_update(void) {
    unsigned int count = 0;

    while (count < SERIAL_INPUT_BYTES_PER_UPDATE) {
        /*
            uart_can_read() is non-blocking.  uart_getc() is safe immediately
            afterwards because this OS has one UART reader.
        */
        if (!uart_can_read()) {
            break;
        }

        serial_process_byte((unsigned char)uart_getc());
        count++;
    }

    /* Deliver a lone Escape after the UART receive FIFO has drained. */
    if (count == 0 && serial_escape_state == SERIAL_ESCAPE_GOT_ESC) {
        input_push_key(INPUT_KEY_ESCAPE);
        serial_reset_escape();
    }
}

void input_update(void) {
    virtio_keyboard_update();
    uart_input_update();
}

int input_take_global_close_request(void) {
    int requested = global_close_requested;

    global_close_requested = 0;

    return requested;
}
