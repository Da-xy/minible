/*!  \file     emu_fido_hid.c
*    \brief    Virtual FIDO (CTAPHID) USB device for the desktop emulator, using Linux uhid
*    \note     Compiled against the AUX MCU headers (see Makefile.emu). A single thread owns the uhid
*              descriptor and calls the aux CTAP code: reports written by the host (UHID_OUTPUT) are fed
*              to ctaphid_handle_packet(), and the packets the CTAP code sends are written back as input
*              reports from emu_fido_set_hid_tx_callback()'s callback, in that same thread.
*/
#include "emu_fido.h"

#ifdef __linux__

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uhid.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ctaphid.h"

/* Identification of the virtual device. The product ID differs from the real Mooltipass one so that
 * Moolticute and other tools looking for the real device do not mistake it for hardware. */
#define EMU_FIDO_USB_VENDOR_ID      0x1209
#define EMU_FIDO_USB_PRODUCT_ID     0x4322
#define EMU_FIDO_USB_VERSION        0x0001
#define EMU_FIDO_DEVICE_NAME        "Mooltipass Mini BLE emulator FIDO"
#define EMU_FIDO_UHID_PATH          "/dev/uhid"

/* How long poll() waits before checking whether we have been asked to stop, in ms */
#define EMU_FIDO_POLL_TIMEOUT_MS    100

/* Number of 32-bit words in a CTAPHID packet */
#define EMU_FIDO_PACKET_WORDS       (EMU_FIDO_HID_PACKET_SIZE / sizeof(uint32_t))

/* HID report descriptor, same as the one of the real device (aux_mcu/src/USB/usb_descriptors.c) */
static const uint8_t ctap_hid_report_desc[] =
{
    0x06, 0xD0, 0xF1,                   // Usage Page (0xF1D0, FIDO alliance)
    0x09, 0x01,                         // Usage (FIDO_USAGE_CTAP_HID)
    0xA1, 0x01,                         // Collection (Application)
    0x09, 0x20,                         //   Usage (FIDO_USAGE_DATA_IN)
    0x15, 0x00,                         //   Logical Minimum (0)
    0x26, 0xFF, 0x00,                   //   Logical Maximum (0xFF)
    0x75, 0x08,                         //   Report Size (8)
    0x95, 0x40,                         //   Report Count (64)
    0x81, 0x02,                         //   Input (Data, Variable, Absolute)
    0x09, 0x21,                         //   Usage (FIDO_USAGE_DATA_OUT)
    0x15, 0x00,                         //   Logical Minimum (0)
    0x26, 0xFF, 0x00,                   //   Logical Maximum (0xFF)
    0x75, 0x08,                         //   Report Size (8)
    0x95, 0x40,                         //   Report Count (64)
    0x91, 0x02,                         //   Output (Data, Variable, Absolute)
    0xC0                                // End Collection
};

static int uhid_fd = -1;
static pthread_t bridge_thread;
static volatile int bridge_running = 0;
static volatile int bridge_stop_requested = 0;

/*! \fn     emu_fido_uhid_write(const struct uhid_event* event)
*   \brief  Write an event to the uhid descriptor
*   \param  event   Event
*   \return 0 on success, -1 on error
*/
static int emu_fido_uhid_write(const struct uhid_event* event)
{
    ssize_t written = write(uhid_fd, event, sizeof(*event));
    if (written != (ssize_t)sizeof(*event))
    {
        fprintf(stderr, "emu_fido: uhid write failed: %s\n", written < 0 ? strerror(errno) : "short write");
        return -1;
    }
    return 0;
}

/*! \fn     emu_fido_send_to_host(const uint8_t* packet, uint16_t length)
*   \brief  Send a CTAPHID packet to the host as an input report
*   \param  packet  Packet
*   \param  length  Packet length
*/
static void emu_fido_send_to_host(const uint8_t* packet, uint16_t length)
{
    struct uhid_event event;

    if (length > sizeof(event.u.input2.data))
    {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.type = UHID_INPUT2;
    event.u.input2.size = length;
    memcpy(event.u.input2.data, packet, length);
    emu_fido_uhid_write(&event);
}

/*! \fn     emu_fido_create_device(void)
*   \brief  Ask the kernel to create the virtual HID device
*   \return 0 on success, -1 on error
*/
static int emu_fido_create_device(void)
{
    struct uhid_event event;

    memset(&event, 0, sizeof(event));
    event.type = UHID_CREATE2;
    strncpy((char*)event.u.create2.name, EMU_FIDO_DEVICE_NAME, sizeof(event.u.create2.name) - 1);
    event.u.create2.rd_size = sizeof(ctap_hid_report_desc);
    memcpy(event.u.create2.rd_data, ctap_hid_report_desc, sizeof(ctap_hid_report_desc));
    event.u.create2.bus = BUS_USB;
    event.u.create2.vendor = EMU_FIDO_USB_VENDOR_ID;
    event.u.create2.product = EMU_FIDO_USB_PRODUCT_ID;
    event.u.create2.version = EMU_FIDO_USB_VERSION;
    event.u.create2.country = 0;
    return emu_fido_uhid_write(&event);
}

/*! \fn     emu_fido_handle_event(void)
*   \brief  Read one uhid event and process it
*   \return 0 to carry on, -1 if the descriptor is unusable
*/
static int emu_fido_handle_event(void)
{
    uint32_t packet[EMU_FIDO_PACKET_WORDS];
    struct uhid_event event;
    ssize_t nb_read;

    memset(&event, 0, sizeof(event));
    nb_read = read(uhid_fd, &event, sizeof(event));
    if (nb_read != (ssize_t)sizeof(event))
    {
        fprintf(stderr, "emu_fido: uhid read failed: %s\n", nb_read < 0 ? strerror(errno) : "short read");
        return -1;
    }

    switch (event.type)
    {
        case UHID_OUTPUT:
        {
            const uint8_t* data = event.u.output.data;
            uint16_t size = event.u.output.size;

            /* Some kernels keep the (zero) report ID in front of the report */
            if ((size == EMU_FIDO_HID_PACKET_SIZE + 1) && (data[0] == 0))
            {
                data++;
                size--;
            }
            if (size == EMU_FIDO_HID_PACKET_SIZE)
            {
                memcpy(packet, data, sizeof(packet));
                ctaphid_handle_packet(packet);
            }
            else
            {
                fprintf(stderr, "emu_fido: ignoring output report of %u bytes\n", (unsigned)size);
            }
            break;
        }
        case UHID_GET_REPORT:
        case UHID_SET_REPORT:
        {
            /* Feature reports are not used by CTAPHID: answer with an error so the host does not wait */
            struct uhid_event reply;
            memset(&reply, 0, sizeof(reply));
            if (event.type == UHID_GET_REPORT)
            {
                reply.type = UHID_GET_REPORT_REPLY;
                reply.u.get_report_reply.id = event.u.get_report.id;
                reply.u.get_report_reply.err = EIO;
            }
            else
            {
                reply.type = UHID_SET_REPORT_REPLY;
                reply.u.set_report_reply.id = event.u.set_report.id;
                reply.u.set_report_reply.err = EIO;
            }
            emu_fido_uhid_write(&reply);
            break;
        }
        case UHID_STOP:
        {
            fprintf(stderr, "emu_fido: uhid device stopped by the kernel\n");
            return -1;
        }
        default: break;
    }
    return 0;
}

/*! \fn     emu_fido_thread(void* arg)
*   \brief  Thread serving the virtual device until asked to stop
*   \param  arg Unused
*   \return NULL
*/
static void* emu_fido_thread(void* arg)
{
    struct pollfd poll_fd;
    (void)arg;

    poll_fd.fd = uhid_fd;
    poll_fd.events = POLLIN;
    while (bridge_stop_requested == 0)
    {
        int poll_result = poll(&poll_fd, 1, EMU_FIDO_POLL_TIMEOUT_MS);
        if (poll_result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            fprintf(stderr, "emu_fido: poll failed: %s\n", strerror(errno));
            break;
        }
        if ((poll_result > 0) && ((poll_fd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0))
        {
            break;
        }
        if ((poll_result > 0) && (emu_fido_handle_event() != 0))
        {
            break;
        }
    }
    return NULL;
}

/*! \fn     emu_fido_start(void)
*   \brief  Create the virtual FIDO HID device and start the thread serving it
*   \return 0 if started, -1 on error (reason printed on stderr)
*/
int emu_fido_start(void)
{
    if (bridge_running != 0)
    {
        return 0;
    }

    uhid_fd = open(EMU_FIDO_UHID_PATH, O_RDWR | O_CLOEXEC);
    if (uhid_fd < 0)
    {
        fprintf(stderr, "emu_fido: cannot open %s: %s\n", EMU_FIDO_UHID_PATH, strerror(errno));
        if (errno == EACCES)
        {
            fprintf(stderr, "emu_fido: your user needs access to it, see IMPLEMENTATION.md (udev rule)\n");
        }
        else if (errno == ENODEV)
        {
            fprintf(stderr, "emu_fido: the uhid kernel module is not loaded (sudo modprobe uhid)\n");
        }
        return -1;
    }

    if (emu_fido_create_device() != 0)
    {
        close(uhid_fd);
        uhid_fd = -1;
        return -1;
    }

    ctaphid_init();
    emu_fido_set_hid_tx_callback(emu_fido_send_to_host);

    bridge_stop_requested = 0;
    if (pthread_create(&bridge_thread, NULL, emu_fido_thread, NULL) != 0)
    {
        fprintf(stderr, "emu_fido: cannot create thread\n");
        emu_fido_set_hid_tx_callback(NULL);
        close(uhid_fd);
        uhid_fd = -1;
        return -1;
    }
    bridge_running = 1;
    fprintf(stderr, "emu_fido: virtual FIDO device created (%04x:%04x)\n", EMU_FIDO_USB_VENDOR_ID, EMU_FIDO_USB_PRODUCT_ID);
    return 0;
}

/*! \fn     emu_fido_stop(void)
*   \brief  Stop the thread started by emu_fido_start() and remove the virtual HID device
*/
void emu_fido_stop(void)
{
    struct uhid_event event;

    if (bridge_running == 0)
    {
        return;
    }
    bridge_stop_requested = 1;
    pthread_join(bridge_thread, NULL);
    emu_fido_set_hid_tx_callback(NULL);

    memset(&event, 0, sizeof(event));
    event.type = UHID_DESTROY;
    emu_fido_uhid_write(&event);
    close(uhid_fd);
    uhid_fd = -1;
    bridge_running = 0;
}

#else /* not Linux: uhid is not available */

#include <stdio.h>

int emu_fido_start(void)
{
    fprintf(stderr, "emu_fido: the virtual FIDO device is only supported on Linux\n");
    return -1;
}

void emu_fido_stop(void)
{
}

#endif
