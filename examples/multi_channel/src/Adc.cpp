#include "Adc.h"

#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

namespace adc {

#define EFULL 0b1
#define ECHANNEL 0b10

static struct AdcFrame {
    volatile uint8_t eflags;
    BitResolution res;
    volatile size_t index;
    uint8_t channel_count;
    volatile uint8_t ch_index;
    Channel* channels;
    uint8_t* buf;
    size_t sz;
    uint32_t collected;
} FRAME;

static bool activate(Channel& ch) {
    int8_t mask = ch.mux_mask();
    if (mask < 0) {
        Serial.println(mask);
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
    // Alternate channels we sample from
    FRAME.ch_index++;
    FRAME.ch_index &= (FRAME.channel_count - 1);
    if (!activate(FRAME.channels[FRAME.ch_index])) {
        Serial.println("Channel error!");
        FRAME.eflags |= ECHANNEL;
        return;
    }

    if (FRAME.res == BitResolution::Eight) {
        if (FRAME.index + 1 > FRAME.sz) {
            FRAME.eflags |= EFULL;
            return;
        }
        FRAME.buf[FRAME.index++] = ADCL;
        FRAME.index &= (FRAME.sz - 1);
    } else {
        if (FRAME.index + 2 > FRAME.sz) {
            FRAME.eflags |= EFULL;
            return;
        }
        FRAME.buf[FRAME.index++] = ADCH;
        FRAME.buf[FRAME.index++] = ADCL;
        FRAME.index &= (FRAME.sz - 1);
    }
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

void Adc::start(BitResolution res, uint32_t sample_rate) {
    FRAME.res = res;
    FRAME.eflags = 0;
    FRAME.index = 0;
    FRAME.buf = buf;
    FRAME.sz = sz;
    FRAME.channel_count = channel_count;
    FRAME.channels = channels;
    FRAME.ch_index = 0;
    FRAME.collected = 0;
    // TODO: Actual timer math here
    // Fastest speed for the moment
    ADCSRA &= ~0b111;
    // Start conversion
    ADCSRA |= (1 << ADSC);
}
uint32_t Adc::stop() {
    disable_interrupts();
    disable_autotrigger();
    return FRAME.collected;
}
}  // namespace adc
