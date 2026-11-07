/**
 * \file            hash.c
 * \brief           SHA-256 identity for exact authored and fact input bytes
 * \author          Nexus Team
 * \version         0.1.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "internal.h"
#include <string.h>

static uint32_t rotate(uint32_t value, unsigned shift) {
    return (value >> shift) | (value << (32U - shift));
}

static void block(uint32_t state[8], const unsigned char input[64]) {
    static const uint32_t constants[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
        0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
        0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
        0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
        0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
        0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
        0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
        0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
        0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
    uint32_t words[64];
    for (size_t i = 0U; i < 16U; ++i) {
        words[i] = ((uint32_t)input[i * 4U] << 24U) |
                   ((uint32_t)input[i * 4U + 1U] << 16U) |
                   ((uint32_t)input[i * 4U + 2U] << 8U) |
                   (uint32_t)input[i * 4U + 3U];
    }
    for (size_t i = 16U; i < 64U; ++i) {
        uint32_t x = words[i - 15U];
        uint32_t y = words[i - 2U];
        uint32_t a = rotate(x, 7U) ^ rotate(x, 18U) ^ (x >> 3U);
        uint32_t b = rotate(y, 17U) ^ rotate(y, 19U) ^ (y >> 10U);
        words[i] = words[i - 16U] + a + words[i - 7U] + b;
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (size_t i = 0U; i < 64U; ++i) {
        uint32_t sum = rotate(e, 6U) ^ rotate(e, 11U) ^ rotate(e, 25U);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t first = h + sum + choice + constants[i] + words[i];
        uint32_t second = (rotate(a, 2U) ^ rotate(a, 13U) ^ rotate(a, 22U)) +
                          ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void nxconfig_sha256(const void* data, size_t size, char output[65]) {
    uint32_t state[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                         0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    const unsigned char* bytes = data;
    size_t full = size / 64U;
    for (size_t i = 0U; i < full; ++i)
        block(state, bytes + i * 64U);
    unsigned char tail[128] = {0};
    size_t remaining = size % 64U;
    memcpy(tail, bytes + full * 64U, remaining);
    tail[remaining] = 0x80U;
    size_t length = remaining < 56U ? 64U : 128U;
    uint64_t bits = (uint64_t)size * 8U;
    for (size_t i = 0U; i < 8U; ++i)
        tail[length - 1U - i] = (unsigned char)(bits >> (i * 8U));
    block(state, tail);
    if (length == 128U)
        block(state, tail + 64U);
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0U; i < 32U; ++i) {
        unsigned byte = (state[i / 4U] >> ((3U - i % 4U) * 8U)) & 255U;
        output[i * 2U] = hex[byte >> 4U];
        output[i * 2U + 1U] = hex[byte & 15U];
    }
    output[64] = '\0';
}
