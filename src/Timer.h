#include <stddef.h>
#include <stdint.h>

typedef uint32_t clk_t;
typedef uint16_t pre_t;

#ifndef ssize_t
typedef int32_t ssize_t;
#endif

enum struct TimerRc : uint8_t {
    Okay,
    ImpossibleClock,
    CompareRange,
    ZeroDiv,
    ErrorRange,
    TooLow,
    TooHigh,
};

enum struct Skew : uint8_t {
    Low,
    High,
    None,
};

struct TimerConfig {
    pre_t prescaler;
    clk_t compare;

    clk_t src;
    clk_t desired;
    Skew skew;
    clk_t actual;
    double error;

    TimerConfig(clk_t src_clock, clk_t desired_clk, Skew preference)
        : src(src_clock), desired(desired_clk), skew(preference) {}
    TimerRc compute(size_t nprescalers, pre_t* prescalers, clk_t max_compare,
                    double max_error);
    void pprint();
};

TimerRc activate_t1(TimerConfig& cfg);
void deactivate_t1();
