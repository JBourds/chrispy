#include "WavHeader.h"

void WavHeader::fill(BitResolution res, uint32_t file_size,
                     uint32_t sample_rate) {
    if (res == BitResolution::Eight) {
        this->bits_per_sample = U8_BITS;
    } else {
        this->bits_per_sample = U16_BITS;
    }
    this->chunk_size = file_size - sizeof(chunk_id) - sizeof(chunk_size);
    this->sample_rate = sample_rate;
    this->byte_rate = sample_rate * num_channels * bits_per_sample / U8_BITS;
    this->block_align = num_channels * this->bits_per_sample / U8_BITS;
    this->sub_chunk_2_size = file_size - sizeof(WavHeader);
}
