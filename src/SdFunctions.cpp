
#include "SdFunctions.h"

#include <limits.h>

int32_t close_all(SdFile files[], size_t nfiles) {
    for (size_t i = 0; i < nfiles; ++i) {
        if (files[i].isOpen() && !files[i].close()) {
            return i + 1;
        }
    }
    return 0;
}

int64_t truncate_to_smallest(SdFile files[], size_t nfiles) {
    if (nfiles == 0 || files == nullptr) {
        return -1;
    }
    // Make sure each recording is exactly the same length
    uint64_t min_size = UINT32_MAX;
    for (size_t i = 0; i < nfiles; ++i) {
        if (!files[i].isOpen()) {
            return -2;
        }
        min_size = min(files[i].fileSize(), min_size);
    }
    for (size_t i = 0; i < nfiles; ++i) {
        if (!files[i].truncate(min_size)) {
            return -3;
        }
    }
    return static_cast<int64_t>(min_size);
}
