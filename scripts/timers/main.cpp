#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cstdlib>

#include "timer.h"

#define SKIP_PRESCALER 0

#define max(a, b) (a >= b ? a : b)
#define min(a, b) (a >= b ? b : a)

void print_clock_rate_sweep(clk_t lower, clk_t upper, clk_t step, clk_t src,
                            size_t nprescalers, pre_t* prescalers,
                            clk_t max_compare, const char* outfile) {
    printf("Clock Rate Sweep:\n");
    printf("lower: %lu\n", lower);
    printf("upper: %lu\n", upper);
    printf("step: %lu\n", step);
    printf("src: %lu\n", src);
    printf("nprescalers: %lu\n", nprescalers);
    printf("prescaler values: {");
    for (size_t i = 0; i < nprescalers; ++i) {
        printf("%lu", (clk_t)prescalers[i]);
        if (i != nprescalers - 1) {
            printf(", ");
        }
    }
    printf("}\n");
    printf("max compare: %lu\n", max_compare);
    printf("results: %s\n", outfile);
}

int32_t clock_rate_sweep(clk_t lower, clk_t upper, clk_t step, clk_t src,
                         size_t nprescalers, pre_t* prescalers,
                         clk_t max_compare, const char* output) {
    FILE* outfile = fopen(output, "w");
    if (outfile == NULL) {
        fprintf(stderr, "Error opening output file: %s\n", output);
        return -1;
    }
    int32_t rc = fputs("Desired,Actual\n", outfile);
    if (rc < 0) {
        fprintf(stderr, "Error writing CSV header to %s\n", output);
        fclose(outfile);
        return -2;
    }

    size_t prescaler_sz = nprescalers * sizeof(pre_t);
    pre_t* scratch_prescalers = reinterpret_cast<pre_t*>(malloc(prescaler_sz));
    if (scratch_prescalers == NULL) {
        fprintf(stderr, "Error allocating memory for scratch prescalers.\n");
        return -3;
    }

    for (clk_t desired = lower; desired <= upper; desired += step) {
        TimerConfig cfg(src, desired, Skew::None);
        memcpy(scratch_prescalers, prescalers, prescaler_sz);
        {
            TimerRc rc =
                cfg.compute(nprescalers, scratch_prescalers, max_compare, 0.0);
            if (rc == TimerRc::TooLow || rc == TimerRc::TooHigh) {
                fprintf(stderr,
                        "Unable to find a valid timer configuration for "
                        "desired value of %lu. Encountered "
                        "error: \"%s\"\n",
                        desired, error_str(rc));
                fclose(outfile);
                return -4;
            }
        }
        rc = fprintf(outfile, "%lu,%lu\n", desired, cfg.actual);
        if (rc < 0) {
            fprintf(stderr, "Error writing CSV row to %s\n", output);
            fclose(outfile);
            return -5;
        }
    }
    free(scratch_prescalers);

    return 0;
}

int main(int argc, char* argv[]) {
    size_t nprescalers = 5;
    pre_t prescalers[] = {1, 8, 64, 256, 1024};
    clk_t max_compare = UINT16_MAX;
    clk_t clock_rate = 16000000;
    clk_t low = 1;
    clk_t high = 76000;
    clk_t step = 1;
    const char* outfile = "timer_results.csv";

    print_clock_rate_sweep(low, high, step, clock_rate, nprescalers, prescalers,
                           max_compare, outfile);
    int32_t rc = clock_rate_sweep(low, high, step, clock_rate, nprescalers,
                                  prescalers, max_compare, outfile);
    if (rc < 0) {
        printf("Error performing clock rate sweep\n");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
