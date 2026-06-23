#include <stdint.h>

#include "input.h"
#include "uart.h"

#define BCM2837_PERIPHERAL_BASE 0x3F000000UL
#define BCM2837_SYSTEM_TIMER_BASE (BCM2837_PERIPHERAL_BASE + 0x00003000UL)
#define BCM2837_SYSTEM_TIMER_CLO  (BCM2837_SYSTEM_TIMER_BASE + 0x04UL)
#define BCM2837_DWC2_BASE         (BCM2837_PERIPHERAL_BASE + 0x00980000UL)

#define DWC2_GAHBCFG    0x008U
#define DWC2_GUSBCFG    0x00CU
#define DWC2_GRSTCTL    0x010U
#define DWC2_GINTSTS    0x014U
#define DWC2_HCFG       0x400U
#define DWC2_HFNUM      0x408U
#define DWC2_HPRT0      0x440U
#define DWC2_PCGCCTL    0xE00U

#define DWC2_HCCHAR(channel)  (0x500U + 0x20U * (uint32_t)(channel))
#define DWC2_HCINT(channel)   (0x508U + 0x20U * (uint32_t)(channel))
#define DWC2_HCINTMSK(channel) (0x50CU + 0x20U * (uint32_t)(channel))
#define DWC2_HCTSIZ(channel)  (0x510U + 0x20U * (uint32_t)(channel))
#define DWC2_HCDMA(channel)   (0x514U + 0x20U * (uint32_t)(channel))

#define DWC2_GAHBCFG_DMA_EN       (1U << 5)
#define DWC2_GUSBCFG_FORCE_HOST   (1U << 29)
#define DWC2_GRSTCTL_AHB_IDLE     (1U << 31)
#define DWC2_GRSTCTL_CORE_RESET   (1U << 0)
#define DWC2_HCFG_FSLSPCLK_48MHZ  1U

#define DWC2_HPRT_SPEED_MASK       (3U << 17)
#define DWC2_HPRT_SPEED_LOW        (2U << 17)
#define DWC2_HPRT_POWER            (1U << 12)
#define DWC2_HPRT_RESET            (1U << 8)
#define DWC2_HPRT_OVER_CURRENT_CHG (1U << 5)
#define DWC2_HPRT_ENABLE_CHG       (1U << 3)
#define DWC2_HPRT_ENABLE           (1U << 2)
#define DWC2_HPRT_CONNECT_CHG      (1U << 1)
#define DWC2_HPRT_CONNECTED        (1U << 0)

#define DWC2_HCCHAR_ENABLE         (1U << 31)
#define DWC2_HCCHAR_ODD_FRAME      (1U << 29)
#define DWC2_HCCHAR_DEVICE_SHIFT   22U
#define DWC2_HCCHAR_ENDPOINT_TYPE_SHIFT 18U
#define DWC2_HCCHAR_LOW_SPEED      (1U << 17)
#define DWC2_HCCHAR_DIRECTION_IN   (1U << 15)
#define DWC2_HCCHAR_ENDPOINT_SHIFT 11U

#define DWC2_HCINT_XFER_COMPLETE   (1U << 0)
#define DWC2_HCINT_CHANNEL_HALTED  (1U << 1)
#define DWC2_HCINT_AHB_ERROR       (1U << 2)
#define DWC2_HCINT_STALL           (1U << 3)
#define DWC2_HCINT_NAK             (1U << 4)
#define DWC2_HCINT_ACK             (1U << 5)
#define DWC2_HCINT_NYET            (1U << 6)
#define DWC2_HCINT_XACT_ERROR      (1U << 7)
#define DWC2_HCINT_BABBLE_ERROR    (1U << 8)
#define DWC2_HCINT_DATA_TOGGLE_ERR (1U << 10)
#define DWC2_HCINT_CLEAR_MASK      0x3FFFU

#define DWC2_HCTSIZ_PID_DATA0      0U
#define DWC2_HCTSIZ_PID_DATA1      2U
#define DWC2_HCTSIZ_PID_SETUP      3U
#define DWC2_HCTSIZ_PID_SHIFT      29U
#define DWC2_HCTSIZ_PACKET_SHIFT   19U
#define DWC2_HCTSIZ_XFER_MASK      0x7FFFFU

#define DWC2_EP_TYPE_CONTROL       0U
#define DWC2_EP_TYPE_INTERRUPT     3U

#define USB_REQ_CLEAR_FEATURE       1U
#define USB_REQ_GET_STATUS          0U
#define USB_REQ_SET_FEATURE         3U
#define USB_REQ_GET_DESCRIPTOR      6U
#define USB_REQ_SET_ADDRESS         5U
#define USB_REQ_SET_CONFIGURATION   9U
#define USB_REQ_SET_IDLE             0x0AU
#define USB_REQ_SET_PROTOCOL         0x0BU

#define USB_DESC_DEVICE             1U
#define USB_DESC_CONFIGURATION      2U
#define USB_DESC_INTERFACE          4U
#define USB_DESC_ENDPOINT           5U
#define USB_DESC_HUB                0x29U

#define USB_CLASS_HUB               9U
#define USB_CLASS_HID               3U
#define USB_HID_SUBCLASS_BOOT       1U
#define USB_HID_PROTOCOL_KEYBOARD   1U
#define USB_ENDPOINT_XFER_INTERRUPT 3U

#define USB_HID_REPORT_BYTES        8U
#define USB_HID_MAX_PACKET          64U
#define USB_CONTROL_BUFFER_BYTES    256U

#define USB_HUB_ADDRESS             1U
#define USB_KEYBOARD_ADDRESS        2U
#define USB_CONTROL_CHANNEL         0U

#define USB_HUB_PORT_CONNECTION       (1U << 0)
#define USB_HUB_PORT_ENABLE           (1U << 1)
#define USB_HUB_PORT_LOW_SPEED        (1U << 9)
#define USB_HUB_PORT_HIGH_SPEED       (1U << 10)
#define USB_HUB_PORT_POWER_FEATURE    8U
#define USB_HUB_PORT_RESET_FEATURE    4U
#define USB_HUB_PORT_C_CONNECTION     16U
#define USB_HUB_PORT_C_ENABLE         17U
#define USB_HUB_PORT_C_RESET          20U

#define USB_TRANSFER_TIMEOUT_US     100000U
#define USB_PORT_RESET_US           50000U
#define USB_PORT_SETTLE_US          20000U
#define USB_RETRY_DELAY_US          1000U
#define USB_RETRY_COUNT             8U

#define HID_MOD_LEFT_CTRL  (1U << 0)
#define HID_MOD_LEFT_SHIFT (1U << 1)
#define HID_MOD_LEFT_ALT   (1U << 2)
#define HID_MOD_RIGHT_CTRL (1U << 4)
#define HID_MOD_RIGHT_SHIFT (1U << 5)
#define HID_MOD_RIGHT_ALT  (1U << 6)

#define HID_USAGE_A             0x04U
#define HID_USAGE_Z             0x1DU
#define HID_USAGE_1             0x1EU
#define HID_USAGE_0             0x27U
#define HID_USAGE_ENTER         0x28U
#define HID_USAGE_ESCAPE        0x29U
#define HID_USAGE_BACKSPACE     0x2AU
#define HID_USAGE_TAB           0x2BU
#define HID_USAGE_SPACE         0x2CU
#define HID_USAGE_MINUS         0x2DU
#define HID_USAGE_EQUAL         0x2EU
#define HID_USAGE_LEFT_BRACKET  0x2FU
#define HID_USAGE_RIGHT_BRACKET 0x30U
#define HID_USAGE_BACKSLASH     0x31U
#define HID_USAGE_SEMICOLON     0x33U
#define HID_USAGE_APOSTROPHE    0x34U
#define HID_USAGE_GRAVE         0x35U
#define HID_USAGE_COMMA         0x36U
#define HID_USAGE_DOT           0x37U
#define HID_USAGE_SLASH         0x38U
#define HID_USAGE_CAPS_LOCK     0x39U
#define HID_USAGE_F1            0x3AU
#define HID_USAGE_F12           0x45U
#define HID_USAGE_INSERT        0x49U
#define HID_USAGE_HOME          0x4AU
#define HID_USAGE_PAGE_UP       0x4BU
#define HID_USAGE_DELETE        0x4CU
#define HID_USAGE_END           0x4DU
#define HID_USAGE_PAGE_DOWN     0x4EU
#define HID_USAGE_RIGHT         0x4FU
#define HID_USAGE_LEFT          0x50U
#define HID_USAGE_DOWN          0x51U
#define HID_USAGE_UP            0x52U
#define HID_USAGE_NON_US_BACKSLASH 0x64U

typedef enum Dwc2TransferResult {
    DWC2_TRANSFER_OK = 0,
    DWC2_TRANSFER_NAK,
    DWC2_TRANSFER_STALL,
    DWC2_TRANSFER_TIMEOUT,
    DWC2_TRANSFER_ERROR
} Dwc2TransferResult;

typedef struct UsbSetupPacket {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} __attribute__((packed)) UsbSetupPacket;

static uint8_t usb_setup_dma[sizeof(UsbSetupPacket)] __attribute__((aligned(32)));
static uint8_t usb_control_dma[USB_CONTROL_BUFFER_BYTES] __attribute__((aligned(32)));
static uint8_t usb_interrupt_dma[USB_HID_MAX_PACKET] __attribute__((aligned(32)));

static int usb_controller_initialized = 0;
static int usb_keyboard_ready = 0;
static int usb_keyboard_attempted = 0;

static uint8_t usb_keyboard_device_address = USB_KEYBOARD_ADDRESS;
static uint8_t usb_keyboard_interface = 0;
static uint8_t usb_keyboard_endpoint = 0;
static uint16_t usb_keyboard_packet_size = USB_HID_REPORT_BYTES;
static uint32_t usb_keyboard_poll_interval_us = 10000U;
static uint32_t usb_keyboard_last_poll_at = 0;
static uint8_t usb_keyboard_data_pid = DWC2_HCTSIZ_PID_DATA0;

static uint8_t usb_previous_report[USB_HID_REPORT_BYTES];
static int usb_have_previous_report = 0;

static int usb_left_shift_pressed = 0;
static int usb_right_shift_pressed = 0;
static int usb_left_ctrl_pressed = 0;
static int usb_right_ctrl_pressed = 0;
static int usb_left_alt_pressed = 0;
static int usb_right_alt_pressed = 0;
static int usb_caps_lock_enabled = 0;
static volatile int usb_global_close_requested = 0;

static inline uint32_t mmio_read32(uintptr_t address) {
    return *(volatile uint32_t*)address;
}

static inline void mmio_write32(uintptr_t address, uint32_t value) {
    *(volatile uint32_t*)address = value;
}

static inline void data_memory_barrier(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline void cpu_relax(void) {
    __asm__ volatile("yield" ::: "memory");
}

static uint32_t timer_microseconds(void) {
    return mmio_read32(BCM2837_SYSTEM_TIMER_CLO);
}

static int timer_elapsed(uint32_t start, uint32_t duration_us) {
    return (uint32_t)(timer_microseconds() - start) >= duration_us;
}

static int timer_reached(uint32_t target_time) {
    return (int32_t)(timer_microseconds() - target_time) >= 0;
}

static void delay_microseconds(uint32_t duration_us) {
    uint32_t start = timer_microseconds();

    while (!timer_elapsed(start, duration_us)) {
        cpu_relax();
    }
}

static void usb_log(const char* message) {
    uart_puts(message);
}

static void usb_log_hex_byte(uint8_t value) {
    static const char digits[] = "0123456789ABCDEF";

    uart_putc(digits[(value >> 4) & 0x0FU]);
    uart_putc(digits[value & 0x0FU]);
}

static void usb_log_configuration_descriptor(
    const uint8_t* configuration,
    uint16_t configuration_length
) {
    uint16_t shown = configuration_length;

    if (shown > 64U) {
        shown = 64U;
    }

    uart_puts("usb: config bytes (");
    usb_log_hex_byte((uint8_t)(shown >> 8));
    usb_log_hex_byte((uint8_t)shown);
    uart_puts("):");

    for (uint16_t i = 0; i < shown; i++) {
        if ((i & 15U) == 0U) {
            uart_puts("\nusb:   ");
        }

        usb_log_hex_byte(configuration[i]);
        uart_putc(' ');
    }

    uart_putc('\n');
}

static inline uintptr_t dwc2_register(uint32_t offset) {
    return BCM2837_DWC2_BASE + (uintptr_t)offset;
}

static inline uint32_t dwc2_read(uint32_t offset) {
    return mmio_read32(dwc2_register(offset));
}

static inline void dwc2_write(uint32_t offset, uint32_t value) {
    mmio_write32(dwc2_register(offset), value);
}

static uint32_t usb_dma_address(const void* pointer) {
    return (uint32_t)(uintptr_t)pointer;
}

static int wait_for_register_bits(
    uint32_t offset,
    uint32_t mask,
    uint32_t expected,
    uint32_t timeout_us
) {
    uint32_t start = timer_microseconds();

    while ((dwc2_read(offset) & mask) != expected) {
        if (timer_elapsed(start, timeout_us)) {
            return 0;
        }

        cpu_relax();
    }

    return 1;
}

static int dwc2_wait_for_ahb_idle(void) {
    return wait_for_register_bits(
        DWC2_GRSTCTL,
        DWC2_GRSTCTL_AHB_IDLE,
        DWC2_GRSTCTL_AHB_IDLE,
        USB_TRANSFER_TIMEOUT_US
    );
}

static int dwc2_core_initialize(void) {
    uint32_t value;

    if (usb_controller_initialized) {
        return 1;
    }

    value = dwc2_read(DWC2_GUSBCFG);
    dwc2_write(DWC2_GUSBCFG, value | DWC2_GUSBCFG_FORCE_HOST);
    delay_microseconds(1000U);

    if (!dwc2_wait_for_ahb_idle()) {
        usb_log("usb: DWC2 AHB idle timeout\n");
        return 0;
    }

    value = dwc2_read(DWC2_GRSTCTL);
    dwc2_write(DWC2_GRSTCTL, value | DWC2_GRSTCTL_CORE_RESET);

    if (!wait_for_register_bits(
            DWC2_GRSTCTL,
            DWC2_GRSTCTL_CORE_RESET,
            0,
            USB_TRANSFER_TIMEOUT_US
        )) {
        usb_log("usb: DWC2 core reset timeout\n");
        return 0;
    }

    dwc2_write(DWC2_PCGCCTL, 0);

    value = dwc2_read(DWC2_GAHBCFG);
    value |= DWC2_GAHBCFG_DMA_EN;
    dwc2_write(DWC2_GAHBCFG, value);

    value = dwc2_read(DWC2_HCFG);
    value &= ~3U;
    value |= DWC2_HCFG_FSLSPCLK_48MHZ;
    dwc2_write(DWC2_HCFG, value);

    usb_controller_initialized = 1;
    return 1;
}

static void dwc2_write_port(uint32_t value) {
    value &= ~(DWC2_HPRT_CONNECT_CHG |
               DWC2_HPRT_ENABLE_CHG |
               DWC2_HPRT_OVER_CURRENT_CHG);
    dwc2_write(DWC2_HPRT0, value);
}

static int dwc2_port_reset(void) {
    uint32_t port;

    port = dwc2_read(DWC2_HPRT0);

    if ((port & DWC2_HPRT_CONNECTED) == 0) {
        return 0;
    }

    port |= DWC2_HPRT_POWER;
    dwc2_write_port(port);
    delay_microseconds(USB_PORT_SETTLE_US);

    port = dwc2_read(DWC2_HPRT0);
    port |= DWC2_HPRT_POWER | DWC2_HPRT_RESET;
    dwc2_write_port(port);
    delay_microseconds(USB_PORT_RESET_US);

    port = dwc2_read(DWC2_HPRT0);
    port &= ~DWC2_HPRT_RESET;
    port |= DWC2_HPRT_POWER;
    dwc2_write_port(port);
    delay_microseconds(USB_PORT_SETTLE_US);

    port = dwc2_read(DWC2_HPRT0);

    return (port & DWC2_HPRT_CONNECTED) != 0 &&
           (port & DWC2_HPRT_ENABLE) != 0;
}

static Dwc2TransferResult dwc2_channel_transfer(
    uint8_t device_address,
    uint8_t endpoint,
    int direction_in,
    uint8_t endpoint_type,
    uint16_t max_packet_size,
    uint8_t pid,
    uint8_t* buffer,
    uint32_t length,
    uint32_t timeout_us,
    uint32_t* out_actual_length
) {
    uint32_t packet_count;
    uint32_t hctsiz;
    uint32_t hcchar;
    uint32_t start;

    if (max_packet_size == 0 || max_packet_size > 0x7FFU ||
        length > DWC2_HCTSIZ_XFER_MASK) {
        return DWC2_TRANSFER_ERROR;
    }

    packet_count = length == 0
        ? 1U
        : (length + (uint32_t)max_packet_size - 1U) /
            (uint32_t)max_packet_size;

    if (packet_count > 0x3FFU) {
        return DWC2_TRANSFER_ERROR;
    }

    if (out_actual_length != 0) {
        *out_actual_length = 0;
    }

    dwc2_write(DWC2_HCINTMSK(USB_CONTROL_CHANNEL), 0);
    dwc2_write(DWC2_HCINT(USB_CONTROL_CHANNEL), DWC2_HCINT_CLEAR_MASK);

    dwc2_write(
        DWC2_HCINTMSK(USB_CONTROL_CHANNEL),
        DWC2_HCINT_XFER_COMPLETE |
        DWC2_HCINT_CHANNEL_HALTED |
        DWC2_HCINT_AHB_ERROR |
        DWC2_HCINT_STALL |
        DWC2_HCINT_NAK |
        DWC2_HCINT_NYET |
        DWC2_HCINT_XACT_ERROR |
        DWC2_HCINT_BABBLE_ERROR |
        DWC2_HCINT_DATA_TOGGLE_ERR
    );

    hctsiz = (length & DWC2_HCTSIZ_XFER_MASK) |
             (packet_count << DWC2_HCTSIZ_PACKET_SHIFT) |
             ((uint32_t)pid << DWC2_HCTSIZ_PID_SHIFT);

    hcchar = ((uint32_t)device_address << DWC2_HCCHAR_DEVICE_SHIFT) |
             ((uint32_t)endpoint_type << DWC2_HCCHAR_ENDPOINT_TYPE_SHIFT) |
             ((uint32_t)endpoint << DWC2_HCCHAR_ENDPOINT_SHIFT) |
             (uint32_t)max_packet_size;

    if (direction_in) {
        hcchar |= DWC2_HCCHAR_DIRECTION_IN;
    }

    if ((dwc2_read(DWC2_HPRT0) & DWC2_HPRT_SPEED_MASK) ==
        DWC2_HPRT_SPEED_LOW) {
        hcchar |= DWC2_HCCHAR_LOW_SPEED;
    }

    if (endpoint_type == DWC2_EP_TYPE_INTERRUPT &&
        (dwc2_read(DWC2_HFNUM) & 1U) != 0) {
        hcchar |= DWC2_HCCHAR_ODD_FRAME;
    }

    data_memory_barrier();
    dwc2_write(DWC2_HCDMA(USB_CONTROL_CHANNEL), usb_dma_address(buffer));
    dwc2_write(DWC2_HCTSIZ(USB_CONTROL_CHANNEL), hctsiz);
    data_memory_barrier();
    dwc2_write(DWC2_HCCHAR(USB_CONTROL_CHANNEL), hcchar | DWC2_HCCHAR_ENABLE);

    start = timer_microseconds();

    for (;;) {
        uint32_t interrupt_status = dwc2_read(DWC2_HCINT(USB_CONTROL_CHANNEL));

        if (interrupt_status != 0) {
            uint32_t remaining;
            uint32_t actual;

            remaining = dwc2_read(DWC2_HCTSIZ(USB_CONTROL_CHANNEL)) &
                        DWC2_HCTSIZ_XFER_MASK;
            actual = length >= remaining ? length - remaining : 0;

            dwc2_write(
                DWC2_HCINT(USB_CONTROL_CHANNEL),
                interrupt_status & DWC2_HCINT_CLEAR_MASK
            );

            if ((interrupt_status & DWC2_HCINT_XFER_COMPLETE) != 0) {
                data_memory_barrier();

                if (out_actual_length != 0) {
                    *out_actual_length = actual;
                }

                return DWC2_TRANSFER_OK;
            }

            if ((interrupt_status & DWC2_HCINT_NAK) != 0 ||
                (interrupt_status & DWC2_HCINT_NYET) != 0) {
                return DWC2_TRANSFER_NAK;
            }

            if ((interrupt_status & DWC2_HCINT_STALL) != 0) {
                return DWC2_TRANSFER_STALL;
            }

            return DWC2_TRANSFER_ERROR;
        }

        if (timer_elapsed(start, timeout_us)) {
            return DWC2_TRANSFER_TIMEOUT;
        }

        cpu_relax();
    }
}

static Dwc2TransferResult dwc2_transfer_with_retry(
    uint8_t device_address,
    uint8_t endpoint,
    int direction_in,
    uint8_t endpoint_type,
    uint16_t max_packet_size,
    uint8_t pid,
    uint8_t* buffer,
    uint32_t length,
    uint32_t timeout_us,
    uint32_t* out_actual_length
) {
    Dwc2TransferResult result = DWC2_TRANSFER_ERROR;

    for (uint32_t attempt = 0; attempt < USB_RETRY_COUNT; attempt++) {
        result = dwc2_channel_transfer(
            device_address,
            endpoint,
            direction_in,
            endpoint_type,
            max_packet_size,
            pid,
            buffer,
            length,
            timeout_us,
            out_actual_length
        );

        if (result != DWC2_TRANSFER_NAK) {
            return result;
        }

        delay_microseconds(USB_RETRY_DELAY_US);
    }

    return result;
}

static void write_le16(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)(value >> 8);
}

static uint16_t read_le16(const uint8_t* input) {
    return (uint16_t)input[0] | ((uint16_t)input[1] << 8);
}

static int usb_control_request(
    uint8_t device_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t request_type,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    uint8_t* data,
    uint16_t length,
    uint32_t* out_actual_length
) {
    Dwc2TransferResult result;
    uint32_t actual = 0;
    int direction_in = (request_type & 0x80U) != 0;

    usb_setup_dma[0] = request_type;
    usb_setup_dma[1] = request;
    write_le16(&usb_setup_dma[2], value);
    write_le16(&usb_setup_dma[4], index);
    write_le16(&usb_setup_dma[6], length);

    result = dwc2_transfer_with_retry(
        device_address,
        0,
        0,
        DWC2_EP_TYPE_CONTROL,
        endpoint_zero_packet_size,
        DWC2_HCTSIZ_PID_SETUP,
        usb_setup_dma,
        sizeof(usb_setup_dma),
        USB_TRANSFER_TIMEOUT_US,
        &actual
    );

    if (result != DWC2_TRANSFER_OK || actual != sizeof(usb_setup_dma)) {
        return 0;
    }

    if (length != 0) {
        result = dwc2_transfer_with_retry(
            device_address,
            0,
            direction_in,
            DWC2_EP_TYPE_CONTROL,
            endpoint_zero_packet_size,
            DWC2_HCTSIZ_PID_DATA1,
            data,
            length,
            USB_TRANSFER_TIMEOUT_US,
            &actual
        );

        if (result != DWC2_TRANSFER_OK) {
            return 0;
        }
    } else {
        actual = 0;
    }

    result = dwc2_transfer_with_retry(
        device_address,
        0,
        !direction_in,
        DWC2_EP_TYPE_CONTROL,
        endpoint_zero_packet_size,
        DWC2_HCTSIZ_PID_DATA1,
        usb_setup_dma,
        0,
        USB_TRANSFER_TIMEOUT_US,
        0
    );

    if (result != DWC2_TRANSFER_OK) {
        return 0;
    }

    if (out_actual_length != 0) {
        *out_actual_length = actual;
    }

    return 1;
}

static int usb_get_descriptor(
    uint8_t device_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t descriptor_type,
    uint8_t descriptor_index,
    uint8_t* out,
    uint16_t length,
    uint32_t* out_actual_length
) {
    return usb_control_request(
        device_address,
        endpoint_zero_packet_size,
        0x80U,
        USB_REQ_GET_DESCRIPTOR,
        (uint16_t)(((uint16_t)descriptor_type << 8) | descriptor_index),
        0,
        out,
        length,
        out_actual_length
    );
}

static int usb_set_address(uint8_t address, uint16_t endpoint_zero_packet_size) {
    return usb_control_request(
        0,
        endpoint_zero_packet_size,
        0x00U,
        USB_REQ_SET_ADDRESS,
        address,
        0,
        usb_setup_dma,
        0,
        0
    );
}

static int usb_set_configuration(
    uint8_t device_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t configuration_value
) {
    return usb_control_request(
        device_address,
        endpoint_zero_packet_size,
        0x00U,
        USB_REQ_SET_CONFIGURATION,
        configuration_value,
        0,
        usb_setup_dma,
        0,
        0
    );
}

static int usb_hid_set_protocol(
    uint8_t device_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t interface_number
) {
    return usb_control_request(
        device_address,
        endpoint_zero_packet_size,
        0x21U,
        USB_REQ_SET_PROTOCOL,
        0,
        interface_number,
        usb_setup_dma,
        0,
        0
    );
}

static int usb_hid_set_idle(
    uint8_t device_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t interface_number
) {
    return usb_control_request(
        device_address,
        endpoint_zero_packet_size,
        0x21U,
        USB_REQ_SET_IDLE,
        0,
        interface_number,
        usb_setup_dma,
        0,
        0
    );
}

static int usb_find_hid_keyboard(
    const uint8_t* configuration,
    uint16_t configuration_length,
    uint8_t* out_interface,
    uint8_t* out_endpoint,
    uint16_t* out_packet_size,
    uint32_t* out_poll_interval_us
) {
    uint16_t position = 0;
    int hid_interface_active = 0;
    int boot_keyboard_interface = 0;
    int generic_candidate_found = 0;
    uint8_t generic_interface = 0;
    uint8_t generic_endpoint = 0;
    uint16_t generic_packet_size = 0;
    uint32_t generic_poll_interval_us = 0;

    while (position + 2U <= configuration_length) {
        uint8_t length = configuration[position];
        uint8_t type = configuration[position + 1U];

        if (length < 2U || position + length > configuration_length) {
            usb_log("usb: malformed configuration descriptor\n");
            return 0;
        }

        if (type == USB_DESC_INTERFACE && length >= 9U) {
            uint8_t interface_number = configuration[position + 2U];
            uint8_t interface_class = configuration[position + 5U];
            uint8_t interface_subclass = configuration[position + 6U];
            uint8_t interface_protocol = configuration[position + 7U];

            hid_interface_active = interface_class == USB_CLASS_HID;
            boot_keyboard_interface =
                hid_interface_active &&
                interface_subclass == USB_HID_SUBCLASS_BOOT &&
                interface_protocol == USB_HID_PROTOCOL_KEYBOARD;

            if (hid_interface_active) {
                uart_puts("usb: HID interface ");
                usb_log_hex_byte(interface_number);
                uart_puts(" subclass=");
                usb_log_hex_byte(interface_subclass);
                uart_puts(" protocol=");
                usb_log_hex_byte(interface_protocol);
                uart_putc('\n');
            }

            if (hid_interface_active) {
                *out_interface = interface_number;
            }
        } else if (type == USB_DESC_ENDPOINT && length >= 7U &&
                   hid_interface_active) {
            uint8_t endpoint_address = configuration[position + 2U];
            uint8_t attributes = configuration[position + 3U];
            uint16_t packet_size = read_le16(&configuration[position + 4U]);
            uint8_t interval_ms = configuration[position + 6U];
            uint32_t poll_interval_us =
                interval_ms == 0 ? 1000U : (uint32_t)interval_ms * 1000U;

            if ((endpoint_address & 0x80U) != 0 &&
                (attributes & 3U) == USB_ENDPOINT_XFER_INTERRUPT &&
                packet_size >= USB_HID_REPORT_BYTES &&
                packet_size <= USB_HID_MAX_PACKET) {
                if (boot_keyboard_interface) {
                    *out_endpoint = endpoint_address & 0x0FU;
                    *out_packet_size = packet_size;
                    *out_poll_interval_us = poll_interval_us;
                    return 1;
                }

                if (!generic_candidate_found) {
                    generic_candidate_found = 1;
                    generic_interface = *out_interface;
                    generic_endpoint = endpoint_address & 0x0FU;
                    generic_packet_size = packet_size;
                    generic_poll_interval_us = poll_interval_us;
                }
            }
        }

        position = (uint16_t)(position + length);
    }

    if (generic_candidate_found) {
        usb_log("usb: HID interface is not marked as Boot Keyboard; trying it\n");
        *out_interface = generic_interface;
        *out_endpoint = generic_endpoint;
        *out_packet_size = generic_packet_size;
        *out_poll_interval_us = generic_poll_interval_us;
        return 1;
    }

    return 0;
}

static void usb_keyboard_reset_state(void) {
    for (uint32_t i = 0; i < USB_HID_REPORT_BYTES; i++) {
        usb_previous_report[i] = 0;
    }

    usb_have_previous_report = 0;
    usb_keyboard_data_pid = DWC2_HCTSIZ_PID_DATA0;
    usb_left_shift_pressed = 0;
    usb_right_shift_pressed = 0;
    usb_left_ctrl_pressed = 0;
    usb_right_ctrl_pressed = 0;
    usb_left_alt_pressed = 0;
    usb_right_alt_pressed = 0;
}

static int usb_read_device_descriptor(
    uint8_t device_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t* out_device_class
) {
    uint32_t actual_length;

    if (!usb_get_descriptor(
            device_address,
            endpoint_zero_packet_size,
            USB_DESC_DEVICE,
            0,
            usb_control_dma,
            18,
            &actual_length
        ) || actual_length != 18U) {
        usb_log("usb: could not read full device descriptor\n");
        return 0;
    }

    if (out_device_class != 0) {
        *out_device_class = usb_control_dma[4];
    }

    return 1;
}

static int usb_enumerate_default_address_device(
    uint8_t assigned_address,
    uint16_t* out_endpoint_zero_packet_size,
    uint8_t* out_device_class
) {
    uint32_t actual_length;
    uint16_t endpoint_zero_packet_size;

    if (!usb_get_descriptor(
            0,
            8,
            USB_DESC_DEVICE,
            0,
            usb_control_dma,
            8,
            &actual_length
        ) || actual_length != 8U) {
        usb_log("usb: could not read device descriptor header\n");
        return 0;
    }

    endpoint_zero_packet_size = usb_control_dma[7];

    if (endpoint_zero_packet_size != 8U &&
        endpoint_zero_packet_size != 16U &&
        endpoint_zero_packet_size != 32U &&
        endpoint_zero_packet_size != 64U) {
        usb_log("usb: unsupported endpoint zero packet size\n");
        return 0;
    }

    if (!usb_set_address(assigned_address, endpoint_zero_packet_size)) {
        usb_log("usb: SET_ADDRESS failed\n");
        return 0;
    }

    delay_microseconds(USB_PORT_SETTLE_US);

    if (!usb_read_device_descriptor(
            assigned_address,
            endpoint_zero_packet_size,
            out_device_class
        )) {
        return 0;
    }

    *out_endpoint_zero_packet_size = endpoint_zero_packet_size;
    return 1;
}

static int usb_read_configuration(
    uint8_t device_address,
    uint16_t endpoint_zero_packet_size,
    uint16_t* out_total_length,
    uint8_t* out_configuration_value
) {
    uint32_t actual_length;
    uint16_t total_configuration_length;

    if (!usb_get_descriptor(
            device_address,
            endpoint_zero_packet_size,
            USB_DESC_CONFIGURATION,
            0,
            usb_control_dma,
            9,
            &actual_length
        ) || actual_length != 9U) {
        usb_log("usb: could not read configuration header\n");
        return 0;
    }

    total_configuration_length = read_le16(&usb_control_dma[2]);

    if (total_configuration_length < 9U ||
        total_configuration_length > USB_CONTROL_BUFFER_BYTES) {
        usb_log("usb: configuration descriptor is too large\n");
        return 0;
    }

    if (!usb_get_descriptor(
            device_address,
            endpoint_zero_packet_size,
            USB_DESC_CONFIGURATION,
            0,
            usb_control_dma,
            total_configuration_length,
            &actual_length
        ) || actual_length != total_configuration_length) {
        usb_log("usb: could not read complete configuration descriptor\n");
        return 0;
    }

    *out_total_length = total_configuration_length;
    *out_configuration_value = usb_control_dma[5];
    return 1;
}

static int usb_hub_get_descriptor(
    uint8_t hub_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t* out,
    uint16_t length,
    uint32_t* out_actual_length
) {
    return usb_control_request(
        hub_address,
        endpoint_zero_packet_size,
        0xA0U,
        USB_REQ_GET_DESCRIPTOR,
        (uint16_t)(USB_DESC_HUB << 8),
        0,
        out,
        length,
        out_actual_length
    );
}

static int usb_hub_get_port_status(
    uint8_t hub_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t port_number,
    uint16_t* out_status,
    uint16_t* out_change
) {
    uint32_t actual_length;

    if (!usb_control_request(
            hub_address,
            endpoint_zero_packet_size,
            0xA3U,
            USB_REQ_GET_STATUS,
            0,
            port_number,
            usb_control_dma,
            4,
            &actual_length
        ) || actual_length != 4U) {
        return 0;
    }

    *out_status = read_le16(&usb_control_dma[0]);
    *out_change = read_le16(&usb_control_dma[2]);
    return 1;
}

static int usb_hub_set_port_feature(
    uint8_t hub_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t port_number,
    uint16_t feature
) {
    return usb_control_request(
        hub_address,
        endpoint_zero_packet_size,
        0x23U,
        USB_REQ_SET_FEATURE,
        feature,
        port_number,
        usb_setup_dma,
        0,
        0
    );
}

static int usb_hub_clear_port_feature(
    uint8_t hub_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t port_number,
    uint16_t feature
) {
    return usb_control_request(
        hub_address,
        endpoint_zero_packet_size,
        0x23U,
        USB_REQ_CLEAR_FEATURE,
        feature,
        port_number,
        usb_setup_dma,
        0,
        0
    );
}

static void usb_log_hub_port_status(uint8_t port_number, uint16_t status, uint16_t change) {
    uart_puts("usb: hub port ");
    usb_log_hex_byte(port_number);
    uart_puts(" status=");
    usb_log_hex_byte((uint8_t)(status >> 8));
    usb_log_hex_byte((uint8_t)status);
    uart_puts(" change=");
    usb_log_hex_byte((uint8_t)(change >> 8));
    usb_log_hex_byte((uint8_t)change);
    uart_putc('\n');
}


static int usb_hub_find_keyboard_port(
    uint8_t hub_address,
    uint16_t endpoint_zero_packet_size,
    uint8_t* out_port_number
) {
    uint32_t actual_length;
    uint8_t port_count;

    if (!usb_hub_get_descriptor(
            hub_address,
            endpoint_zero_packet_size,
            usb_control_dma,
            7,
            &actual_length
        ) || actual_length < 3U || usb_control_dma[1] != USB_DESC_HUB) {
        usb_log("usb: could not read hub descriptor\n");
        return 0;
    }

    port_count = usb_control_dma[2];

uart_puts("usb: hub ports = ");
    usb_log_hex_byte(port_count);
    uart_putc('\n');

    for (uint8_t port = 1; port <= port_count; port++) {
        uint16_t status = 0;
        uint16_t change = 0;

        (void)usb_hub_set_port_feature(
            hub_address,
            endpoint_zero_packet_size,
            port,
            USB_HUB_PORT_POWER_FEATURE
        );

        delay_microseconds(5000U);

        if (!usb_hub_get_port_status(
                hub_address,
                endpoint_zero_packet_size,
                port,
                &status,
                &change
            )) {
            usb_log("usb: could not read hub port status\n");
            continue;
        }

        usb_log_hub_port_status(port, status, change);

        if ((status & USB_HUB_PORT_CONNECTION) == 0) {
            continue;
        }

        if ((change & USB_HUB_PORT_C_CONNECTION) != 0) {
            (void)usb_hub_clear_port_feature(
                hub_address,
                endpoint_zero_packet_size,
                port,
                USB_HUB_PORT_C_CONNECTION
            );
        }

        if (!usb_hub_set_port_feature(
                hub_address,
                endpoint_zero_packet_size,
                port,
                USB_HUB_PORT_RESET_FEATURE
            )) {
            usb_log("usb: hub port reset request failed\n");
            continue;
        }

        delay_microseconds(USB_PORT_RESET_US);

        if (!usb_hub_get_port_status(
                hub_address,
                endpoint_zero_packet_size,
                port,
                &status,
                &change
            )) {
            usb_log("usb: could not read hub port after reset\n");
            continue;
        }

        usb_log_hub_port_status(port, status, change);

        if ((change & USB_HUB_PORT_C_RESET) != 0) {
            (void)usb_hub_clear_port_feature(
                hub_address,
                endpoint_zero_packet_size,
                port,
                USB_HUB_PORT_C_RESET
            );
        }

        if ((change & USB_HUB_PORT_C_ENABLE) != 0) {
            (void)usb_hub_clear_port_feature(
                hub_address,
                endpoint_zero_packet_size,
                port,
                USB_HUB_PORT_C_ENABLE
            );
        }

        if ((status & (USB_HUB_PORT_CONNECTION | USB_HUB_PORT_ENABLE)) ==
            (USB_HUB_PORT_CONNECTION | USB_HUB_PORT_ENABLE)) {
            *out_port_number = port;
            return 1;
        }
    }

    usb_log("usb: no enabled device behind hub\n");
    return 0;
}

static int usb_configure_hid_keyboard(
    uint8_t device_address,
    uint16_t endpoint_zero_packet_size
) {
    uint16_t total_configuration_length;
    uint8_t configuration_value;

    if (!usb_read_configuration(
            device_address,
            endpoint_zero_packet_size,
            &total_configuration_length,
            &configuration_value
        )) {
        return 0;
    }

    usb_log_configuration_descriptor(usb_control_dma, total_configuration_length);

    if (!usb_find_hid_keyboard(
            usb_control_dma,
            total_configuration_length,
            &usb_keyboard_interface,
            &usb_keyboard_endpoint,
            &usb_keyboard_packet_size,
            &usb_keyboard_poll_interval_us
        )) {
        usb_log("usb: no HID Boot Keyboard interface\n");
        return 0;
    }

    if (!usb_set_configuration(
            device_address,
            endpoint_zero_packet_size,
            configuration_value
        )) {
        usb_log("usb: keyboard SET_CONFIGURATION failed\n");
        return 0;
    }

    if (!usb_hid_set_protocol(
            device_address,
            endpoint_zero_packet_size,
            usb_keyboard_interface
        )) {
        usb_log("usb: keyboard SET_PROTOCOL failed\n");
        return 0;
    }

    if (!usb_hid_set_idle(
            device_address,
            endpoint_zero_packet_size,
            usb_keyboard_interface
        )) {
        usb_log("usb: keyboard SET_IDLE failed\n");
        return 0;
    }

    usb_keyboard_device_address = device_address;
    return 1;
}

static int usb_keyboard_initialize(void) {
    uint16_t root_endpoint_zero_packet_size;
    uint8_t root_device_class;
    uint16_t hub_configuration_length;
    uint8_t hub_configuration_value;
    uint16_t keyboard_endpoint_zero_packet_size;
    uint8_t keyboard_device_class;
    uint8_t keyboard_port;

    usb_keyboard_ready = 0;
    usb_keyboard_reset_state();

    if (!dwc2_core_initialize()) {
        usb_log("usb: controller initialization failed\n");
        return 0;
    }

    if (!dwc2_port_reset()) {
        usb_log("usb: no device on DWC2 root port\n");
        return 0;
    }

    if (!usb_enumerate_default_address_device(
            USB_HUB_ADDRESS,
            &root_endpoint_zero_packet_size,
            &root_device_class
        )) {
        return 0;
    }

    if (root_device_class == USB_CLASS_HUB) {
        usb_log("usb: root device is a USB hub\n");

        if (!usb_read_configuration(
                USB_HUB_ADDRESS,
                root_endpoint_zero_packet_size,
                &hub_configuration_length,
                &hub_configuration_value
            )) {
            return 0;
        }

        usb_log_configuration_descriptor(usb_control_dma, hub_configuration_length);

        if (!usb_set_configuration(
                USB_HUB_ADDRESS,
                root_endpoint_zero_packet_size,
                hub_configuration_value
            )) {
            usb_log("usb: hub SET_CONFIGURATION failed\n");
            return 0;
        }

        delay_microseconds(USB_PORT_SETTLE_US);

        if (!usb_hub_find_keyboard_port(
                USB_HUB_ADDRESS,
                root_endpoint_zero_packet_size,
                &keyboard_port
            )) {
            return 0;
        }

uart_puts("usb: enumerating child on hub port ");
        usb_log_hex_byte(keyboard_port);
        uart_putc('\n');

        if (!usb_enumerate_default_address_device(
                USB_KEYBOARD_ADDRESS,
                &keyboard_endpoint_zero_packet_size,
                &keyboard_device_class
            )) {
            usb_log("usb: could not enumerate hub child\n");
            return 0;
        }

        if (keyboard_device_class != 0 && keyboard_device_class != USB_CLASS_HID) {
            usb_log("usb: hub child has unexpected device class; checking interfaces\n");
        }

        if (!usb_configure_hid_keyboard(
                USB_KEYBOARD_ADDRESS,
                keyboard_endpoint_zero_packet_size
            )) {
            return 0;
        }
    } else {
        usb_log("usb: root device is not a hub; treating it as keyboard\n");

        if (!usb_configure_hid_keyboard(
                USB_HUB_ADDRESS,
                root_endpoint_zero_packet_size
            )) {
            return 0;
        }
    }

    usb_keyboard_ready = 1;
    usb_keyboard_last_poll_at = timer_microseconds();
    usb_log("usb: HID keyboard ready\n");
    return 1;
}

static int usb_shift_pressed(void) {
    return usb_left_shift_pressed || usb_right_shift_pressed;
}

static uint8_t usb_current_modifiers(void) {
    uint8_t modifiers = INPUT_MOD_NONE;

    if (usb_shift_pressed()) {
        modifiers |= INPUT_MOD_SHIFT;
    }

    if (usb_left_ctrl_pressed || usb_right_ctrl_pressed) {
        modifiers |= INPUT_MOD_CTRL;
    }

    if (usb_left_alt_pressed || usb_right_alt_pressed) {
        modifiers |= INPUT_MOD_ALT;
    }

    return modifiers;
}

static char usb_usage_to_character(uint8_t usage) {
    int shifted = usb_shift_pressed();
    int uppercase = shifted ^ usb_caps_lock_enabled;

    if (usage >= HID_USAGE_A && usage <= HID_USAGE_Z) {
        char character = (char)('a' + usage - HID_USAGE_A);
        return uppercase ? (char)(character - 'a' + 'A') : character;
    }

    switch (usage) {
        case HID_USAGE_1: return shifted ? '!' : '1';
        case HID_USAGE_1 + 1U: return shifted ? '@' : '2';
        case HID_USAGE_1 + 2U: return shifted ? '#' : '3';
        case HID_USAGE_1 + 3U: return shifted ? '$' : '4';
        case HID_USAGE_1 + 4U: return shifted ? '%' : '5';
        case HID_USAGE_1 + 5U: return shifted ? '^' : '6';
        case HID_USAGE_1 + 6U: return shifted ? '&' : '7';
        case HID_USAGE_1 + 7U: return shifted ? '*' : '8';
        case HID_USAGE_1 + 8U: return shifted ? '(' : '9';
        case HID_USAGE_0: return shifted ? ')' : '0';
        case HID_USAGE_MINUS: return shifted ? '_' : '-';
        case HID_USAGE_EQUAL: return shifted ? '+' : '=';
        case HID_USAGE_LEFT_BRACKET: return shifted ? '{' : '[';
        case HID_USAGE_RIGHT_BRACKET: return shifted ? '}' : ']';
        case HID_USAGE_BACKSLASH: return shifted ? '|' : '\\';
        case HID_USAGE_SEMICOLON: return shifted ? ':' : ';';
        case HID_USAGE_APOSTROPHE: return shifted ? '"' : '\'';
        case HID_USAGE_GRAVE: return shifted ? '~' : '`';
        case HID_USAGE_COMMA: return shifted ? '<' : ',';
        case HID_USAGE_DOT: return shifted ? '>' : '.';
        case HID_USAGE_SLASH: return shifted ? '?' : '/';
        case HID_USAGE_NON_US_BACKSLASH: return shifted ? '>' : '<';
        case HID_USAGE_ENTER: return '\n';
        case HID_USAGE_BACKSPACE: return '\b';
        case HID_USAGE_TAB: return '\t';
        case HID_USAGE_SPACE: return ' ';
        default: return 0;
    }
}

static int usb_usage_to_special_key(uint8_t usage, uint8_t modifiers) {
    if (usage == HID_USAGE_ESCAPE) {
        input_push_key_with_modifiers(INPUT_KEY_ESCAPE, modifiers);
    } else if (usage == HID_USAGE_LEFT) {
        input_push_key_with_modifiers(INPUT_KEY_LEFT, modifiers);
    } else if (usage == HID_USAGE_RIGHT) {
        input_push_key_with_modifiers(INPUT_KEY_RIGHT, modifiers);
    } else if (usage == HID_USAGE_UP) {
        input_push_key_with_modifiers(INPUT_KEY_UP, modifiers);
    } else if (usage == HID_USAGE_DOWN) {
        input_push_key_with_modifiers(INPUT_KEY_DOWN, modifiers);
    } else if (usage == HID_USAGE_HOME) {
        input_push_key_with_modifiers(INPUT_KEY_HOME, modifiers);
    } else if (usage == HID_USAGE_END) {
        input_push_key_with_modifiers(INPUT_KEY_END, modifiers);
    } else if (usage == HID_USAGE_INSERT) {
        input_push_key_with_modifiers(INPUT_KEY_INSERT, modifiers);
    } else if (usage == HID_USAGE_DELETE) {
        input_push_key_with_modifiers(INPUT_KEY_DELETE, modifiers);
    } else if (usage == HID_USAGE_PAGE_UP) {
        input_push_key_with_modifiers(INPUT_KEY_PAGE_UP, modifiers);
    } else if (usage == HID_USAGE_PAGE_DOWN) {
        input_push_key_with_modifiers(INPUT_KEY_PAGE_DOWN, modifiers);
    } else if (usage >= HID_USAGE_F1 && usage <= HID_USAGE_F12) {
        input_push_key_with_modifiers(
            (InputKey)(INPUT_KEY_F1 + (usage - HID_USAGE_F1)),
            modifiers
        );
    } else {
        return 0;
    }

    return 1;
}

static int hid_report_contains_usage(const uint8_t* report, uint8_t usage) {
    for (uint32_t i = 2; i < USB_HID_REPORT_BYTES; i++) {
        if (report[i] == usage) {
            return 1;
        }
    }

    return 0;
}

static void usb_update_modifiers(uint8_t hid_modifiers) {
    usb_left_ctrl_pressed = (hid_modifiers & HID_MOD_LEFT_CTRL) != 0;
    usb_left_shift_pressed = (hid_modifiers & HID_MOD_LEFT_SHIFT) != 0;
    usb_left_alt_pressed = (hid_modifiers & HID_MOD_LEFT_ALT) != 0;
    usb_right_ctrl_pressed = (hid_modifiers & HID_MOD_RIGHT_CTRL) != 0;
    usb_right_shift_pressed = (hid_modifiers & HID_MOD_RIGHT_SHIFT) != 0;
    usb_right_alt_pressed = (hid_modifiers & HID_MOD_RIGHT_ALT) != 0;
}

static void usb_handle_usage_press(uint8_t usage) {
    uint8_t modifiers;
    char character;

    if (usage == 0 || usage == 1U || usage == 2U || usage == 3U) {
        return;
    }

    if (usage == HID_USAGE_CAPS_LOCK) {
        usb_caps_lock_enabled = !usb_caps_lock_enabled;
        return;
    }

    modifiers = usb_current_modifiers();

    if (usage == HID_USAGE_F1 + 3U &&
        (modifiers & INPUT_MOD_ALT) != 0) {
        usb_global_close_requested = 1;
        return;
    }

    if (usb_usage_to_special_key(usage, modifiers)) {
        return;
    }

    character = usb_usage_to_character(usage);

    if (character != 0) {
        input_push_char_with_modifiers(character, modifiers);
    }
}

static void usb_process_hid_report(const uint8_t* report) {
    usb_update_modifiers(report[0]);

    for (uint32_t i = 2; i < USB_HID_REPORT_BYTES; i++) {
        uint8_t usage = report[i];

        if (usage != 0 &&
            (!usb_have_previous_report ||
             !hid_report_contains_usage(usb_previous_report, usage))) {
            usb_handle_usage_press(usage);
        }
    }

    for (uint32_t i = 0; i < USB_HID_REPORT_BYTES; i++) {
        usb_previous_report[i] = report[i];
    }

    usb_have_previous_report = 1;
}

static void usb_keyboard_update(void) {
    Dwc2TransferResult result;
    uint32_t actual_length = 0;
    uint32_t now = timer_microseconds();


    if (!usb_keyboard_ready && !usb_keyboard_attempted) {
        usb_keyboard_attempted = 1;

        if (!usb_keyboard_initialize()) {
            usb_log("usb: keyboard initialization failed; USB input disabled for this boot\n");
            return;
        }
    }

    if (!usb_keyboard_ready ||
        !timer_elapsed(usb_keyboard_last_poll_at, usb_keyboard_poll_interval_us)) {
        return;
    }

    usb_keyboard_last_poll_at = now;

    result = dwc2_channel_transfer(
        usb_keyboard_device_address,
        usb_keyboard_endpoint,
        1,
        DWC2_EP_TYPE_INTERRUPT,
        usb_keyboard_packet_size,
        usb_keyboard_data_pid,
        usb_interrupt_dma,
        usb_keyboard_packet_size,
        USB_TRANSFER_TIMEOUT_US,
        &actual_length
    );

    if (result == DWC2_TRANSFER_NAK) {
        return;
    }

    if (result != DWC2_TRANSFER_OK) {
        usb_log("usb: keyboard endpoint stopped responding; USB input disabled for this boot\n");
        usb_keyboard_ready = 0;
        return;
    }

    if (actual_length >= USB_HID_REPORT_BYTES) {
        usb_process_hid_report(usb_interrupt_dma);
    }

    usb_keyboard_data_pid =
        usb_keyboard_data_pid == DWC2_HCTSIZ_PID_DATA0
            ? DWC2_HCTSIZ_PID_DATA1
            : DWC2_HCTSIZ_PID_DATA0;
}


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

static void serial_push_function_key(unsigned int number) {
    if (number >= 11 && number <= 15) {
        input_push_key((InputKey)(INPUT_KEY_F1 + (number - 11)));
    } else if (number == 17) {
        input_push_key(INPUT_KEY_F6);
    } else if (number == 18) {
        input_push_key(INPUT_KEY_F7);
    } else if (number == 19) {
        input_push_key(INPUT_KEY_F8);
    } else if (number == 20) {
        input_push_key(INPUT_KEY_F9);
    } else if (number == 21) {
        input_push_key(INPUT_KEY_F10);
    } else if (number == 23) {
        input_push_key(INPUT_KEY_F11);
    } else if (number == 24) {
        input_push_key(INPUT_KEY_F12);
    }
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
        } else {
            serial_push_function_key(serial_csi_number);
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

        input_push_key(INPUT_KEY_ESCAPE);
        serial_reset_escape();
        serial_process_byte(byte);
        return;
    }

    if (serial_escape_state == SERIAL_ESCAPE_CSI) {
        if (byte >= '0' && byte <= '9') {
            if (serial_csi_number < 999) {
                serial_csi_number = serial_csi_number * 10u +
                    (unsigned int)(byte - '0');
            }
            return;
        }

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

    if (byte == '\t') {
        input_push_char('\t');
        return;
    }

    if (byte == '\b' || byte == 127) {
        input_push_char('\b');
        return;
    }

    if (byte >= 1 && byte <= 26) {
        input_push_char_with_modifiers(
            (char)('a' + byte - 1),
            INPUT_MOD_CTRL
        );
        return;
    }

    if (byte >= 32 && byte <= 126) {
        input_push_char((char)byte);
    }
}

static void uart_input_update(void) {
    unsigned int count = 0;

    while (count < 64U) {
        if (!uart_can_read()) {
            break;
        }

        serial_process_byte((unsigned char)uart_getc());
        count++;
    }

    if (count == 0 && serial_escape_state == SERIAL_ESCAPE_GOT_ESC) {
        input_push_key(INPUT_KEY_ESCAPE);
        serial_reset_escape();
    }
}

void input_update(void) {
    usb_keyboard_update();
    uart_input_update();
}

int input_take_global_close_request(void) {
    int requested = usb_global_close_requested;

    usb_global_close_requested = 0;
    return requested;
}
