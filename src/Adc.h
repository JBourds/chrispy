#pragma once
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "WavHeader.h"

enum struct Gain : uint8_t {
    One,
    Ten,
    TwoHundred,
};

struct Channel {
    // Channel being read, or `a` in `a` - `b`
    uint8_t pin;
    // Power pin
    uint8_t power;
    // CURRENTLY UNSUPPORTED
    // < 0 if there is no pin to be differenced again
    int8_t differenced;
    Gain gain;

    int8_t mux_mask();
};

struct Adc {
    const uint8_t nchannels;
    Channel* channels;
    uint8_t* buf;
    const size_t sz;
    BitResolution res;

    Adc(uint8_t _nchannels, Channel* _channels, uint8_t* _buf, size_t _sz)
        : nchannels(_nchannels), channels(_channels), buf(_buf), sz(_sz) {}
    void on();
    void off();
    void sleep();
    void enable_interrupts();
    void disable_interrupts();
    void enable_autotrigger();
    void disable_autotrigger();
    int8_t start(BitResolution res, uint32_t sample_rate);
    uint32_t stop();
};
