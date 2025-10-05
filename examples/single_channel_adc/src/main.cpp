#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

#include "Adc.h"
#include "WavHeader.h"

#define MIC_PIN A0
#define MIC_POWER 22
#define POWER_5V 5
#define POWER_3V 3
#define SD_CS_PIN 12
#define SD_EN 4
#define RESOLUTION BitResolution::Eight
#define SAMPLE_RATE 24000ul

// Recording
#define DURATION_SEC 5ul
#define BUF_SZ 1024
uint8_t BUF[BUF_SZ] = {0};

// Max SPI rate for AVR is 10 MHz for F_CPU 20 MHz, 8 MHz for F_CPU 16 MHz.
#define SPI_CLOCK SD_SCK_MHZ(F_CPU / 2)

// Select fastest interface.
#if ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif

SdFat SD;
SdFile REC;
uint8_t NCHANNELS = 1;
Channel CHANNELS[] = {{.pin = MIC_PIN, .power = MIC_POWER}};
const char* FILENAME = "adc_rec.wav";

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        delay(50);
    }

    pinMode(SD_EN, OUTPUT);
    pinMode(POWER_5V, OUTPUT);
    digitalWrite(SD_EN, HIGH);
    digitalWrite(POWER_5V, HIGH);

    if (!SD.begin(SD_CONFIG)) {
        Serial.println("SD init failed!");
        while (true) {
        }
    }
    if (!REC.open(FILENAME, O_TRUNC | O_WRITE | O_CREAT)) {
        Serial.println("Failed to open recording file.");
        while (true) {
        }
    }

    for (size_t i = 0; i < NCHANNELS; ++i) {
        pinMode(CHANNELS[i].pin, INPUT);
        pinMode(CHANNELS[i].power, OUTPUT);
        digitalWrite(CHANNELS[i].power, LOW);
    }

    Serial.println("Initialized");
}

void done() {
    if (!REC.close()) {
        Serial.println("Error closing recording file.");
    }
    while (true) {
    }
}

void loop() {
    Adc adc(NCHANNELS, CHANNELS, BUF, BUF_SZ);
    WavHeader hdr;
    uint8_t* tmp_buf = nullptr;
    size_t sz = 0;
    size_t ch_index = 0;
    uint32_t ncollected;
    uint32_t sample_rate;

    if (REC.write(&hdr, sizeof(hdr)) != sizeof(hdr)) {
        Serial.println("Error writing out placeholder header bytes.");
        done();
    }
    if (adc.start(RESOLUTION, SAMPLE_RATE) != 0) {
        Serial.println("Error starting ADC");
        done();
    }
    uint32_t deadline = millis() + DURATION_SEC * 1000;
    while (millis() < deadline) {
        if (adc.swap_buffer(&tmp_buf, sz, ch_index) == 0) {
            if (tmp_buf == nullptr) {
                continue;
            }
            uint32_t t0 = micros();
            size_t nbytes = REC.write(tmp_buf, sz);
            Serial.println(micros() - t0);
            if (nbytes != sz) {
                Serial.println("Error writing to file!");
                Serial.print("Expected ");
                Serial.println(sz);
                Serial.print("Got ");
                Serial.println(nbytes);
                done();
            }
        }
    }
    ncollected = adc.stop();
    while (adc.drain_buffer(&tmp_buf, sz, ch_index) == 0) {
        Serial.print("Draining ");
        Serial.print(sz);
        Serial.println(" more samples");
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
            done();
        }
    }

    sample_rate = ncollected / DURATION_SEC;
    Serial.print("Seconds: ");
    Serial.println(DURATION_SEC);
    Serial.print("Number of samples: ");
    Serial.println(ncollected);
    Serial.print("Sample Rate (Hz): ");
    Serial.println(sample_rate);

    hdr.fill(RESOLUTION, static_cast<uint32_t>(REC.fileSize()), sample_rate);
    if (!(REC.seekSet(0) && REC.write(&hdr, sizeof(hdr)) == sizeof(hdr))) {
        Serial.println(
            "Error shrinking file to used size and writing out filled in wav "
            "header.");
        done();
    }
    done();
}
