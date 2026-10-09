#ifndef CONFIG_WIRE_H
#define CONFIG_WIRE_H
#include "config/config_def.h"
#include <float.h>
#include <string.h>
#define CONFIG_BINARY_MAGIC 0x43464742u
#define CONFIG_BINARY_VERSION 2u
#define CONFIG_BINARY_HEADER_SIZE 16u
#define CONFIG_BINARY_ENTRY_SIZE 6u
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "Config binary v2 requires IEEE754 binary32");
static inline bool config_utf8_valid(const uint8_t* text, size_t size) {
    size_t position = 0;
    while (position < size) {
        uint8_t lead = text[position++];
        if (lead < 0x80) continue;
        size_t count = lead >= 0xc2 && lead <= 0xdf ? 1u :
                       lead >= 0xe0 && lead <= 0xef ? 2u :
                       lead >= 0xf0 && lead <= 0xf4 ? 3u : 0u;
        if (!count || count > size - position) return false;
        uint8_t second = text[position];
        if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second >= 0xa0) ||
            (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second >= 0x90)) return false;
        while (count--) {
            uint8_t byte = text[position++];
            if (byte < 0x80 || byte > 0xbf) return false;
        }
    }
    return true;
}
static inline void config_wire_put(uint8_t* output, uint64_t value, size_t size) {
    for (size_t i = 0; i < size; ++i) output[i] = (uint8_t)(value >> (8u * i));
}
static inline uint64_t config_wire_get(const uint8_t* input, size_t size) {
    uint64_t value = 0;
    for (size_t i = 0; i < size; ++i) value |= (uint64_t)input[i] << (8u * i);
    return value;
}
/* Endian transform only scalar values. Strings, blobs and authenticated records
 * are exact bytes. No native C layout or padding is part of the wire format. */
static inline bool config_wire_value(config_type_t type, uint8_t flags,
    const uint8_t* input, size_t size, uint8_t* output, bool encoding) {
    if ((flags & CONFIG_FLAG_ENCRYPTED) || type == CONFIG_TYPE_STRING || type == CONFIG_TYPE_BLOB) {
        memcpy(output, input, size); return true;
    }
    if (type == CONFIG_TYPE_BOOL) {
        if (size != 1 || input[0] > 1) return false;
        output[0] = input[0]; return true;
    }
    size_t expected = type == CONFIG_TYPE_I64 ? 8u :
        type == CONFIG_TYPE_I32 || type == CONFIG_TYPE_U32 || type == CONFIG_TYPE_FLOAT ? 4u : 0u;
    if (!expected || size != expected) return false;
    if (encoding) {
        uint64_t value = 0;
        if (size == 4) { uint32_t word; memcpy(&word, input, 4); value = word; }
        else memcpy(&value, input, 8);
        config_wire_put(output, value, size);
    } else {
        uint64_t value = config_wire_get(input, size);
        if (size == 4) { uint32_t word = (uint32_t)value; memcpy(output, &word, 4); }
        else memcpy(output, &value, 8);
    }
    return true;
}
#endif
