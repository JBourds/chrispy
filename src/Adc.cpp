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
#define SOURCE_MASK 0b111
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

#define CYCLES_PER_SAMPLE 13.5
#define ADC_MAX_FREQ 200000

static const size_t NPRESCALERS = 7;

static const pre_t PRESCALERS[] = {2, 4, 8, 16, 32, 64, 128};

static uint8_t prescaler_mask(pre_t val) {
    switch (val) {
        case 2:
            return DIV_2;
        case 4:
            return DIV_4;
        case 8:
            return DIV_8;
        case 16:
            return DIV_16;
        case 32:
            return DIV_32;
        case 64:
            return DIV_64;
        case 128:
            return DIV_128;
        default:
            return 0;
    }
}

static struct AdcFrame {
    // Flags used to communicate state of sampling
    volatile uint8_t eflags;
    // Bit resolution being used for sampling
    BitResolution res;
    // Array of channels being swapped betwee.
    Channel* channels;
    // Length of `channels` array
    size_t nchannels;
    // Current channel index
    volatile size_t ch_index;
    // Double buffers being swapped between
    uint8_t* buf1;
    uint8_t* buf2;
    // Number of bytes in each buffer (guaranteed to be an increment of the
    // number of bytes needed per sample)
    size_t buf_sz;
    // Current sample index in the active buffer
    volatile size_t sample_index;
    // Accounting information on # samples collected
    volatile uint32_t collected;
} FRAME;

static int8_t init_frame(BitResolution res, uint8_t nchannels,
                         Channel* channels, uint8_t* buf, size_t sz) {
    memset(&FRAME, 0, sizeof(FRAME));
    // Slice up the buffer into a double buffer
    size_t bytes_per_sample = res == BitResolution::Eight ? 1 : 2;
    size_t samples_per_buf = sz / (2 * bytes_per_sample);
    FRAME.channels = channels;
    FRAME.nchannels = nchannels;
    FRAME.res = res;
    FRAME.buf_sz = samples_per_buf * bytes_per_sample;
    FRAME.buf1 = buf;
    FRAME.buf2 = buf + FRAME.buf_sz;
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

ISR(ADC_vect) {
    if ((FRAME.eflags & EFULL) == EFULL) {
        return;
    }
    bool use_buf_1 = !(FRAME.eflags & BUF1FULL);
    uint8_t* buf = use_buf_1 ? FRAME.buf1 : FRAME.buf2;
    if (FRAME.res != BitResolution::Eight) {
        buf[FRAME.sample_index++] = ADCL;
    }
    buf[FRAME.sample_index++] = ADCH;
    if (FRAME.sample_index == FRAME.buf_sz) {
        FRAME.eflags |= (use_buf_1 ? BUF1FULL : BUF2FULL);
        FRAME.sample_index = 0;
    }
    ++FRAME.collected;
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

void Adc::set_source(enum AdcSource src) {
    ADCSRB &= ~SOURCE_MASK;
    ADCSRB |= static_cast<uint8_t>(src);
}

TimerRc Adc::set_frequency(uint32_t sample_rate) {
    TimerConfig host_cfg(F_CPU, sample_rate, Skew::High);
    TimerRc rc = activate_t1(host_cfg);
    if (rc != TimerRc::Okay && rc != TimerRc::ErrorRange) {
        return rc;
    }

    // Set overflow match on A and B so count resets (uses A) and triggers
    // interrupt when it does so (match on B)
    cli();
    OCR1A = host_cfg.compare;
    OCR1B = host_cfg.compare;
    sei();

    uint32_t adc_rate = CYCLES_PER_SAMPLE * sample_rate;
    if (adc_rate > ADC_MAX_FREQ) {
        return TimerRc::ImpossibleClock;
    }
    TimerConfig adc_cfg(F_CPU, adc_rate, Skew::High);
    rc = adc_cfg.compute(NPRESCALERS, PRESCALERS, 1, 0.0);
    if (rc != TimerRc::Okay && rc != TimerRc::ErrorRange) {
        return rc;
    }
    uint8_t prescaler = prescaler_mask(adc_cfg.prescaler);
    ADCSRA &= ~PRESCALER_MASK;
    ADCSRA |= prescaler;
}

int8_t Adc::start(BitResolution res, uint32_t sample_rate) {
    int8_t rc = init_frame(res, nchannels, channels, buf, sz);
    if (rc) {
        return rc;
    }
    this->res = res;

    on();
    enable_interrupts();
    enable_autotrigger();
    set_source(AdcSource::TimCnt1CmpB);
    set_frequency(sample_rate);
    ADMUX = 1 << REFS0;
    // Left adjust result so we can just read from ADCH in ISR
    if (res == BitResolution::Eight) {
        ADMUX |= (1 << ADLAR);
    }
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
        } else {
            return -2;
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
            return -3;
        }
    }
    sz = FRAME.buf_sz;
    return 0;
}

uint32_t Adc::stop() {
    off();
    disable_interrupts();
    disable_autotrigger();
    deactivate_t1();
    uint32_t collected = FRAME.collected;
    memset(&FRAME, 0, sizeof(FRAME));
    for (size_t i = 0; i < nchannels; ++i) {
        if (channels[i].power >= 0) {
            digitalWrite(channels[i].power, LOW);
        }
    }
    return collected;
}
