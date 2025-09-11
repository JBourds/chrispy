#include <SdFat.h>

#include "../../../include/WavHeader.h"
#include "Adc.h"

#define MIC_PIN A0
#define CS_PIN 12
#define MIC_EN 22
#define SD_EN 4
#define POWER_5V 5

// Recording
#define SAMPLE_RATE 24000ul  // 24 kHz
#define DURATION_SEC 3ul

#define NUM_SAMPLES (SAMPLE_RATE * DURATION_SEC)

uint16_t fileIndex = 1;
SdFat SD;

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        delay(50);
    }
    delay(500);

    pinMode(MIC_EN, OUTPUT);
    pinMode(SD_EN, OUTPUT);
    pinMode(POWER_5V, OUTPUT);
    digitalWrite(MIC_EN, LOW);
    digitalWrite(SD_EN, HIGH);
    digitalWrite(POWER_5V, HIGH);

    if (!SD.begin(CS_PIN)) {
        Serial.println("SD init failed!");
        while (1);
    }

    Serial.println("SD initialized.");
}

void loop() {
    // Build file name
    char filename[20];
    sprintf(filename, "Rec%d.wav", fileIndex);
    SdFile recording;
    if (!recording.open(filename, O_TRUNC | O_WRITE | O_CREAT)) {
        Serial.println("Could not open file!");
        while (true) {
        }
    }

    for (uint32_t i = 0; i < sizeof(WavHeader); ++i) {
        if (!recording.write((uint8_t)0)) {
            Serial.println("Failed to write zeroed .wav header.");
            while (true) {
            }
        }
    }

    // Write WAV header
    Serial.print("Recording ");
    Serial.println(filename);

    // TODO: Record
    uint8_t channel_count = 3;
    adc::Channel channels[] = {
        {.pin = A0, .differenced = -1, .gain = adc::Gain::One},
        {.pin = A4, .differenced = -1, .gain = adc::Gain::One},
        {.pin = A5, .differenced = -1, .gain = adc::Gain::One},
    };
#define BUF_SZ 36
    uint8_t buf[BUF_SZ] = {0};
    adc::Adc adc(BUF_SZ, buf, channel_count, channels);
    adc.enable_interrupts();
    adc.enable_autotrigger();
    adc.start(BitResolution::Ten, 24000);
    while (true) {
    }
    adc.disable_autotrigger();
    adc.disable_interrupts();

    WavHeader hdr(BitResolution::Ten, recording.fileSize(), SAMPLE_RATE);
    recording.seekSet(0);
    if (recording.write(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr)) !=
        sizeof(hdr)) {
        Serial.println("Failed to write .wav header.");
        while (true) {
        }
    }
    if (!recording.close()) {
        Serial.println("Failed to close file.");
        while (true) {
        }
    }

    ++fileIndex;
    delay(2000);
}
