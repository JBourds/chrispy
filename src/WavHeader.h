#pragma once

#include <limits.h>
#include <stdint.h>

#include "Adc.h"

using adc::BitResolution;

#define U8_BITS CHAR_BIT
#define U16_BITS (2 * U8_BITS)

/**
 * Header of a standard PCM WAV file.
 *
 * Struct contains the metadata in a WAV header in the order it must appear.
 * Must be `filled` with information specific to a given recording instance
 * before being written.
 */
struct WavHeader {
    /**
     * RIFF chunk identifier ("RIFF").
     */
    const char chunk_id[4] = {'R', 'I', 'F', 'F'};
    /**
     * Size of (entire file in bytes - 8 bytes) or (data size + 36)
     * Gets rewritten after data is fully written to file.
     */
    uint32_t chunk_size = 36;
    /**
     * Format identifier (always "WAVE").
     */
    const char format[4] = {'W', 'A', 'V', 'E'};
    /**
     * Subchunk ID (always "fmt ").
     */
    const char subchunk_id[4] = {'f', 'm', 't', ' '};
    /**
     * Size of the "fmt " subchunk (always 16).
     */
    const uint32_t subchunk_size = 16;
    /**
     * Audio format code (PCM = 1).
     */
    const uint16_t audio_format = 1;
    /**
     * Number of channels.
     * 1 = Mono, 2 = Stereo
     * With the current implementation, each recording gets its own file and
     * thus makes this mono.
     */
    const uint16_t num_channels = 1;
    /**
     * Sampling rate in hertz (samples per second). Filled in later.
     */
    uint32_t sample_rate = 0;
    /**
     * Byte rate.
     *
     * SampleRate * NumChannels * BitsPerSample / 8.
     */
    uint32_t byte_rate = 0;
    /**
     * Byte alignment of each sample.
     *
     * NumChannels * BitsPerSample / 8
     */
    uint16_t block_align = 2;
    /**
     * Number of bits per sample. Rounded to next byte-increment, so 10-bit
     * or 12-bit audio would both become 16-bit.
     */
    uint16_t bits_per_sample;
    /**
     * Subchunk 2 ID. Always "data".
     */
    const char sub_chunk_2_id[4] = {'d', 'a', 't', 'a'};
    /**
     * Size of data chunk.
     *
     * NumSamples * NumChannels * BitsPerSample/8
     */
    uint32_t sub_chunk_2_size = 0;

    /**
     * Fill in WAV header fields once all details are known.
     *
     * @param res: Bit resolution of samples.
     * @param file_size: Size in bytes of the recording file.
     * @param sample_rate: Sample rate in hertz of audio recording.
     */
    void fill(BitResolution res, uint32_t file_size, uint32_t sample_rate);
};
