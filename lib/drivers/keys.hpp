#include <hardware/gpio.h>
#include <initializer_list>
#include <set>
#include <drivers/log.hpp>

struct KeysInfo {
    std::set<uint32_t> pressed;
    std::set<uint32_t> released;
    std::set<uint32_t> state;
};

class Keys;
Keys* storage_keys = nullptr;
static void gpio_irq_callback(uint gpio, uint32_t event_mask);

class Keys {
    friend void gpio_irq_callback(uint gpio, uint32_t event_mask);

public:
    Keys(const std::initializer_list<uint32_t> _keys)
        : keys(_keys) {
        storage_keys = this;
        gpio_set_irq_callback(&gpio_irq_callback);
        for(auto key : keys) {
            gpio_init(key);
            gpio_set_dir(key, GPIO_IN);
            gpio_set_pulls(key, false, false);
            gpio_set_input_hysteresis_enabled(key, true);
            gpio_set_irq_enabled(key, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
        }
        irq_set_enabled(IO_IRQ_BANK0, true);
    }

    bool is_need_update() {
        if(need_update) {
            need_update = false;
            return true;
        }
        return false;
    }

    void poll(void) {
        last_state = current_state;
        current_state = 0;
        for(auto key : keys) {
            current_state |= gpio_get(key) << key;
        }
    }

    KeysInfo get_keys_info() {
        KeysInfo info;
        for(auto key : keys) {
            if(!(current_state & (1 << key)) && (last_state & (1 << key))) {
                info.pressed.insert(key);
            }
            if((current_state & (1 << key)) && !(last_state & (1 << key))) {
                info.released.insert(key);
            }
            if(!(current_state & (1 << key))) {
                info.state.insert(key);
            }
        }
        last_state = current_state;
        return info;
    }

private:
    const std::initializer_list<uint32_t> keys;
    volatile bool need_update = false;
    uint32_t last_state = 0;
    uint32_t current_state = 0;
};

static void gpio_irq_callback(uint gpio, uint32_t event_mask) {
    storage_keys->need_update = true;
}
