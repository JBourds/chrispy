
#include <stddef.h>
#include <stdint.h>

#include "SdFat.h"

int32_t close_all(SdFile files[], size_t nfiles);
int64_t truncate_to_smallest(SdFile files[], size_t nfiles);
