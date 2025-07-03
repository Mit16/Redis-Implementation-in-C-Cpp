#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

// intrusive data structure
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// FNV hash
inline uint64_t str_hash(const uint8_t *data, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL; // 64-bit FNV offset basis
    for (size_t i = 0; i < len; i++) {
        h ^= data[i]; // XOR instead of addition
        h *= 0x100000001b3ULL; // 64-bit FNV prime
    }
    return h;
}

#endif // COMMON_H