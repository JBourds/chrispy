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

// Bit masks
#define MUX_MASK 0b11111
#define LOW_CHANNEL_MASK 0b111
#define HIGH_CHANNEL_MASK ~LOW_CHANNEL_MASK
#define PRESCALER_MASK 0b111

// Prescaler masks
#define DIV_128 0b111
#define DIV_64 0b110
#define DIV_32 0b101
#define DIV_16 0b100
#define DIV_8 0b011
#define DIV_4 0b010
#define DIV_2 0b001
#define DIV_2_2 0b000

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
    // Buffer actively being written to. Only gets swapped when capacity is hit.
    volatile uint8_t* current;
    // Total counter of the number of samples collected
    volatile uint32_t collected;
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
    FRAME.current = FRAME.buf1 = buf;
    FRAME.buf2 = buf + FRAME.max_buf_index + 1;
    return 0;
}

static inline bool activate(Channel& ch) {
    int8_t mask = ch.mux_mask();
    if (mask < 0) {
        return false;
    }
    ADMUX &= ~MUX_MASK;
    ADMUX |= mask;
    // If the mask comes from a higher channel (>7) set MUX5
    if (mask & HIGH_CHANNEL_MASK) {
        ADCSRB |= (1 << MUX5);
    } else {
        ADCSRB &= ~(1 << MUX5);
    }
    return true;
}

ISR(ADC_vect) {
    if ((FRAME.eflags & EFULL) == EFULL) {
        return;
    }

    if (!activate(FRAME.channels[FRAME.ch_index])) {
        FRAME.eflags |= ECHANNEL;
        return;
    }
    // Only sample high byte if we are in > 8-bit resolution
    if (FRAME.res != BitResolution::Eight) {
        FRAME.current[FRAME.sample_index++] = ADCL;
    }
    FRAME.current[FRAME.sample_index] = ADCH;

    // Hit the end of this buffer.
    // Because we didn't early return at the beginning, we know the other buffer
    // is available. Mark in `eflags` this buffer is ready to write to then swap
    // the buffers.
    if (FRAME.sample_index == FRAME.max_buf_index) {
        FRAME.sample_index = 0;
        bool use_buf_1 = !(FRAME.eflags & BUF1FULL);
        FRAME.eflags |= use_buf_1 ? BUF1FULL : BUF2FULL;
        FRAME.current = use_buf_1 ? FRAME.buf1 : FRAME.buf2;
    } else {
        ++FRAME.sample_index;
    }
    // Wrapping channel index
    FRAME.ch_index =
        FRAME.ch_index == FRAME.max_ch_index ? 0 : FRAME.ch_index + 1;

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

void Adc::on() {
    PRR0 &= ~(1 << PRADC);
    ADCSRA |= (1 << ADEN);
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
    for (size_t i = 0; i < nchannels; ++i) {
        if (channels[i].power >= 0) {
            pinMode(channels[i].power, OUTPUT);
            digitalWrite(channels[i].power, HIGH);
        }
    }
    // TODO: Actual timer math here
    // Fastest speed for the moment
    ADCSRA &= ~PRESCALER_MASK;
    ADCSRA |= DIV_64;

    // Set reference voltage to analog 5V
    ADMUX &= ~((1 << REFS0) | (1 << REFS1));
    ADMUX |= (1 << REFS0);
    // Start conversion
    ADCSRA |= (1 << ADSC);
    return 0;
}

int8_t Adc::swap_buffer(uint8_t** buf, size_t& sz) {
    if (buf == nullptr) {
        return -1;
    }
    if (*buf == nullptr) {
        if (FRAME.eflags & BUF1FULL) {
            *buf = FRAME.buf1;
        } else if (FRAME.eflags & BUF2FULL) {
            *buf = FRAME.buf2;
        }
    } else {
        if (*buf == FRAME.buf1) {
            noInterrupts();
            FRAME.eflags ^= BUF1FULL;
            interrupts();
            *buf = FRAME.eflags & BUF2FULL ? FRAME.buf2 : nullptr;
        } else if (*buf == FRAME.buf2) {
            noInterrupts();
            FRAME.eflags ^= BUF2FULL;
            interrupts();
            *buf = FRAME.eflags & BUF1FULL ? FRAME.buf1 : nullptr;
        } else {
            return -2;
        }
    }
    sz = FRAME.max_buf_index + 1;
    return 0;
}

int16_t Adc::next_sample(size_t& ch_index) {
    // Try to get one of the buffers
    // It's okay if an interrupt fires during this since the ISR will never
    // unset this flag.
    if (SAMPLER.buf == nullptr) {
        if (FRAME.eflags & BUF1FULL) {
            SAMPLER.buf = FRAME.buf1;
        } else if (FRAME.eflags & BUF2FULL) {
            SAMPLER.buf = FRAME.buf2;
        }
    }
    if (SAMPLER.buf == nullptr) {
        return -1;
    }
    int16_t val = 0;
    // May have a high byte we sample first
    if (res != BitResolution::Eight) {
        val |= (SAMPLER.buf[SAMPLER.index++] << CHAR_BIT);
    }
    // Low byte
    val |= SAMPLER.buf[SAMPLER.index];
    // Update internal channel counter
    SAMPLER.ch_index =
        SAMPLER.ch_index == FRAME.max_ch_index ? 0 : SAMPLER.ch_index + 1;
    ch_index = SAMPLER.ch_index;
    // Exhausted a buffer, reset sampler state and signal that
    // the frame can reuse the buffer
    if (SAMPLER.index == FRAME.max_buf_index) {
        SAMPLER.buf = nullptr;
        SAMPLER.index = 0;
        noInterrupts();
        FRAME.eflags ^= SAMPLER.buf == FRAME.buf1 ? BUF1FULL : BUF2FULL;
        interrupts();
    } else {
        SAMPLER.index++;
    }
    return val;
}

uint32_t Adc::stop() {
    disable_interrupts();
    disable_autotrigger();
    uint32_t collected = FRAME.collected;
    memset(&FRAME, 0, sizeof(FRAME));
    memset(&SAMPLER, 0, sizeof(SAMPLER));
    for (size_t i = 0; i < nchannels; ++i) {
        if (channels[i].power >= 0) {
            digitalWrite(channels[i].power, LOW);
        }
    }
    return collected;
}
