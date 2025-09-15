#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

#include "Adc.h"

#define MIC_PIN A0
#define MIC_EN 22
#define POWER_5V 5
#define CS_PIN 12
#define SD_EN 4
#define RESOLUTION BitResolution::Ten
#define SAMPLE_RATE 24000ul

// Recording
#define DURATION_SEC 3ul
#define BUF_SZ 512
uint8_t buf[BUF_SZ] = {0};
volatile static size_t index = 0;
uint32_t deadline = 0;

SdFat SD;
SdFile REC;
#define NUM_SAMPLES (SAMPLE_RATE * DURATION_SEC)

ISR(ADC_vect) {
    if (index < BUF_SZ) {
        buf[index++] = ADCL;
        buf[index++] = ADCH;
    }
}

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
}

void loop() {
    uint8_t nchannels = 1;
    Channel channels[] = {{.pin = A0, .power = 22}};
    Adc adc(nchannels, channels, buf, BUF_SZ);
    int16_t rc = adc.start(RESOLUTION, SAMPLE_RATE);
    if (rc != 0) {
        Serial.println("Error starting ADC");
        while (true) {
        }
    }
    uint32_t deadline = millis() + DURATION_SEC * 1000;
    while (true) {
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
}
