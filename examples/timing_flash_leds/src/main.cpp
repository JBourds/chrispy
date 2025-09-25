#include <Arduino.h>

#include "Timer.h"

#define LED_PIN 13

// Simply trigger LED back/forth
ISR(TIMER1_COMPA_vect) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); }

void setup() {
    pinMode(LED_PIN, OUTPUT);

    Serial.begin(9600);
    while (!Serial) {
    }

    clk_t rate = 1;
    TimerConfig cfg(F_CPU, rate, Skew::High);
    cfg.pprint();

    TimerRc rc = activate_t1(cfg);
    if (rc == TimerRc::ErrorRange) {
        Serial.println("Unable to get less than or equal to max error bound.");
    } else if (rc != TimerRc::Okay) {
        Serial.print("Error: ");
        Serial.println(static_cast<uint8_t>(rc));
        while (true) {
        }
    }

    // Set counter to compare value
    cli();
    OCR1A = cfg.compare;
    sei();

    // Enable interrupts
    TIMSK1 |= (1 << OCIE1A);
}
void loop() {
    delay(5000);
    deactivate_t1();
}
