#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SKIP_PRESCALER 0

#define max(a, b) (a >= b ? a : b)
#define min(a, b) (a >= b ? b : a)

typedef uint64_t clk_t;
typedef uint32_t pre_t;

enum TimerRc {
    OKAY,
    IMPOSSIBLE_CLK,
    COMPARE_RANGE,
    ZERO_DIV,
    ERROR_RANGE,
    TOO_LOW,
    TOO_HIGH,
};

enum SkewBehavior {
    PREFER_LOW,
    PREFER_HIGH,
    EITHER,
};

struct TimerConfig {
    enum SkewBehavior preference;
    pre_t prescaler;
    clk_t src;
    clk_t desired;
    clk_t actual;
    clk_t compare;
    double error;
};

const char* error_str(enum TimerRc rc) {
    switch (rc) {
        case IMPOSSIBLE_CLK:
            return "Impossible Clock";
        case ZERO_DIV:
            return "Zero Division";
        case COMPARE_RANGE:
            return "Compare Range";
        case ERROR_RANGE:
            return "Compare Range";
        default:
            return "?";
    }
}

void print_timer_config(struct TimerConfig* cfg) {
    printf("Prescaler: %u\n", cfg->prescaler);
    printf("Compare Value: %lu\n", cfg->compare);
    printf("Source Clock Frequency (Hz): %lu\n", cfg->src);
    printf("Desired Clock Frequency (Hz): %lu\n", cfg->desired);
    printf("Achieved Clock Frequency (Hz): %lu\n", cfg->actual);
    printf("Error (%%): %f\n", cfg->error * 100);
}

ssize_t find_smallest_prescaler(struct TimerConfig* cfg, size_t sz,
                                pre_t prescalers[sz]) {
    ssize_t prescaler_index = -1;
    for (size_t i = 0; i < sz; ++i) {
        // Avoid divide by zero, and allow for this to be used in a fixed point
        // algorithm in case some other prescaler works better
        if (prescalers[i] == SKIP_PRESCALER) {
            continue;
        } else if ((cfg->src / prescalers[i]) > cfg->desired) {
            cfg->prescaler = prescalers[i];
            prescaler_index = i;
            break;
        }
    }
    return prescaler_index;
}

void compute_actual(struct TimerConfig* cfg) {
    cfg->actual = cfg->src / (cfg->prescaler * cfg->compare);
}

clk_t compute_delta(struct TimerConfig* cfg) {
    return max(cfg->actual, cfg->desired) - min(cfg->actual, cfg->desired);
}

void compute_error(struct TimerConfig* cfg) {
    clk_t delta = compute_delta(cfg);
    cfg->error = delta / (double)cfg->desired;
}

enum TimerRc validate_preference(struct TimerConfig* cfg) {
    if (cfg->preference == PREFER_HIGH && cfg->actual < cfg->desired) {
        return TOO_LOW;
    } else if (cfg->preference == PREFER_LOW && cfg->actual > cfg->desired) {
        return TOO_HIGH;
    } else {
        return OKAY;
    }
}

void recompute(struct TimerConfig* cfg) {
    compute_actual(cfg);
    compute_error(cfg);
}

enum TimerRc get_compare_value(struct TimerConfig* cfg, clk_t max_compare) {
    if (cfg->desired == 0 || cfg->prescaler == 0) {
        return ZERO_DIV;
    }
    double ideal_compare = cfg->src / (double)(cfg->desired * cfg->prescaler);
    clk_t actual_compare = max(round(ideal_compare), 1);
    if (actual_compare > max_compare) {
        return COMPARE_RANGE;
    }
    cfg->compare = actual_compare;
    recompute(cfg);
    return validate_preference(cfg);
}

enum TimerRc get_timer_config(struct TimerConfig* cfg, size_t sz,
                              pre_t prescalers[sz], clk_t max_compare,
                              double max_error) {
    if (cfg->desired > cfg->src) {
        return IMPOSSIBLE_CLK;
    }
    pre_t best_pre = 0;
    clk_t best_cmp = 0;
    double best_error = INFINITY;
    cfg->error = INFINITY;
    do {
        ssize_t i = find_smallest_prescaler(cfg, sz, prescalers);
        if (i < 0) {
            break;
        }
        prescalers[i] = SKIP_PRESCALER;
        enum TimerRc rc = get_compare_value(cfg, max_compare);
        if (rc != OKAY) {
            // No valid compare value
            continue;
        } else if (cfg->error < best_error) {
            best_pre = cfg->prescaler;
            best_cmp = cfg->compare;
            best_error = cfg->error;
        }
    } while (cfg->error > max_error);

    if (isinf(best_error)) {
        return IMPOSSIBLE_CLK;
    }

    // Config should always end up as the best solution we find
    cfg->prescaler = best_pre;
    cfg->compare = best_cmp;
    cfg->error = best_error;
    recompute(cfg);

    return cfg->error <= max_error ? 0 : ERROR_RANGE;
}

int main(int argc, char* argv[]) {
    size_t nprescalers = 5;
    pre_t prescalers[] = {1, 8, 64, 256, 1024};
    uint16_t max_compare = UINT16_MAX;
    struct TimerConfig cfg = {
        .src = 16000000,
        .desired = 1,
        .preference = PREFER_HIGH,
    };
    int32_t rc =
        get_timer_config(&cfg, nprescalers, prescalers, max_compare, 0.0);
    if (rc < 0) {
        printf(
            "Unable to find a valid timer configuration. Encountered error: "
            "\"%s\"\n",
            error_str(rc));
        exit(EXIT_FAILURE);
    }
    print_timer_config(&cfg);

    exit(EXIT_SUCCESS);
}
