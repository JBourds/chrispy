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

    inline int8_t mux_mask();
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

    /**
     * Try to return the next sample and mark which channel
     * it came from. If there is no sample, return a negative
     * value. int16_t is valid since we don't use the MSB anyways
     * when sampling 10-bit audio.
     *
     * @param ch_index: Reference var which gets set to the channel the
     * sample is from (if there is a sample).
     *
     * @returns (int16_t): Negative if there is no sample. The sampled
     * value otherwise.
     */
    int16_t next_sample(size_t& ch_index);
    void swap_buffer();
    uint32_t stop();
};
