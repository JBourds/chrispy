#pragma once
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "WavHeader.h"

struct Channel {
    // Channel being read, or `a` in `a` - `b`
    uint8_t pin;
    // Power pin. If negative, indicates no power is needed.
    int8_t power;

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
    void set_frequency(uint32_t sample_rate);
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
    /**
     * Attempt to swap the buffer in the argument for a new
     * one. If `buf` = nullptr, try to give a full buffer.
     * Otherwise, check if it is one of the two buffers in the
     * recording frame. If it is, mark that it is no longer full.
     *
     * Intent: Call this in a loop starting with a nullptr.
     *
     * @param buf: Pointer to buffer being swapped. If it is a null-pointer,
     * will try to swap it with a currently full buffer without marking it as
     * usable by the ISR. If it is one of the double buffers, will free the
     * buffer for use again and try to exchange it for the other buffer if
     * possible.
     * @param sz: Out-parameter for the number of bytes in the buffer.
     *
     * @returns (int8_t): 0 if success. Nonzero otherwise.
     */
    int8_t swap_buffer(uint8_t** buf, size_t& sz);
    uint32_t stop();
};
