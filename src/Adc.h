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
     * @param ch_window_sz: Size of each channel's window. Only checked when
     * there are multiple channels being recorded from. Defaults to 8.
     * @param warmup_ms: Milliseconds to delay after starting ADC before
     * ingesting samples. A small warmup helps prevent poor signal from
     * channel switching noise.
     *
     * @returns (int8_t): Return code. 0 if all is good, negative otherwise.
     */
    int8_t start(BitResolution res, uint32_t sample_rate,
                 size_t ch_window_sz = 8, uint32_t warmup_ms = 100);

    /**
     * Retrieve a buffer from the module belonging to a specific channel,
     * writing the size and channel index to the reference arguments.
     *
     * Intent: Call this in a busy loop starting with a nullptr and write data
     * out to the appropriate channel as it comes in.
     *
     * @param buf: Pointer to buffer being swapped. If it is a null-pointer,
     * will try to swap it with a currently full buffer without marking it as
     * usable by the ISR. If it is one of the double buffers, will free the
     * buffer for use again and try to exchange it for the other buffer if
     * possible.
     * @param sz: Out-parameter for the number of bytes in the buffer.
     * @param ch_index: Out-paramter for the channel index this data is from.
     *
     * @returns (int8_t): 0 if a new buffer is returned. Nonzero otherwise.
     */
    int8_t swap_buffer(uint8_t** buf, size_t& sz, size_t& ch_index);

    /**
     * NOT SAFE TO USE WHEN THE ADC IS ENABLED!
     *
     * Identical API as `swap_buffer` but this will also give the active buffer.
     * Used to drain any remaining samples from the buffer.
     */
    int8_t drain_buffer(uint8_t** buf, size_t& sz, size_t& ch_index);

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
