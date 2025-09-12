#include "Adc.h"

#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

// Active buffer. 0 = `buf1`, 1 = `buf2`
#define BUF2 0b1
// Buffer 1 is full and ready to be emptied
#define BUF1FULL 0b10
// Buffer 2 is full and ready to be emptied
#define BUF2FULL 0b100
// Only an error if both buffers are full
#define EFULL (BUF1FULL | BUF2FULL)
// Error encountered activating the channel
#define ECHANNEL 0b10000

static struct AdcFrame {
    // Flags field containing potential errors or information about active buf
    volatile uint8_t eflags;
    // Bit resolution to determine whether samples must be one or two bytes
    BitResolution res;
    // Maximum allowed channel index. Stored as `nchannels` - 1 to get rid of a
    // subtraction in the ISR
    uint8_t max_ch_index;
    // Current channel being recorded on. Alternates each sample
    // if `nchannels` > 1.
    volatile uint8_t ch_index;
    // Array of channel definitions
    Channel* channels;
    // Maximum allowed buffer index. Stored like this to save a subtraction.
    size_t max_buf_index;
    // Index into the current buffer
    volatile size_t sample_index;
    // Double buffer which gets swapped once sample_index would exceed `sz`
    // Guaranteed to
    uint8_t* buf1;
    uint8_t* buf2;
    // Total counter of the number of samples collected
    uint32_t collected;
} FRAME;

static struct Sampler {
    uint8_t* buf = nullptr;
    size_t index = 0;
    size_t ch_index = 0;
} SAMPLER;

static int8_t init_frame(BitResolution res, uint8_t nchannels,
                         Channel* channels, uint8_t* buf, size_t sz) {
    memset(&FRAME, 0, sizeof(FRAME));
    FRAME.res = res;
    FRAME.max_ch_index = nchannels - 1;
    FRAME.channels = channels;
    // Slice up the buffer into a double buffer
    size_t bytes_per_sample = res == BitResolution::Eight ? 1 : 2;
    size_t samples_per_buf = sz / (2 * bytes_per_sample);
    FRAME.max_buf_index = samples_per_buf * bytes_per_sample - 1;
    FRAME.buf1 = buf;
    FRAME.buf2 = buf + FRAME.max_buf_index + 1;
    return 0;
}

static inline bool activate(Channel& ch) {
    int8_t mask = ch.mux_mask();
    if (mask < 0) {
        return false;
    }
#define MUX_MASK 0b11111
    ADMUX &= ~MUX_MASK;
    ADMUX |= mask;
#undef MUX_MASK
    // TODO: Set MUX5 in ADCSRB
    return true;
}

ISR(ADC_vect) {
    if ((FRAME.eflags & EFULL) == EFULL) {
        return;
    }
    bool use_buf_1 = !(FRAME.eflags & BUF1FULL);
    uint8_t* buf = use_buf_1 ? FRAME.buf1 : FRAME.buf2;
    if (!activate(FRAME.channels[FRAME.ch_index])) {
        FRAME.eflags |= ECHANNEL;
        return;
    }
    // Only sample high byte if we aren't in 8-bit resolution
    if (FRAME.res != BitResolution::Eight) {
        buf[FRAME.sample_index++] = ADCH;
    }
    buf[FRAME.sample_index] = ADCL;

    // Setup for next sampling
    if (FRAME.sample_index == FRAME.max_buf_index) {
        FRAME.sample_index = 0;
        FRAME.eflags |= use_buf_1 ? BUF1FULL : BUF2FULL;
    } else {
        ++FRAME.sample_index;
    }
    FRAME.ch_index =
        FRAME.ch_index == FRAME.max_ch_index ? 0 : FRAME.ch_index + 1;

    // Accounting information
    FRAME.collected++;
}

int8_t Channel::mux_mask() {
    switch (pin) {
        case A0:
            return 0b000;
        case A1:
            return 0b001;
        case A2:
            return 0b010;
        case A3:
            return 0b011;
        case A4:
            return 0b100;
        case A5:
            return 0b101;
        case A6:
            return 0b110;
        case A7:
            return 0b111;
        default:
            return -1;
    }
}

Adc::~Adc() {
    disable_interrupts();
    disable_autotrigger();
    off();
}

void Adc::on() {
    PRR0 &= ~(1 << PRADC);
    ADCSRA |= 1 << ADEN;
}

void Adc::off() { ADCSRA &= ~(1 << ADEN); }

void Adc::sleep() {
    off();
    PRR0 |= (1 << PRADC);
}

void Adc::enable_interrupts() { ADCSRA |= (1 << ADIE); }
void Adc::disable_interrupts() { ADCSRA &= ~(1 << ADIE); }

void Adc::enable_autotrigger() { ADCSRA |= (1 << ADATE); }
void Adc::disable_autotrigger() { ADCSRA &= ~(1 << ADATE); }

int8_t Adc::start(BitResolution res, uint32_t sample_rate) {
    int8_t rc = init_frame(res, nchannels, channels, buf, sz);
    if (rc) {
        return rc;
    }
    this->res = res;
    // TODO: Actual timer math here
    // Fastest speed for the moment
    ADCSRA &= ~0b111;
    // Start conversion
    ADCSRA |= (1 << ADSC);
    return 0;
}

uint32_t Adc::stop() {
    disable_interrupts();
    disable_autotrigger();
    uint32_t collected = FRAME.collected;
    memset(&FRAME, 0, sizeof(FRAME));
    memset(&SAMPLER, 0, sizeof(SAMPLER));
    return collected;
}
