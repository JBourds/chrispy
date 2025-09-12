#include "Adc.h"

#define MIC_PIN A0
#define MIC_EN 22
#define POWER_5V 5
#define RESOLUTION BitResolution::Ten

// Recording
#define SAMPLE_RATE 24000ul  // 24 kHz
#define DURATION_SEC 3ul
#define BUF_SZ 1024

#define NUM_SAMPLES (SAMPLE_RATE * DURATION_SEC)

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        delay(50);
    }
    delay(500);

    pinMode(MIC_EN, OUTPUT);
    pinMode(POWER_5V, OUTPUT);
    digitalWrite(MIC_EN, LOW);
    digitalWrite(POWER_5V, HIGH);
}

void loop() {
    uint8_t nchannels = 1;
    Channel channels[] = {
        {.pin = A0, .power = 22, .differenced = -2, .gain = Gain::One},
        {.pin = A4, .power = 26, .differenced = -1, .gain = Gain::One},
        {.pin = A5, .power = 27, .differenced = -1, .gain = Gain::One},
    };
    uint8_t buf[BUF_SZ] = {0};
    Adc adc(nchannels, channels, buf, BUF_SZ);
    adc.enable_interrupts();
    adc.enable_autotrigger();
    uint8_t prescaler_mask = 0b111;
    uint32_t deadline = millis() + DURATION_SEC * 1000;

    adc.start(RESOLUTION, SAMPLE_RATE);
    while (millis() < deadline) {
    }
    uint32_t collected = adc.stop();

    Serial.print("Channels: ");
    Serial.println(nchannels);

    Serial.print("Bit Resolution: ");
    Serial.println(RESOLUTION == BitResolution::Eight ? 8 : 10);

    Serial.print("Prescaler value: 0x");
    Serial.println(ADCSRA & prescaler_mask, HEX);

    Serial.print("Collected ");
    Serial.print(collected);
    Serial.print(" samples in ");
    Serial.print(DURATION_SEC);
    Serial.print(" seconds (");
    double kSPS = (double)collected / (DURATION_SEC * 1000.0);
    Serial.print(kSPS);
    Serial.println("kHz sample rate)");

    delay(2000);
}
