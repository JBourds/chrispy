#include "Adc.h"

#include <Arduino.h>
#include <SdFat.h>
#include <stddef.h>
#include <stdint.h>

namespace adc {

#define TEN_BIT_BIAS 0x1FF
#define TEN_TO_SIXTEEN_BIT(x) (((x) - TEN_BIT_BIAS) << 6)

// Bit masks
#define MUX_MASK 0b11111
#define SOURCE_MASK 0b111
#define LOW_CHANNEL_MASK 0b111
#define HIGH_CHANNEL_MASK ~LOW_CHANNEL_MASK
#define PRESCALER_MASK 0b111

// Prescaler masks
#define DIV_128 0b111
#define DIV_64 0b110
#define DIV_32 0b101
#define DIV_16 0b100
#define DIV_8 0b011
#define DIV_4 0b010
#define DIV_2 0b001
#define DIV_2_2 0b000

#define MIN_BUF_SZ_PER_CHANNEL 512

#define ADC_CYCLES_PER_SAMPLE 13.5

const size_t MAX_CHANNEL_COUNT = 16;

// ISR helper functions
static int8_t init_frame(BitResolution res, size_t ch_window_sz);
static inline bool activate_adc_channel(Channel& ch);
static bool increment_channel_buffer_index();

// Internal ADC control functions
static uint8_t prescaler_mask(pre_t val);
static void enable_interrupts();
static void disable_interrupts();
static void enable_autotrigger();
static void disable_autotrigger();
static void set_source(AutotriggerSource src);
static TimerRc set_frequency(uint32_t sample_rate);
static void configure_channels(size_t nchannels, Channel* channels);

static const size_t NPRESCALERS = 7;
static const pre_t PRESCALERS[] = {2, 4, 8, 16, 32, 64, 128};
static pre_t SCRATCH_PRESCALERS[NPRESCALERS];

// Global static used when swapping/draining buffers.
// Needs to be global so it also gets reset when resetting ISR frame.
static size_t CH_BUFFER_INDEX = 0;

/**
 * Private/static data member for use by the ISR.
 *
 * Stores the configuration state passed in via public module functions
 * and metadata used by the ISR.
 */
static struct AdcFrame {
    /* !< Currently active channel */
    volatile size_t ch_index;
    /* !< Current sample index within the channel sub-buffer */
    volatile size_t sample_index;
    /* !< Channel `ch_index` sub-buffer */
    volatile uint8_t* ch_buffer;

    // Flags with frame state

    /* !< "Index" into the double buffer */
    volatile bool using_buf_1;
    /* !< Flag indicating if the first buffer is full */
    volatile bool buf1full;
    /* !< Flag indicating if the second buffer is full */
    volatile bool buf2full;

    /* !< Flag indicating an error when switching between channels */
    volatile bool ch_error;

    /* !< First buffer being written to on ADC interrupts. */
    uint8_t* buf1;
    /* !< Second buffer being written to on ADC interrupts. */
    uint8_t* buf2;

    /* !< Number of channels in the `channels` array - 1 (save subtractions). */
    size_t max_ch_index;
    /* !< Number of samples to collect for a channel before swapping to the
     * next.
     */
    size_t ch_window_sz;
    /* !< `ch_window_sz` - 1. Cached result to save subtractions in ISR. This
     * is a valid mask because we ensure the window size is a power of 2.*/
    size_t ch_window_mask;
    /* !< Number of bytes per channel buffer */
    size_t ch_buf_sz;

    /* !< Number of samples collected */
    volatile uint32_t collected;
    /* !< Flag for whether the frame is currently in use */
    bool active;
    /* !< Bit resolution for samples */
    BitResolution res;
} FRAME;

/**
 * Singleton instance of the ADC.
 */
static struct Adc {
    uint8_t nchannels;
    Channel* channels;
    uint8_t* buf;
    size_t sz;
    BitResolution res;
    bool initialized = false;
} INSTANCE;

/**
 * Interrupt service routine responsible for reading samples from the ADC.
 */
ISR(ADC_vect) {
    // 1) Immediately reenable timer so we don't miss a beat
    TIFR1 = UINT8_MAX;

    // 2) Check that we can actually perform work
    if (!FRAME.active) {
        return;
    } else if (FRAME.buf1full && FRAME.buf2full) {
        return;
    } else if (FRAME.ch_error) {
        return;
    }

    // 3) Read the sample
    if (FRAME.res == BitResolution::Eight) {
        FRAME.ch_buffer[FRAME.sample_index++] = ADCH;
    } else {
        uint8_t low = ADCL;
        uint8_t high = ADCH;
        uint16_t new_sample = TEN_TO_SIXTEEN_BIT((high << CHAR_BIT) | low);
        FRAME.ch_buffer[FRAME.sample_index++] = new_sample & UINT8_MAX;
        FRAME.ch_buffer[FRAME.sample_index++] = new_sample >> CHAR_BIT;
    }
    ++FRAME.collected;

    // 4) Swap buffer we are writing to if we just filled the current one up
    if (FRAME.sample_index == FRAME.ch_buf_sz &&
        FRAME.ch_index == FRAME.max_ch_index) {
        if (FRAME.using_buf_1) {
            FRAME.buf1full = true;
        } else {
            FRAME.buf2full = true;
        }
        FRAME.using_buf_1 = !FRAME.using_buf_1;
        FRAME.sample_index = 0;
        FRAME.ch_index = 0;
        FRAME.ch_buffer = FRAME.using_buf_1 ? FRAME.buf1 : FRAME.buf2;
    }

    // 5) Swap channels if it is time to
    if (FRAME.max_ch_index > 0 && FRAME.sample_index > 0 &&
        (FRAME.sample_index & FRAME.ch_window_mask) == 0) {
        if (FRAME.ch_index == FRAME.max_ch_index) {
            FRAME.ch_index = 0;
        } else {
            ++FRAME.ch_index;
            FRAME.sample_index -= FRAME.ch_window_sz;
        }
        if (!activate_adc_channel(INSTANCE.channels[FRAME.ch_index])) {
            FRAME.ch_error = true;
        }
        uint8_t* base = FRAME.using_buf_1 ? FRAME.buf1 : FRAME.buf2;
        FRAME.ch_buffer = base + FRAME.ch_index * FRAME.ch_buf_sz;
    }
}

int8_t drain_buffer(uint8_t** buf, size_t& sz, size_t& ch_index) {
    if (buf == nullptr) {
        return -1;
    } else if (FRAME.active) {
        return -2;
    }

    // Drain full buffers first. Preserve old index in case this doesn't find
    // a full buffer but mutates global.
    int8_t rc = swap_buffer(buf, sz, ch_index);
    if (rc == 0) {
        return rc;
    }

    // Only drain samples if we have any full windows to check
    size_t bytes_per_sample = FRAME.res == BitResolution::Eight ? 1 : 2;
    size_t window_sz_bytes = FRAME.ch_window_sz * bytes_per_sample;
    if (FRAME.sample_index < window_sz_bytes) {
        return -3;
    }

    sz = FRAME.sample_index & ~(window_sz_bytes - 1);
    ch_index = CH_BUFFER_INDEX;
    *buf = FRAME.using_buf_1 ? FRAME.buf1 : FRAME.buf2;
    *buf += sz * ch_index;

    // Once this wraps around, reset the sample index so subsequent calls fail.
    if (increment_channel_buffer_index()) {
        FRAME.sample_index = 0;
    }

    return 0;
}

int8_t swap_buffer(uint8_t** buf, size_t& sz, size_t& ch_index) {
    if (buf == nullptr) {
        return -1;
    } else if (!(FRAME.buf1full || FRAME.buf2full)) {
        return -2;
    }

    // Case 1) Not returning a pointer. Looking for channel 0 in full buffer.
    if (*buf == nullptr) {
        if (FRAME.buf1full && FRAME.buf2full) {
            // If both are full, the flag will indicate the next buffer to
            // be written while is also the oldest uncleared one
            *buf = FRAME.using_buf_1 ? FRAME.buf1 : FRAME.buf2;
        } else if (FRAME.buf1full) {
            *buf = FRAME.buf1;
        } else if (FRAME.buf2full) {
            *buf = FRAME.buf2;
        } else {
            return -3;
        }
        ch_index = CH_BUFFER_INDEX;
        sz = FRAME.ch_buf_sz;
        return 0;
    }

    // Case 2) Returning some pointer.
    increment_channel_buffer_index();
    ch_index = CH_BUFFER_INDEX;
    sz = FRAME.ch_buf_sz;
    if (ch_index == 0) {
        // Case 2.1) Incrementing channel index wrapped around, so this was the
        // last channel buffer. Figure out if it came from buf1 or buf2 and
        // return it.
        bool from_buf_1 = *buf < FRAME.buf2;
        if (from_buf_1) {
            FRAME.buf1full = false;
            *buf = FRAME.buf2full ? FRAME.buf2 : nullptr;
        } else {
            FRAME.buf2full = false;
            *buf = FRAME.buf1full ? FRAME.buf1 : nullptr;
        }
        if (*buf == nullptr) {
            return -4;
        }
    } else {
        // Case 2.2) Not the last channel buffer, increment to the next one.
        *buf += FRAME.ch_buf_sz;
    }
    return 0;
}

bool init(uint8_t nchannels, Channel* channels, uint8_t* buf, size_t sz) {
    if (nchannels > MAX_CHANNEL_COUNT || FRAME.active) {
        return false;
    }

    INSTANCE.nchannels = nchannels;
    INSTANCE.channels = channels;
    INSTANCE.buf = buf;
    INSTANCE.sz = sz;
    INSTANCE.initialized = true;
    return true;
}

void on() {
    PRR0 &= ~(1 << PRADC);
    ADCSRA |= (1 << ADEN);
}

void off() { ADCSRA &= ~(1 << ADEN); }

void sleep() {
    off();
    PRR0 |= (1 << PRADC);
}

int8_t start(BitResolution res, uint32_t sample_rate, size_t ch_window_sz,
             uint32_t warmup_ms) {
    if (!INSTANCE.initialized) {
        return -1;
    }
    int8_t rc = init_frame(res, ch_window_sz);
    if (rc) {
        return -2;
    }
    INSTANCE.res = res;

    on();
    configure_channels(INSTANCE.nchannels, INSTANCE.channels);
    set_source(AutotriggerSource::TimCnt1CmpB);
    set_frequency(sample_rate);
    // 5V analog reference
    ADMUX = (1 << REFS0);
    // Start with first channel
    if (!activate_adc_channel(INSTANCE.channels[0])) {
        return -3;
    }
    // Left adjust result so we can just read from ADCH in ISR
    if (res == BitResolution::Eight) {
        ADMUX |= (1 << ADLAR);
    }
    // Slight delay to allow channels to settle
    FRAME.active = false;
    enable_autotrigger();
    enable_interrupts();
    delay(warmup_ms);
    FRAME.active = true;

    return 0;
}

uint32_t collected() { return FRAME.collected; }

uint32_t stop() {
    off();
    disable_interrupts();
    disable_autotrigger();
    deactivate_t1();
    uint32_t collected = FRAME.collected;
    FRAME.active = false;
    return collected;
}

static void enable_interrupts() { ADCSRA |= (1 << ADIE); }

static void disable_interrupts() { ADCSRA &= ~(1 << ADIE); }

static void enable_autotrigger() { ADCSRA |= (1 << ADATE); }

static void disable_autotrigger() { ADCSRA &= ~(1 << ADATE); }

static void set_source(enum AutotriggerSource src) {
    ADCSRB &= ~SOURCE_MASK;
    ADCSRB |= static_cast<uint8_t>(src);
}

static void configure_channels(size_t nchannels, Channel* channels) {
    for (size_t i = 0; i < nchannels; ++i) {
        if (channels[i].power >= 0) {
            pinMode(channels[i].pin, INPUT);
            digitalWrite(channels[i].power,
                         channels[i].active_high ? HIGH : LOW);
        }
    }
}

static TimerRc set_frequency(uint32_t sample_rate) {
    // For each channel
    sample_rate *= INSTANCE.nchannels;

    TimerConfig host_cfg(F_CPU, sample_rate, Skew::High);
    TimerRc rc = activate_t1(host_cfg);
    if (rc != TimerRc::Okay && rc != TimerRc::ErrorRange) {
        return rc;
    }

    // Set overflow match on A and B so count resets (uses A) and triggers
    // interrupt when it does so (match on B)
    cli();
    OCR1A = host_cfg.compare;
    OCR1B = host_cfg.compare;
    sei();

    clk_t adc_rate = ADC_CYCLES_PER_SAMPLE * sample_rate;
    // Account for the portion of the time spent switching to the next channel
    if (INSTANCE.nchannels > 1) {
        adc_rate += adc_rate;
    }
    TimerConfig adc_cfg(F_CPU, adc_rate, Skew::High);
    memcpy(SCRATCH_PRESCALERS, PRESCALERS, sizeof(PRESCALERS));
    rc = adc_cfg.compute(NPRESCALERS, SCRATCH_PRESCALERS, 1, 0.0);
    if (rc != TimerRc::Okay && rc != TimerRc::ErrorRange) {
        return rc;
    }
    uint8_t prescaler = prescaler_mask(adc_cfg.prescaler);
    ADCSRA &= ~PRESCALER_MASK;
    ADCSRA |= prescaler;
    return rc;
}

int8_t Channel::mux_mask() {
    switch (pin) {
        case A0:
            return 0b000;
        case A1:
            return 0b001;
        case A2:
            return 0b010;
        case A3:
            return 0b011;
        case A4:
            return 0b100;
        case A5:
            return 0b101;
        case A6:
            return 0b110;
        case A7:
            return 0b111;
        default:
            return -1;
    }
}

static int8_t init_frame(BitResolution res, size_t ch_window_sz) {
    size_t ch_window_mask = ch_window_sz - 1;
    if (INSTANCE.nchannels < 1) {
        return -1;
    } else if (ch_window_sz == 0) {
        return -2;
    } else if (ch_window_sz & ch_window_mask) {
        // Channel window size needs to be a power of 2
        return -3;
    } else if (INSTANCE.sz < MIN_BUF_SZ_PER_CHANNEL * INSTANCE.nchannels) {
        return -4;
    }

    const size_t nbuffers = 2;
    size_t bytes_per_sample = res == BitResolution::Eight ? 1 : 2;
    size_t samples_per_buf = INSTANCE.sz / (nbuffers * bytes_per_sample);
    size_t samples_per_ch_buf = samples_per_buf / INSTANCE.nchannels;
    // Shrink channel buffers if needed to get increment of window size
    size_t window_increment_delta = samples_per_ch_buf & ch_window_mask;
    if (window_increment_delta == samples_per_ch_buf) {
        return -5;
    }
    samples_per_ch_buf -= window_increment_delta;

    memset(&FRAME, 0, sizeof(FRAME));

    FRAME.res = res;

    // Slice up the buffer into a double buffer
    FRAME.buf1 = INSTANCE.buf;
    FRAME.buf2 = INSTANCE.buf + samples_per_buf * bytes_per_sample;

    FRAME.max_ch_index = INSTANCE.nchannels - 1;
    FRAME.ch_window_sz = ch_window_sz;
    FRAME.ch_window_mask = ch_window_mask;
    FRAME.ch_buffer = FRAME.buf1;
    FRAME.ch_buf_sz = samples_per_ch_buf * bytes_per_sample;

    FRAME.using_buf_1 = true;
    FRAME.active = true;

    CH_BUFFER_INDEX = 0;

    return 0;
}

static inline bool activate_adc_channel(Channel& ch) {
    int8_t mask = ch.mux_mask();
    if (mask < 0) {
        return false;
    }
    ADMUX &= ~MUX_MASK;
    ADMUX |= mask;
    // If the mask comes from a higher channel (>7) set MUX5
    if (mask & HIGH_CHANNEL_MASK) {
        ADCSRB |= (1 << MUX5);
    } else {
        ADCSRB &= ~(1 << MUX5);
    }
    return true;
}

/**
 * Perform wrapping index addition.
 * @returns (bool): True when last channel buffer was encoutnered.
 * False otherwise.
 */
static inline bool increment_channel_buffer_index() {
    if (CH_BUFFER_INDEX == FRAME.max_ch_index) {
        CH_BUFFER_INDEX = 0;
        return true;
    } else {
        ++CH_BUFFER_INDEX;
        return false;
    }
}

static uint8_t prescaler_mask(pre_t val) {
    switch (val) {
        case 2:
            return DIV_2;
        case 4:
            return DIV_4;
        case 8:
            return DIV_8;
        case 16:
            return DIV_16;
        case 32:
            return DIV_32;
        case 64:
            return DIV_64;
        case 128:
            return DIV_128;
        default:
            return 0;
    }
}
}  // namespace adc
