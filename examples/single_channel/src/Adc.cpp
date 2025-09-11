#include "Adc.h"

#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

namespace adc {

struct AdcFrame {
    uint8_t* buf;
    SdFile* file;
    BitResolution res;
    size_t sz;
    size_t index;
} FRAME;

ISR(ADC_vect) {
    Serial.print("Conversion complete: ");
    uint16_t val = (ADCH << 8) | ADCL;
    Serial.println(val, HEX);
    FRAME.index++;
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

void Adc::sample() {
    uint8_t mask = 1 << ADSC;
    ADCSRA |= mask;
    while (ADCSRA & mask) {
    }
}
}  // namespace adc
