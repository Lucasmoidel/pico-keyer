
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"

#ifdef USB_KEYER
const char* hid_string_descriptor[5] = { (char[]) { 0x09, 0x04 },"TinyUSB","TinyUSB Device","123456","Example HID interface", };
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) { return 0; }
const uint8_t hid_report_descriptor[] = { TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)) };
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) { return hid_report_descriptor; }
#endif

enum interface_count {
    #ifdef KEYBOARD_KEYER
    ITF_NUM_HID,
    #endif
    #ifdef MIDI_KEYER
    ITF_NUM_MIDI,
    ITF_NUM_MIDI_STREAMING,
    #endif
    ITF_COUNT
};
enum usb_endpoints {
    EP_EMPTY = 0,
    #ifdef KEYBOARD_KEYER
    EPNUM_HID,
    #endif
    #ifdef MIDI_KEYER
    EPNUM_MIDI,
    #endif
};
#if defined(KEYBOARD_KEYER) && defined(MIDI_KEYER)
#define TUSB_DESCRIPTOR_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_MIDI_DESC_LEN)
static const uint8_t hid_configuration_descriptor[] = { TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),TUD_HID_DESCRIPTOR(ITF_NUM_HID, 4, false, sizeof(hid_report_descriptor), (0x80 | EPNUM_HID), 16, 10), TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64) };
#elif defined(KEYBOARD_KEYER)
#define TUSB_DESCRIPTOR_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t hid_configuration_descriptor[] = { TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),TUD_HID_DESCRIPTOR(ITF_NUM_HID, 4, false, sizeof(hid_report_descriptor), (0x80 | EPNUM_HID), 16, 10) };
#elif defined(MIDI_KEYER)
#define TUSB_DESCRIPTOR_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)
static const uint8_t hid_configuration_descriptor[] = { TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100),TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64) };
#endif

int initTinyUSB() {
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    return true;
}