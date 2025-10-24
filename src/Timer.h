#include <stddef.h>
#include <stdint.h>

typedef uint32_t clk_t;
typedef uint16_t pre_t;

#ifndef ssize_t
typedef int32_t ssize_t;
#endif

/**
 * Possible return codes when trying to compute the best timer configuration.
 */
enum struct TimerRc : uint8_t {
    Okay,            /* !< Operation successful. */
    ImpossibleClock, /* !< Clock rate cannot be achieved with parameters.*/
    ZeroDiv,         /* !< Operation would cause divide by zero error. */
    ErrorRange,      /* !< Invalid range of values.  */
    TooLow,          /* !< Value is too hgih for range. */
    TooHigh,         /* !< Value is too low for range. */
};

/**
 * Retrieve a string description of a timer return code.
 *
 * @param rc: Return code to get its string for.
 *
 * @returns (const char*): Static string for return code.
 */
const char* error_str(TimerRc rc);

/**
 * Preference for how the computed timer configuration should skew if it cannot
 * get the precise value.
 */
enum struct Skew : uint8_t {
    Low,  /* !< Prefer a lower clock rate. */
    High, /* !< Prefer a higher clock rate. */
    None, /* !< No preference. */
};

/**
 * Configuration for a hardware timer.
 */
struct TimerConfig {
    /**
     * Prescaler value to use with timer.
     */
    pre_t prescaler;
    /**
     * Comparison value to trigger timer at.
     */
    clk_t compare;
    /**
     * Input clock frequency (Hz).
     */
    clk_t src;
    /**
     * Target clock frequency (Hz) given input.
     */
    clk_t desired;
    /**
     * Preference for whether a lower/high clock rate.
     */
    Skew skew;
    /**
     * Actual clock rate achieved.
     */
    clk_t actual;
    /**
     * Percentage error from desired clock rate.
     */
    double error;

    /**
     * Constructor for a timer config which initializes required variables for
     * a configuration but does not perform any computation.
     *
     * @param src_clock: Input clock frequency (Hz).
     * @param desired_clock: Desired clock frequency (Hz).
     * @param preference: Preference for whether to err low or high when
     *  calculating parameters to get desired clock frequency.
     */
    TimerConfig(clk_t src_clock, clk_t desired_clock, Skew preference)
        : src(src_clock), desired(desired_clock), skew(preference) {}

    /**
     * Compute the first timer configuration satisfying the error constraint,
     * or find the best possible timer configuration if given a `max_error` of
     * 0.
     *
     * @param nprescalers: Size of prescaler array values,
     * @param prescalers: Array of potential prescaler values.
     * @param max_compare: Upper bound for timer compare value. Typically
     * `UINT8_MAX` for 8-bit timers and `UINT16_MAX` for 16-bit timers.
     * @param max_error: Upper error bound for computation. Causes the first
     * configuration satisfying the bound to be used. When set to 0.0, this
     * will examine every potential option and end with the best one.
     *
     * @returns (TimerRc): Return code with `Okay` or the error which occured.
     */
    TimerRc compute(size_t nprescalers, pre_t* prescalers, clk_t max_compare,
                    double max_error);

    /**
     * Pretty print the timer configuration. For debugging purposes only.
     */
    void pprint();
};

/**
 * Activate the 16-bit timer 1 on ATMEGA2560 with a given configuration.
 *
 * @param cfg: Computed timer configuration with desired clock frequency.
 *
 * @returns (TimerRc): Return code from operation.
 */
TimerRc activate_t1(TimerConfig& cfg);

/**
 * Deactivate 16-bit timer 1 on ATMEGA2560.
 */
void deactivate_t1();
