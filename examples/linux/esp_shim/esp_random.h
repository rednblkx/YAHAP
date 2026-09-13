#pragma once

#include <sodium.h>

inline void esp_fill_random(void* buf, size_t len) {
    randombytes(static_cast<unsigned char*>(buf), len);
}
