
#include "timer.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>

#define SKIP_PRESCALER 0

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

static const size_t NPRESCALERS = 5;
static const pre_t PRESCALERS[] = {1, 8, 64, 256, 1024};
static pre_t SCRATCH_PRESCALERS[NPRESCALERS];

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

void TimerConfig::pprint() {
    printf("Prescaler: %lu\n", this->prescaler);
    printf("Compare Value: %lu\n", this->compare);
    printf("Source Clock Frequency (Hz): %lu\n", this->src);
    printf("Desired Clock Frequency (Hz): %lu\n", this->desired);
    printf("Achieved Clock Frequency (Hz): %lu\n", this->actual);
    printf("Error (%%): %f\n", this->error * 100);
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
