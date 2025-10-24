#include "Recorder.h"

#include <Arduino.h>
#include <SdFat.h>

#include "SdFunctions.cpp"
#include "WavHeader.h"

namespace recording {

static struct {
    adc::Channel *channels;
    SdFat *sd;
    uint8_t nchannels;
    bool initialized = false;
} INSTANCE;

bool init(uint8_t nchannels, adc::Channel *channels, SdFat *sd) {
    if (sd == nullptr) {
        return false;
    }
    INSTANCE.nchannels = nchannels;
    INSTANCE.channels = channels;
    INSTANCE.sd = sd;
    INSTANCE.initialized = true;
    return true;
}

int64_t record(const char *filenames[], BitResolution res, uint32_t sample_rate,
               uint32_t duration_ms, uint8_t *buf, size_t sz) {
    if (!INSTANCE.initialized) {
        return -1;
    } else if (INSTANCE.nchannels > adc::MAX_CHANNEL_COUNT) {
        return -2;
    }

    // This number is bounded by `MAX_CHANNEL_COUNT` so the VLA is alright
    SdFile files[INSTANCE.nchannels];
    // Create files and write blank headers
    WavHeader hdr;
    for (size_t i = 0; i < INSTANCE.nchannels; ++i) {
        if (!(files[i].open(filenames[i], O_TRUNC | O_WRITE | O_CREAT) &&
              files[i].write(&hdr, sizeof(hdr)) == sizeof(hdr))) {
            close_all(files, INSTANCE.nchannels);
            return -3;
        }
    }

    adc::init(INSTANCE.nchannels, INSTANCE.channels, buf, sz);
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
                close_all(files, INSTANCE.nchannels);
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
            close_all(files, INSTANCE.nchannels);
            return -6;
        }
    }

    // Make all files the exact same size then write out WAV header
    int64_t rc = truncate_to_smallest(files, INSTANCE.nchannels);
    if (rc < 0) {
        close_all(files, INSTANCE.nchannels);
        return -7;
    }
    uint32_t file_size = static_cast<uint32_t>(rc);
    uint32_t per_ch_sample_rate =
        ncollected / (INSTANCE.nchannels * duration_ms / 1000.0);
    hdr.fill(res, file_size, per_ch_sample_rate);
    for (size_t i = 0; i < INSTANCE.nchannels; ++i) {
        if (!(files[i].seekSet(0) &&
              files[i].write(&hdr, sizeof(hdr)) == sizeof(hdr))) {
            close_all(files, INSTANCE.nchannels);
            return -8;
        }
    }

    close_all(files, INSTANCE.nchannels);
    return 0;
}
}  // namespace recording
