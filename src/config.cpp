//
// Created by awalol on 2026/5/4.
//

#include "config.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "audio.h"
#include "bt.h"
#include "button_remap.h"
#include "button_shortcut.h"
#include "button_utils.h"
#include "status_gpio.h"
#include "utils.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/btstack_flash_bank.h"
#include "pico/cyw43_arch.h"
#include "pico/flash.h"

constexpr uint32_t CONFIG_MAGIC = 0x66ccff00;
constexpr uint16_t CONFIG_VERSION = 5; // 如果想要强制重置配置，再更新 CONFIG_VERSION。
constexpr uint32_t CONFIG_FLASH_OFFSET = PICO_FLASH_BANK_STORAGE_OFFSET - FLASH_SECTOR_SIZE;
static ConfigStorage storage{};
static Config &config = storage.config;
bool is_dse = false;

// 编译期保护
// 判断完整配置存储结构是否能放进 flash 256 bytes
static_assert(sizeof(ConfigStorage) <= FLASH_PAGE_SIZE);
// 配置区起始地址必须按 flash sector 对齐。
static_assert(CONFIG_FLASH_OFFSET % FLASH_SECTOR_SIZE == 0);

static uint32_t calc_config_crc(const ConfigStorage &con) {
    return crc32(reinterpret_cast<const uint8_t *>(&con.config.body),
                 sizeof(ConfigStorage) - offsetof(Config, body));
}

static const ConfigStorage *get_xip_addr(uint32_t offset) {
    return reinterpret_cast<const ConfigStorage *>(XIP_BASE + offset);
}

static bool load_old_config(ConfigStorage &s) {
    const auto old_addr = get_xip_addr(PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE);
    uint32_t magic_header;
    memcpy(&magic_header, old_addr, sizeof(uint32_t));
    if (magic_header == CONFIG_MAGIC) {
        printf("[Config] Trying load old sector config\n");
        memset(&s, 0xFF, sizeof(s)); // 先进行 0xFF 填充，确保后续缺项能够正常初始化
        memcpy(&s, old_addr, sizeof(s));
        printf("[Config] Old Config loaded\n");
        return true;
    }
    return false;
}

static void validate_button_remap(ConfigStorage &s) {
    for (uint8_t i = 0; i < BUTTON_REMAP_COUNT; i++) {
        if (s.button.button_remap[i] > Disable) {
            printf("[Config] Button remap value is invalid %u\n", s.button.button_remap[i]);
            s.button.button_remap[i] = i;
        }
    }
}

static void validate_shortcuts(ConfigStorage &s) {
    for (auto &shortcut : s.button.shortcuts) {
        if (!shortcut_slot_valid(shortcut)) {
            // Slot enablement is represented only by trigger_a; normalize every
            // invalid or disabled slot to one deterministic byte representation.
            shortcut.trigger_a = BUTTON_SHORTCUT_DISABLED;
            shortcut.trigger_b = BUTTON_SHORTCUT_DISABLED;
            shortcut.action = ShortcutActionKeyboard;
            shortcut.flags = 0;
            memset(shortcut.payload, 0, sizeof(shortcut.payload));
            continue;
        }
        switch (shortcut.action) {
            case ShortcutActionKeyboard:
                shortcut.keyboard.reserved = 0;
                break;
            case ShortcutActionConsumer:
                shortcut.consumer.reserved = 0;
                break;
            default:
                // Firmware actions do not consume the payload bytes.
                memset(shortcut.payload, 0, sizeof(shortcut.payload));
                break;
        }
    }
}

static void validate_storage(ConfigStorage &s) {
    // valid config and set default value
    if (s.config.magic != CONFIG_MAGIC) {
        if (&s != &storage || !load_old_config(s)) {
            s.config.magic = CONFIG_MAGIC;
            printf("[Config] Config Magic Header is invalid\n");
        }
    }
    if (s.config.size != sizeof(Config_body)) {
        s.config.size = sizeof(Config_body);
        printf("[Config] Config Body size is invalid\n");
    }
    auto *body = &s.config.body;
    if (body->config_version != CONFIG_VERSION) {
        memset(body, 0xFF, sizeof(Config_body));
        body->config_version = CONFIG_VERSION;
        printf("[Config] Warning: Config may breaking change. Reset to default\n");
    }
    if (std::isnan(body->haptics_gain) || body->haptics_gain < 1.0f || body->haptics_gain > 2.0f) {
        body->haptics_gain = 1.0f;
        printf("[Config] Haptics Gain value is invalid\n");
    }
    if (body->speaker_volume > 127) {
        body->speaker_volume = 100;
        printf("[Config] Speaker Volume is invalid\n");
    }
    if (body->headset_volume > 127) {
        body->headset_volume = 100;
        printf("[Config] Headset Volume is invalid\n");
    }
    if (body->speaker_gain > 7) {
        body->speaker_gain = 2;
        printf("[Config] speaker_gain is invalid\n");
    }
    if (body->inactive_time > 60) {
        body->inactive_time = 30;
        printf("[Config] Inactive time is invalid\n");
    }
    if (body->disable_pico_led > 1) {
        body->disable_pico_led = 0;
        printf("[Config] disable_pico_led is invalid\n");
    }
    if (body->polling_rate_mode > 3) {
        body->polling_rate_mode = 1;
        printf("[Config] polling_rate_mode is invalid\n");
    }
    if (body->audio_buffer_length < 16 || body->audio_buffer_length > 128) {
        body->audio_buffer_length = 48;
        printf("[Config] haptics_buffer_length is invalid\n");
    }
    if (body->controller_mode > 2) {
        body->controller_mode = 2;
        printf("[Config] controller_mode is invalid\n");
    }
    if (body->enable_usb_sn > 1) {
        body->enable_usb_sn = 0;
        printf("[Config] Warning: enable_usb_sn is invalid\n");
    }
    if (body->enable_keyboard > 1) {
        body->enable_keyboard = 0;
        printf("[Config] enable_keyboard is invalid\n");
    }
    if (body->mic_select > 3) {
        body->mic_select = 0;
        printf("[Config] mic_select is invalid\n");
    }
    if (body->speaker_select > 3) {
        body->speaker_select = 0;
        printf("[Config] speaker_select is invalid\n");
    }
    if (body->enable_wake > 1) {
        body->enable_wake = 0;
        printf("[Config] enable_wake is invalid\n");
    }
    if (body->trigger_reduce > 10) {
        body->trigger_reduce = 0;
        printf("[Config] trigger_reduce is invalid\n");
    }
    if (body->lock_volume > 2) {
        body->lock_volume = 0;
        printf("[Config] lock_volume is invalid\n");
    }
    if (!status_gpio_pin_valid(body->status_gpio_pin)) {
        body->status_gpio_pin = STATUS_GPIO_DISABLED;
        printf("[Config] status_gpio_pin is invalid\n");
    }
    if (body->status_gpio_mode > STATUS_GPIO_MODE_BUTTON) {
        body->status_gpio_mode = 0;
        printf("[Config] status_gpio_mode is invalid\n");
    }
    if (body->packet_mode > 1) {
        body->packet_mode = 0;
        printf("[Config] packet_mode is invalid\n");
    }
    if (body->disconnect_on_dock > 1) {
        body->disconnect_on_dock = 0;
        printf("[Config] disconnect_on_dock is invalid\n");
    }
    if (body->sleep_on_dock > 1) {
        body->sleep_on_dock = 0;
        printf("[Config] sleep_on_dock is invalid\n");
    }
    validate_button_remap(s);
    validate_shortcuts(s);
}

void config_valid() {
    validate_storage(storage);
}

void button_remap_valid() {
    validate_button_remap(storage);
}

void shortcut_valid() {
    validate_shortcuts(storage);
}

void config_load() {
    memcpy(&storage, get_xip_addr(CONFIG_FLASH_OFFSET), sizeof(storage));

    if (storage.config.magic != CONFIG_MAGIC) {
        if (!load_old_config(storage)) {
            memset(&storage, 0xFF, sizeof(storage));
            storage.config.magic = CONFIG_MAGIC;
            printf("[Config] Config Magic Header is invalid\n");
        }
    }

    if (storage.config.magic == CONFIG_MAGIC &&
        storage.config.size == sizeof(Config_body) &&
        storage.config.body.config_version == CONFIG_VERSION) {
        if (calc_config_crc(storage) != storage.config.crc32) {
            printf("[Config] Warning: Flash CRC32 mismatch, resetting to defaults\n");
            memset(&storage, 0xFF, sizeof(storage));
            storage.config.magic = CONFIG_MAGIC;
        }
    }

    config_valid();
    refresh_remapped_report();
}

// Runs with core1 parked (flash_safe_execute) and core0 interrupts disabled, so
// neither core touches XIP flash while the sector is erased/programmed. Without
// the core1 park this races the audio core and corrupts audio (buzzing).
static void config_save_flash_op(void *param) {
    const auto *page = static_cast<const uint8_t *>(param);
    const uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CONFIG_FLASH_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);
}

bool config_save() {
    config.crc32 = calc_config_crc(storage);
    alignas(4) uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xff, sizeof(page));
    memcpy(page, &storage, sizeof(storage));

#if !ENABLE_SERIAL
    watchdog_update();
#endif
    const int rc = flash_safe_execute(config_save_flash_op, page, 500);
#if !ENABLE_SERIAL
    watchdog_update();
#endif
    if (rc != PICO_OK) {
        printf("[Config] config_save flash_safe_execute failed: %d\n", rc);
        return false;
    }

    ConfigStorage verify{};
    memcpy(&verify, get_xip_addr(CONFIG_FLASH_OFFSET), sizeof(verify));
    const auto verify_crc32 = calc_config_crc(verify);
    if (verify_crc32 == config.crc32) {
        printf("[Config] Config write flash verify success\n");
        return true;
    }
    printf("[Config] Config write flash verify failed\n");
    return false;
}

Config_body& get_config() {
    return config.body;
}

Button& get_button() {
    return storage.button;
}

void set_button_remap(const uint8_t *new_remap, const uint16_t len) {
    const auto copy_len = len < BUTTON_REMAP_COUNT ? len : BUTTON_REMAP_COUNT;
    memcpy(storage.button.button_remap, new_remap, copy_len);
    button_remap_valid();
    refresh_remapped_report();
}

void set_shortcut(const uint8_t *new_shortcuts, const uint16_t len) {
    button_shortcut_reset();
    constexpr auto room = sizeof(Button::shortcuts);
    const auto copy_len = len < room ? len : room;
    memcpy(storage.button.shortcuts, new_shortcuts, copy_len);
    shortcut_valid();
}

static void apply_validated_config(ConfigStorage &next) {
    validate_storage(next);
    const uint8_t validated_mic = next.config.body.mic_select;
    const uint8_t validated_spk = next.config.body.speaker_select;
    next.config.body.mic_select = config.body.mic_select;
    next.config.body.speaker_select = config.body.speaker_select;
    config.body = next.config.body;
    __atomic_store_n(&config.body.mic_select, validated_mic, __ATOMIC_RELAXED);
    __atomic_store_n(&config.body.speaker_select, validated_spk, __ATOMIC_RELAXED);
    storage.button = next.button;
    refresh_remapped_report();
}

void set_config(const uint8_t *new_config, const uint16_t len) {
    const bool controller_connected = bt_is_connected();
    gpio_on_disconnect();

    const uint8_t prev_sleep_on_dock = config.body.sleep_on_dock;
    const uint8_t prev_mic_select = config.body.mic_select;
    ConfigStorage next = storage;
    const auto copy_len = len < sizeof(Config_body) ? len : sizeof(Config_body);
    memcpy(&next.config.body, new_config, copy_len);
    // If set from legacy tool/web UI (which only sends 24 bytes), preserve sleep_on_dock
    if (len <= 24) {
        next.config.body.sleep_on_dock = prev_sleep_on_dock;
    }
    apply_validated_config(next);

    const bool mic_changed = (prev_mic_select != config.body.mic_select);

    if (controller_connected && config.body.status_gpio_mode != STATUS_GPIO_MODE_BUTTON) {
        gpio_on_connect();
    } else {
        gpio_on_disconnect();
    }

    if (config.body.disable_pico_led) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    } else if (controller_connected) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
    }

    if (controller_connected) {
        SetStateData state{};
        if (config.body.trigger_reduce > 0) {
            state.AllowMotorPowerLevel = 1;
            state.TriggerMotorPowerReduction = config.body.trigger_reduce;
        }
        if (config.body.speaker_gain > 0) {
            state.AllowAudioControl2 = 1;
            state.SpeakerCompPreGain = config.body.speaker_gain;
        }
        state.AllowAudioControl = 1;
        state.MicSelect = config.body.mic_select;
        state.NoiseCancelEnable = 1;
        update_state(state);
        if (mic_changed) {
            update_mic_status();
        }
    }
}

void set_config(const Config_body &new_config) {
    const bool controller_connected = bt_is_connected();
    gpio_on_disconnect();

    ConfigStorage next = storage;
    next.config.body = new_config;
    apply_validated_config(next);

    if (controller_connected && config.body.status_gpio_mode != STATUS_GPIO_MODE_BUTTON) {
        gpio_on_connect();
    } else {
        gpio_on_disconnect();
    }
}
