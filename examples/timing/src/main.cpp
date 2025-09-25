#include <Arduino.h>

#include "Timer.h"

#define LED_PIN 13

// Simply trigger LED back/forth
ISR(TIMER1_COMPA_vect) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); }

void setup() {
    Serial.begin(9600);
    while (!Serial) {
    }
    size_t nprescalers = 5;
    pre_t prescalers[] = {1, 8, 64, 256, 1024};
    clk_t rate = 1;
    TimerConfig cfg(F_CPU, rate, Skew::High);
    TimerRc rc = cfg.compute(nprescalers, prescalers, UINT16_MAX, 0.0);
    if (rc == TimerRc::ErrorRange) {
        Serial.println("Unable to get less than or equal to max error bound.");
    } else if (rc != TimerRc::Okay) {
        Serial.print("Error: ");
        Serial.println(static_cast<uint8_t>(rc));
        while (true) {
        }
    }
    pinMode(LED_PIN, OUTPUT);
    cfg.pprint();
    activate_t1(cfg);
    cfg.pprint();
    // Enable interrupts
    TIMSK1 |= (1 << OCIE1A);
}
void loop() {
    delay(5000);
    deactivate_t1();
}
