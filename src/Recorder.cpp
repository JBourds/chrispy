#include "Recorder.h"

#include <Arduino.h>

#include "SdFunctions.cpp"

int64_t Recorder::record(const char *filenames[], BitResolution res,
                         uint32_t sample_rate, uint32_t duration_ms,
                         uint8_t *buf, size_t sz) {
    if (sd == nullptr) {
        return -1;
    } else if (this->nchannels > adc::MAX_CHANNEL_COUNT) {
        return -2;
    }

    // This number is bounded by `MAX_CHANNEL_COUNT` so the VLA is alright
    SdFile files[nchannels];
    // Create files and write blank headers
    WavHeader hdr;
    for (size_t i = 0; i < nchannels; ++i) {
        if (!(files[i].open(filenames[i], O_TRUNC | O_WRITE | O_CREAT) &&
              files[i].write(&hdr, sizeof(hdr)) == sizeof(hdr))) {
            close_all(files, nchannels);
            return -3;
        }
    }

    adc::init(nchannels, channels, buf, sz);
    uint8_t *tmp_buf = nullptr;
    size_t tmp_sz = 0;
    size_t ch_index = 0;
    if (adc::start(res, sample_rate) != 0) {
        return -4;
    }
    uint32_t deadline = millis() + duration_ms;
    while (millis() <= deadline) {
        if (adc::swap_buffer(&tmp_buf, tmp_sz, ch_index) == 0) {
            if (tmp_buf == nullptr) {
                continue;
            }
            size_t nwritten = files[ch_index].write(tmp_buf, tmp_sz);
            if (nwritten != tmp_sz) {
                adc::stop();
                close_all(files, nchannels);
                return -5;
            }
        }
    }
    uint32_t ncollected = adc::stop();
    while (adc::drain_buffer(&tmp_buf, tmp_sz, ch_index) == 0) {
        if (tmp_buf == nullptr) {
            continue;
        }
        size_t nwritten = files[ch_index].write(tmp_buf, tmp_sz);
        if (nwritten != tmp_sz) {
            close_all(files, nchannels);
            return -6;
        }
    }

    // Make all files the exact same size then write out WAV header
    int64_t rc = truncate_to_smallest(files, nchannels);
    if (rc < 0) {
        close_all(files, nchannels);
        return -7;
    }
    uint32_t file_size = static_cast<uint32_t>(rc);
    uint32_t per_ch_sample_rate =
        ncollected / (nchannels * duration_ms / 1000.0);
    hdr.fill(res, file_size, per_ch_sample_rate);
    for (size_t i = 0; i < nchannels; ++i) {
        if (!(files[i].seekSet(0) &&
              files[i].write(&hdr, sizeof(hdr)) == sizeof(hdr))) {
            close_all(files, nchannels);
            return -8;
        }
    }

    close_all(files, nchannels);
    return 0;
}
