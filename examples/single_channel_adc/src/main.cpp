#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

#include "Adc.h"
#include "WavHeader.h"

#define MIC_PIN A0
#define MIC_EN 22
#define POWER_5V 5
#define CS_PIN 12
#define SD_EN 4
#define RESOLUTION BitResolution::Ten
#define SAMPLE_RATE 24000ul

// Recording
#define DURATION_SEC 5ul
#define BUF_SZ 1024
uint8_t buf[BUF_SZ] = {0};
uint32_t deadline = 0;

SdFat SD;
SdFile REC;
#define NUM_SAMPLES (SAMPLE_RATE * DURATION_SEC)

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
    if (!REC.open("adc_rec.wav", O_TRUNC | O_WRITE | O_CREAT)) {
        Serial.println("Failed to open recrding file.");
        while (true) {
        }
    }

    Serial.println("Initialized");
}

void loop() {
    uint8_t nchannels = 1;
    Channel channels[] = {{.pin = A0, .power = 22}};
    Adc adc(nchannels, channels, buf, BUF_SZ);
    WavHeader hdr;
    if (REC.write(&hdr, sizeof(hdr)) != sizeof(hdr)) {
        Serial.println("Error writing out placeholder header bytes.");
        goto done;
    }
    uint8_t* tmp_buf = nullptr;
    size_t sz = 0;
    uint32_t deadline = millis() + DURATION_SEC * 1000;
    if (adc.start(RESOLUTION, SAMPLE_RATE) != 0) {
        Serial.println("Error starting ADC");
        goto done;
    }
    while (millis() < deadline) {
        if (adc.swap_buffer(&tmp_buf, sz) == 0) {
            if (tmp_buf == nullptr) {
                continue;
            }
            size_t nbytes = REC.write(tmp_buf, sz);
            if (nbytes != sz) {
                Serial.println("Error writing to file!");
                Serial.print("Expected ");
                Serial.println(sz);
                Serial.print("Got ");
                Serial.println(nbytes);
                goto done;
            }
        }
    }
    uint32_t ncollected = adc.stop();
    uint32_t sample_rate = ncollected / DURATION_SEC;
    Serial.print("Seconds: ");
    Serial.println(DURATION_SEC);
    Serial.print("Number of samples: ");
    Serial.println(ncollected);
    Serial.print("Sample Rate (Hz): ");
    Serial.println(sample_rate);
    hdr.fill(RESOLUTION, static_cast<uint32_t>(REC.fileSize()), sample_rate);
    if (!(REC.seekSet(0) && REC.write(&hdr, sizeof(hdr)) == sizeof(hdr))) {
        Serial.println("Error writing out filled in wav header.");
        goto done;
    }

done:
    if (!REC.close()) {
        Serial.println("Error closing recording file.");
    }
    while (true) {
    }
}
