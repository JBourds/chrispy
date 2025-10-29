#include "Timer.h"

#include <Arduino.h>
#include <avr/interrupt.h>
#include <limits.h>
#include <math.h>

#define SKIP_PRESCALER 0
#define CTC_MODE (0b01 << WGM12)

#ifndef max
#define max(a, b) (a >= b ? a : b)
#endif
#ifndef min
#define min(a, b) (a >= b ? b : a)
#endif

static enum TimerRc get_compare_value(struct TimerConfig* cfg,
                                      clk_t max_compare);
static ssize_t find_smallest_prescaler(struct TimerConfig* cfg, size_t sz,
                                       pre_t* prescalers);
static void recompute(struct TimerConfig* cfg);
static enum TimerRc validate_preference(struct TimerConfig* cfg);
static void compute_error(struct TimerConfig* cfg);
static clk_t compute_delta(struct TimerConfig* cfg);
static void compute_actual(struct TimerConfig* cfg);
static uint8_t prescaler_mask(pre_t val);

static struct Timer1 {
    bool is_active = false;
    uint8_t tccr1a;
    uint8_t tccr1b;
    uint8_t tccr1c;
    uint16_t ocr1a;
    uint16_t ocr1b;
    uint16_t ocr1c;
    uint16_t icr1;
    uint8_t timsk1;

    void activate(TimerConfig& cfg) {
        if (is_active) {
            deactivate();
        }
        cli();
        // Save register state
        tccr1a = TCCR1A;
        tccr1b = TCCR1B;
        tccr1c = TCCR1C;
        ocr1a = OCR1A;
        ocr1b = OCR1B;
        ocr1c = OCR1C;
        icr1 = ICR1;
        timsk1 = TIMSK1;

        TCCR1A = 0;
        TCCR1B = prescaler_mask(cfg.prescaler) | CTC_MODE;
        // Clear all flags and timer state
        TCNT1 = 0;
        TIMSK1 = 0;
        TIFR1 = UINT8_MAX;

        sei();
        is_active = true;
    }

    void deactivate() {
        if (!is_active) {
            return;
        }
        cli();
        // Restore register state
        TCCR1A = tccr1a;
        TCCR1B = tccr1b;
        TCCR1C = tccr1c;
        OCR1A = ocr1a;
        OCR1B = ocr1b;
        OCR1C = ocr1c;
        ICR1 = icr1;
        TIMSK1 = timsk1;
        sei();
    }
} TIMER1;

static const size_t NPRESCALERS = 5;
static const pre_t PRESCALERS[] = {1, 8, 64, 256, 1024};
static pre_t SCRATCH_PRESCALERS[NPRESCALERS];

TimerRc activate_t1(TimerConfig& cfg) {
    memcpy(SCRATCH_PRESCALERS, PRESCALERS, sizeof(PRESCALERS));
    TimerRc rc = cfg.compute(NPRESCALERS, SCRATCH_PRESCALERS, UINT16_MAX, 0.0);
    if (rc == TimerRc::Okay || rc == TimerRc::ErrorRange) {
        TIMER1.activate(cfg);
    }
    return rc;
}
void deactivate_t1() { TIMER1.deactivate(); }

enum TimerRc TimerConfig::compute(size_t nprescalers, pre_t* prescalers,
                                  clk_t max_compare, double max_error) {
    if (this->desired > this->src) {
        return TimerRc::ImpossibleClock;
    }
    pre_t best_pre = 0;
    clk_t best_cmp = 0;
    double best_error = INFINITY;
    this->error = INFINITY;
    do {
        ssize_t i = find_smallest_prescaler(this, nprescalers, prescalers);
        if (i < 0) {
            break;
        }
        prescalers[i] = SKIP_PRESCALER;
        enum TimerRc rc = get_compare_value(this, max_compare);
        if (rc != TimerRc::Okay) {
            // No valid compare value
            continue;
        } else if (this->error < best_error) {
            best_pre = this->prescaler;
            best_cmp = this->compare;
            best_error = this->error;
        }
    } while (this->error > max_error);

    if (isinf(best_error)) {
        return TimerRc::ImpossibleClock;
    }

    // Config should always end up as the best solution we find
    this->prescaler = best_pre;
    this->compare = best_cmp;
    this->error = best_error;
    recompute(this);

    return this->error <= max_error ? TimerRc::Okay : TimerRc::ErrorRange;
}

static ssize_t find_smallest_prescaler(struct TimerConfig* cfg, size_t sz,
                                       pre_t* prescalers) {
    ssize_t prescaler_index = -1;
    for (size_t i = 0; i < sz; ++i) {
        // Avoid divide by zero, and allow for this to be used in a fixed point
        // algorithm in case some other prescaler works better
        if (prescalers[i] == SKIP_PRESCALER) {
            continue;
        } else if ((cfg->src / prescalers[i]) >= cfg->desired) {
            cfg->prescaler = prescalers[i];
            prescaler_index = i;
            break;
        }
    }
    return prescaler_index;
}

static enum TimerRc get_compare_value(struct TimerConfig* cfg,
                                      clk_t max_compare) {
    if (cfg->desired == 0 || cfg->prescaler == 0) {
        return TimerRc::ZeroDiv;
    }
    double ideal_compare =
        cfg->src / static_cast<double>((cfg->desired * cfg->prescaler));
    clk_t actual_compare = max(round(ideal_compare), 1);
    actual_compare = min(actual_compare, max_compare);
    cfg->compare = actual_compare;
    recompute(cfg);
    return validate_preference(cfg);
}

void TimerConfig::pprint() {
    Serial.print("Prescaler: ");
    Serial.println(this->prescaler);
    Serial.print("Compare Value: ");
    Serial.println(this->compare);
    Serial.print("Source Clock Frequency (Hz): ");
    Serial.println(this->src);
    Serial.print("Desired Clock Frequency (Hz): ");
    Serial.println(this->desired);
    Serial.print("Achieved Clock Frequency (Hz): ");
    Serial.println(this->actual);
    Serial.print("Error (%): ");
    Serial.println(this->error * 100);
}

const char* error_str(TimerRc rc) {
    switch (rc) {
        case TimerRc::Okay:
            return "Okay";
        case TimerRc::ImpossibleClock:
            return "Impossible Clock";
        case TimerRc::ZeroDiv:
            return "Zero Division";
        case TimerRc::ErrorRange:
            return "Error Range";
        case TimerRc::TooLow:
            return "Clock Rate Too Low";
        case TimerRc::TooHigh:
            return "Clock Rate Too High";
        default:
            return "?";
    }
}

static void compute_actual(struct TimerConfig* cfg) {
    cfg->actual = cfg->src / (cfg->prescaler * cfg->compare);
}

static clk_t compute_delta(struct TimerConfig* cfg) {
    return max(cfg->actual, cfg->desired) - min(cfg->actual, cfg->desired);
}

static void compute_error(struct TimerConfig* cfg) {
    clk_t delta = compute_delta(cfg);
    cfg->error = delta / static_cast<double>(cfg->actual);
}

static enum TimerRc validate_preference(struct TimerConfig* cfg) {
    if (cfg->skew == Skew::High && cfg->actual < cfg->desired) {
        return TimerRc::TooLow;
    } else if (cfg->skew == Skew::Low && cfg->actual > cfg->desired) {
        return TimerRc::TooHigh;
    } else {
        return TimerRc::Okay;
    }
}

static void recompute(struct TimerConfig* cfg) {
    compute_actual(cfg);
    compute_error(cfg);
}
static uint8_t prescaler_mask(pre_t val) {
    switch (val) {
        case 1:
            return 0b001;
        case 8:
            return 0b010;
        case 64:
            return 0b011;
        case 256:
            return 0b100;
        case 1024:
            return 0b101;
        default:
            return 0;
    }
}
