
#include <limits.h>
#include <stdint.h>

#define U8_BITS CHAR_BIT
#define U16_BITS (2 * U8_BITS)

enum struct BitResolution : uint8_t {
    Eight = 8,
    Ten = 10,
    Twelve = 12,
};

struct WavHeader {
    const char chunk_id[4] = {'R', 'I', 'F', 'F'};
    // Size of (entire file in bytes - 8 bytes) or (data size + 36)
    // Needs to be rewritten after data is fully written to file
    uint32_t chunk_size = 36;
    const char format[4] = {'W', 'A', 'V', 'E'};
    const char subchunk_id[4] = {'f', 'm', 't', ' '};
    const uint32_t subchunk_size = 16;
    // PCM == 1
    const uint16_t audio_format = 1;
    // 1=Mono, 2=Stereo
    // This is always one since each mic maps to its own file
    const uint16_t num_channels = 1;
    // Samples per second
    uint32_t sample_rate = 0;
    //== SampleRate * NumChannels * BitsPerSample/8
    uint32_t byte_rate = 0;
    //== NumChannels * BitsPerSample/8
    const uint16_t block_align = 2;
    // 10-bit adc requires two bytes per sample
    uint16_t bits_per_sample;
    const char sub_chunk_2_id[4] = {'d', 'a', 't', 'a'};
    //== NumSamples * NumChannels * BitsPerSample/8
    uint32_t sub_chunk_2_size = 0;
    // Audio data gets written after this header

    WavHeader(BitResolution res, uint32_t file_size, uint32_t sample_rate) {
        if (res == BitResolution::Eight) {
            this->bits_per_sample = U8_BITS;
        } else {
            this->bits_per_sample = U16_BITS;
        }
        this->chunk_size = file_size - sizeof(chunk_id) - sizeof(chunk_size);
        this->sample_rate = sample_rate;
        this->byte_rate =
            sample_rate * num_channels * bits_per_sample / U8_BITS;
        this->sub_chunk_2_size = chunk_size - sizeof(WavHeader);
    }
};
