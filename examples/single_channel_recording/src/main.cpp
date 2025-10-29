#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

#include "Recorder.h"

#define MIC_PIN A0
#define MIC_POWER 22
#define POWER_5V 5
#define SD_CS_PIN 12
#define SD_EN 4
#define RESOLUTION BitResolution::Eight
#define SAMPLE_RATE 24000ul

// Recording
#define DURATION_SEC 5ul
#define BUF_SZ 4096
uint8_t BUF[BUF_SZ] = {0};

// Max SPI rate for AVR is 10 MHz for F_CPU 20 MHz, 8 MHz for F_CPU 16 MHz.
#define SPI_CLOCK SD_SCK_MHZ(F_CPU / 2)

// Select fastest interface.
#if ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif

using adc::Channel;

SdFat SD;
#define NCHANNELS 1
Channel CHANNELS[] = {Channel(MIC_PIN, MIC_POWER, false)};
const char* FILENAMES[NCHANNELS] = {"adc_rec.wav"};

void done() {
    Serial.println("Done");
    while (true) {
    }
}

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
        done();
    }

    if (!recording::init(NCHANNELS, CHANNELS, &SD)) {
        Serial.println("Recording init failed!");
        done();
    }

    Serial.println("Initialized");
}

void loop() {
    int64_t rc = recording::record(FILENAMES, RESOLUTION, SAMPLE_RATE,
                                   DURATION_SEC * 1000, BUF, BUF_SZ);
    if (rc < 0) {
        Serial.print("Error during recording. RC: ");
        Serial.println(static_cast<int32_t>(rc));
    }
    done();
}
