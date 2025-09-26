#pragma once
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "Timer.h"
#include "WavHeader.h"

struct Channel {
    // Channel being read, or `a` in `a` - `b`
    uint8_t pin;
    // Power pin. If negative, indicates no power is needed.
    int8_t power;

    // Get bit pattern for mux mask.
    inline int8_t mux_mask();
};

enum struct AdcSource : uint8_t {
    FreeRunning = 0b000,
    AnalogComparator = 0b001,
    ExternalIrq0 = 0b010,
    TimCnt0CmpA = 0b011,
    TimCnt0Ovf = 0b100,
    TimCnt1CmpB = 0b101,
    TimCnt1Ovf = 0b110,
    TimCnt1Cap = 0b111,
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

    /**
     * Set the autotrigger source.
     * @param src: Which source to use for auto triggering.
     */
    void set_source(AdcSource src);

    /**
     * Helper function to setup the hardware timer and ADC clocks for sampling.
     *
     * @param sample_rate: Sampling frequency in Hz.
     *
     * @returns (TimerRc): Return code. `TimerRc::Okay` if both succeed.
     */
    TimerRc set_frequency(uint32_t sample_rate);

    /**
     * Start ADC sampling at a certain rate with a given bit resolution.
     *
     * @param res: Bit resolution to use.
     * @param sample_rate: Sample rate in Hz to try and record at,
     *
     * @returns (int8_t): Return code. 0 if all is good, negative otherwise.
     */
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

    /**
     * NOT SAFE TO USE WHEN THE ADC IS ENABLED!
     * Identical API as `swap_buffer` but this will also give the active buffer.
     * Used to drain any remaining samples from the buffer.
     */
    int8_t drain_buffer(uint8_t** buf, size_t& sz);

    /**
     * @returns (uint32_t): Number of samples collected in the current/previous
     * round of sampling.
     */
    uint32_t collected();

    /**
     * Stops ADC sampling and performs cleanup on registers.
     * @returns (uint32_t): Number of samples collected.
     */
    uint32_t stop();
};
