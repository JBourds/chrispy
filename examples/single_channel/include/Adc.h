#pragma once
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "../../../include/WavHeader.h"

namespace adc {

enum struct Gain : uint8_t {
    One,
    Ten,
    TwoHundred,
};

struct Channel {
    // Channel being read, or `a` in `a` - `b`
    uint8_t pin;
    // CURRENTLY UNSUPPORTED
    // < 0 if there is no pin to be differenced again
    int8_t differenced;
    Gain gain;

    int8_t mux_mask();
};

struct Adc {
    const size_t sz;
    const uint8_t* buf;
    const uint8_t channel_count;
    const Channel* const channels;

    Adc(size_t _sz, uint8_t* _buf, uint8_t _channel_count, Channel* _channels)
        : sz(_sz),
          buf(_buf),
          channel_count(_channel_count),
          channels(_channels) {}
    void on();
    void off();
    void sleep();
    void enable_interrupts();
    void disable_interrupts();
    void sample();
};
}  // namespace adc
