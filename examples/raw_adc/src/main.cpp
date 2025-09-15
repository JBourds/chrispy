#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

#define MIC_PIN A0
#define MIC_EN 22
#define POWER_5V 5
#define CS_PIN 12
#define SD_EN 4
#define RESOLUTION BitResolution::Eight

// Recording
#define DURATION_SEC 3ul
#define BUF_SZ 512
uint8_t buf[BUF_SZ] = {0};
volatile static size_t index = 0;
uint32_t deadline = 0;

SdFat SD;
SdFile REC;
#define NUM_SAMPLES (SAMPLE_RATE * DURATION_SEC)

// Prescaler masks
#define DIV_128 0b111
#define DIV_64 0b110
#define DIV_32 0b101
#define DIV_16 0b100
#define DIV_8 0b011
#define DIV_4 0b010
#define DIV_2 0b001
#define DIV_2_2 0b000

ISR(ADC_vect) {
    if (index < BUF_SZ) {
        buf[index++] = ADCL;
        buf[index++] = ADCH;
    }
}

namespace adc {
void set_frequency(uint32_t sample_rate) {
    ADCSRA &= ~(DIV_128);
    ADCSRA |= DIV_64;
}
void enable_interrupts() { ADCSRA |= (1 << ADIE); }
void disable_interrupts() { ADCSRA &= ~(1 << ADIE); }

void enable_autotrigger() { ADCSRA |= (1 << ADATE); }
void disable_autotrigger() { ADCSRA &= ~(1 << ADATE); }

void on() {
    PRR0 &= ~(1 << PRADC);
    ADCSRA |= (1 << ADEN);
}

void off() { ADCSRA &= ~(1 << ADEN); }

void sleep() {
    off();
    PRR0 |= (1 << PRADC);
}
void start() {
    on();
    enable_interrupts();
    enable_autotrigger();
    set_frequency(24000);
    ADMUX = 1 << REFS0;
    ADCSRA |= (1 << ADSC);
}
}  // namespace adc

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        delay(50);
    }
    delay(500);

    pinMode(MIC_EN, OUTPUT);
    pinMode(POWER_5V, OUTPUT);
    pinMode(SD_EN, OUTPUT);

    pinMode(MIC_PIN, INPUT);

    digitalWrite(SD_EN, HIGH);
    digitalWrite(MIC_EN, LOW);
    digitalWrite(POWER_5V, HIGH);

    if (!SD.begin(CS_PIN)) {
        Serial.println("SD init failed!");
        while (true) {
        }
    }
    if (!REC.open("recbytes", O_TRUNC | O_WRITE | O_CREAT)) {
        Serial.println("Failed to open recrding file.");
        while (true) {
        }
    }

    Serial.println("Initialized");

    // while (true) {
    //     Serial.println(analogRead(MIC_PIN));
    // }
    // Start
    adc::start();
    deadline = millis() + DURATION_SEC * 1000;
}

void loop() {
    if (millis() > deadline) {
        REC.close();
        Serial.println("Done");
        while (true) {
        }
    }
    if (index == BUF_SZ) {
        size_t nbytes = REC.write(buf, index);
        for (size_t i = 0; i < index; i += 2) {
            uint8_t low = buf[i];
            uint8_t hi = buf[i + 1];
            Serial.println(low | (hi << 8), HEX);
        }
        if (nbytes != index) {
            Serial.println("Error writing to file!");
            while (true) {
            }
        }
        index = 0;
    }
}
