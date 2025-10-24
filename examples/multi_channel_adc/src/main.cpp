#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

#include "Adc.h"
#include "SdFunctions.h"
#include "WavHeader.h"

using adc::Channel;

#define MIC1_PIN A0
#define MIC1_POWER 22
#define MIC2_PIN A4
#define MIC2_POWER 26
#define POWER_5V 5
#define SD_CS_PIN 12
#define SD_EN 4
#define RESOLUTION BitResolution::Eight
#define SAMPLE_RATE 18000ul

// Max SPI rate for AVR is 10 MHz for F_CPU 20 MHz, 8 MHz for F_CPU 16 MHz.
#define SPI_CLOCK SD_SCK_MHZ(F_CPU / 2)

// Select fastest interface.
#if ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif

// Recording
#define DURATION_SEC 5ul
#define BUF_SZ 4096
uint8_t BUF[BUF_SZ] = {0};

#define NCHANNELS 2
Channel CHANNELS[] = {
    Channel(MIC1_PIN, MIC1_POWER, false),
    Channel(MIC2_PIN, MIC2_POWER, false),
};

SdFat SD;
SdFile FILES[NCHANNELS];
const char* FILENAMES[NCHANNELS] = {
    "adc_rec_ch1.wav",
    "adc_rec_ch2.wav",
};

void done() {
    close_all(FILES, NCHANNELS);
    while (true) {
    }
}

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        delay(50);
    }

    pinMode(POWER_5V, OUTPUT);
    pinMode(SD_EN, OUTPUT);
    digitalWrite(SD_EN, HIGH);
    digitalWrite(POWER_5V, HIGH);

    if (!SD.begin(SD_CONFIG)) {
        done();
    }
    if (!adc::init(NCHANNELS, CHANNELS, BUF, BUF_SZ)) {
        Serial.println("ADC init failed.");
        done();
    }

    for (size_t i = 0; i < NCHANNELS; ++i) {
        pinMode(CHANNELS[i].power, OUTPUT);
        digitalWrite(CHANNELS[i].power, LOW);
        pinMode(CHANNELS[i].pin, INPUT);
        if (!FILES[i].open(FILENAMES[i], O_TRUNC | O_WRITE | O_CREAT)) {
            Serial.print("Error opening file ");
            Serial.println(FILENAMES[i]);
            done();
        }
    }

    Serial.println("Initialized");
}

void loop() {
    WavHeader hdr;
    uint8_t* tmp_buf = nullptr;
    size_t sz = 0;
    size_t ch_index = 0;
    for (size_t i = 0; i < NCHANNELS; ++i) {
        if (FILES[i].write(&hdr, sizeof(hdr)) != sizeof(hdr)) {
            Serial.println("Error writing placeholder header bytes.");
            done();
        }
    }

    if (adc::start(RESOLUTION, SAMPLE_RATE) != 0) {
        Serial.println("Error starting ADC");
        done();
    }
    uint32_t deadline = millis() + DURATION_SEC * 1000;
    while (millis() < deadline) {
        if (adc::swap_buffer(&tmp_buf, sz, ch_index) == 0) {
            if (tmp_buf == nullptr) {
                continue;
            }
            size_t nbytes = FILES[ch_index].write(tmp_buf, sz);
            if (nbytes != sz) {
                Serial.println("Error writing to file!");
                Serial.print("Expected ");
                Serial.println(sz);
                Serial.print("Got ");
                Serial.println(nbytes);
                adc::stop();
                done();
            }
        }
    }
    uint32_t ncollected = adc::stop();
    while (adc::drain_buffer(&tmp_buf, sz, ch_index) == 0) {
        Serial.print("Draining ");
        Serial.print(sz);
        Serial.println(" more samples");
        if (tmp_buf == nullptr) {
            continue;
        }
        size_t nbytes = FILES[ch_index].write(tmp_buf, sz);
        if (nbytes != sz) {
            Serial.println("Error writing to file!");
            Serial.print("Expected ");
            Serial.println(sz);
            Serial.print("Got ");
            Serial.println(nbytes);
            done();
        }
    }

    uint32_t per_ch_sample_rate = ncollected / (NCHANNELS * DURATION_SEC);
    Serial.print("Seconds: ");
    Serial.println(DURATION_SEC);
    Serial.print("Number of samples: ");
    Serial.println(ncollected);
    Serial.print("Sample Rate Per adc::Channel (Hz): ");
    Serial.println(per_ch_sample_rate);

    int64_t rc = truncate_to_smallest(FILES, NCHANNELS);
    if (rc < 0) {
        Serial.println("Error truncating recorded files to smallest one.");
        done();
    }
    uint32_t min_sz = static_cast<uint32_t>(rc);
    hdr.fill(RESOLUTION, static_cast<uint32_t>(min_sz), per_ch_sample_rate);
    for (size_t i = 0; i < NCHANNELS; ++i) {
        if (!(FILES[i].seekSet(0) &&
              FILES[i].write(&hdr, sizeof(hdr)) == sizeof(hdr))) {
            Serial.println("Error writing completed wav header.");
        }
    }
    done();
}
