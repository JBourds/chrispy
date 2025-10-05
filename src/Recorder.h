#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Adc.h"

// TODO: Conditional compilation with SD library
#include "SdFat.h"

namespace recording {
/**
 * Initialize recorder with these fields.
 *
 * @param nchannels: Number of channels to expect in `mic_pins` and
 * `power_pins`.
 * @param channels: Array of channels to record on.
 * @param sd: SD card to use for recording.
 */
bool init(uint8_t nchannels, adc::Channel *channels, SdFat *sd);

/**
 * Uses the SD singleton to record to every file in `files` with the same
 * sample rate and duration. Truncates all files to be equal to the shortest
 * lengths.
 *
 * Invariants:
 *  - SD is initialized and in the directory recordings should go.
 *  - `files` is of at least `channels` length and contains valid filenames.
 *
 * @param filenames: Array of filenames to record to. Must be at least as
 * long as `nchannels`.
 * @param res: Bit resolution to record at.
 * @param sample_rate: Requested sample rate for each channel.
 * @param duration_ms: Length in milliseconds to record for.
 * @param buf: Buffer allocated to receive ADC samples.
 * @param sz: Buffer size.
 *
 * @returns (int32_t): The file size in bytes if successful. Returns a
 * negative value if there is an error.
 */
int64_t record(const char *filenames[], BitResolution res, uint32_t sample_rate,
               uint32_t duration_ms, uint8_t *buf, size_t sz);
};  // namespace recording
