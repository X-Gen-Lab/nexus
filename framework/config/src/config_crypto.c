/**
 * \file            config_crypto.c
 * \brief           Config Manager Crypto Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements AES-128/256 encryption for config values.
 *                  Uses a simple software AES implementation for portability.
 *                  Requirements: 12.3, 12.4, 12.5
 */

#include "config_crypto.h"
#include "config/config.h"
#include "config_store.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* AES Constants                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           AES S-box for SubBytes transformation
 */
static const uint8_t aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
    0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
    0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
    0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
    0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
    0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
    0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
    0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
    0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
    0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
    0xb0, 0x54, 0xbb, 0x16};

/**
 * \brief           AES inverse S-box for InvSubBytes transformation
 */
static const uint8_t aes_inv_sbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e,
    0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
    0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32,
    0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
    0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50,
    0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05,
    0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
    0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41,
    0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8,
    0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
    0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
    0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59,
    0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
    0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d,
    0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63,
    0x55, 0x21, 0x0c, 0x7d};

/**
 * \brief           AES round constants
 */
static const uint8_t aes_rcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10,
                                     0x20, 0x40, 0x80, 0x1b, 0x36};

/*---------------------------------------------------------------------------*/
/* Crypto Context                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Crypto context structure
 */
typedef struct {
    bool enabled;                            /**< Encryption enabled */
    config_crypto_algo_t algo;               /**< Algorithm */
    uint8_t key[CONFIG_CRYPTO_MAX_KEY_SIZE]; /**< Encryption key */
    size_t key_len;                          /**< Key length */
    uint8_t expanded_key[240];               /**< Expanded key schedule */
    uint8_t num_rounds;                      /**< Number of AES rounds */
} config_crypto_ctx_t;

/**
 * \brief           Global crypto context
 */
static config_crypto_ctx_t g_crypto_ctx;

/**
 * \brief           Simple PRNG state for IV generation
 */
static uint32_t g_prng_state = 0x12345678;

/*---------------------------------------------------------------------------*/
/* Internal Helper Functions                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simple PRNG for IV generation
 * \return          Random byte
 */
static uint8_t prng_next(void) {
    g_prng_state ^= g_prng_state << 13;
    g_prng_state ^= g_prng_state >> 17;
    g_prng_state ^= g_prng_state << 5;
    return (uint8_t)(g_prng_state & 0xFF);
}

/**
 * \brief           Generate random IV
 * \param[out]      iv: Output IV buffer (16 bytes)
 */
static void generate_iv(uint8_t* iv) {
    for (size_t i = 0; i < CONFIG_CRYPTO_IV_SIZE; ++i) {
        iv[i] = prng_next();
    }
}

/*---------------------------------------------------------------------------*/
/* AES Core Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Galois field multiplication by 2
 */
static uint8_t gf_mul2(uint8_t x) {
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

/**
 * \brief           Galois field multiplication by 3
 */
static uint8_t gf_mul3(uint8_t x) {
    return gf_mul2(x) ^ x;
}

/**
 * \brief           Galois field multiplication
 */
static uint8_t gf_mul(uint8_t a, uint8_t b) {
    uint8_t result = 0;
    uint8_t hi_bit;
    for (int i = 0; i < 8; ++i) {
        if (b & 1) {
            result ^= a;
        }
        hi_bit = a & 0x80;
        a <<= 1;
        if (hi_bit) {
            a ^= 0x1b;
        }
        b >>= 1;
    }
    return result;
}

/**
 * \brief           AES key expansion
 * \param[in]       key: Input key
 * \param[in]       key_len: Key length (16 or 32)
 * \param[out]      expanded: Expanded key schedule
 * \param[out]      num_rounds: Number of rounds
 */
static void aes_key_expansion(const uint8_t* key, size_t key_len,
                              uint8_t* expanded, uint8_t* num_rounds) {
    size_t nk = key_len / 4;               /* Number of 32-bit words in key */
    size_t nr = (key_len == 16) ? 10 : 14; /* Number of rounds */
    size_t nb = 4;                         /* Block size in 32-bit words */

    *num_rounds = (uint8_t)nr;

    /* Copy original key */
    memcpy(expanded, key, key_len);

    /* Expand key */
    size_t i = nk;
    uint8_t temp[4];

    while (i < nb * (nr + 1)) {
        /* Copy previous word */
        memcpy(temp, &expanded[(i - 1) * 4], 4);

        if (i % nk == 0) {
            /* RotWord */
            uint8_t t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;

            /* SubWord */
            temp[0] = aes_sbox[temp[0]];
            temp[1] = aes_sbox[temp[1]];
            temp[2] = aes_sbox[temp[2]];
            temp[3] = aes_sbox[temp[3]];

            /* XOR with Rcon */
            temp[0] ^= aes_rcon[i / nk];
        } else if (nk > 6 && i % nk == 4) {
            /* SubWord for AES-256 */
            temp[0] = aes_sbox[temp[0]];
            temp[1] = aes_sbox[temp[1]];
            temp[2] = aes_sbox[temp[2]];
            temp[3] = aes_sbox[temp[3]];
        }

        /* XOR with word nk positions earlier */
        expanded[i * 4 + 0] = expanded[(i - nk) * 4 + 0] ^ temp[0];
        expanded[i * 4 + 1] = expanded[(i - nk) * 4 + 1] ^ temp[1];
        expanded[i * 4 + 2] = expanded[(i - nk) * 4 + 2] ^ temp[2];
        expanded[i * 4 + 3] = expanded[(i - nk) * 4 + 3] ^ temp[3];

        ++i;
    }
}

/**
 * \brief           AES AddRoundKey transformation
 */
static void aes_add_round_key(uint8_t* state, const uint8_t* round_key) {
    for (int i = 0; i < 16; ++i) {
        state[i] ^= round_key[i];
    }
}

/**
 * \brief           AES SubBytes transformation
 */
static void aes_sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; ++i) {
        state[i] = aes_sbox[state[i]];
    }
}

/**
 * \brief           AES InvSubBytes transformation
 */
static void aes_inv_sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; ++i) {
        state[i] = aes_inv_sbox[state[i]];
    }
}

/**
 * \brief           AES ShiftRows transformation
 */
static void aes_shift_rows(uint8_t* state) {
    uint8_t temp;

    /* Row 1: shift left by 1 */
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;

    /* Row 2: shift left by 2 */
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    /* Row 3: shift left by 3 (= right by 1) */
    temp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = temp;
}

/**
 * \brief           AES InvShiftRows transformation
 */
static void aes_inv_shift_rows(uint8_t* state) {
    uint8_t temp;

    /* Row 1: shift right by 1 */
    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;

    /* Row 2: shift right by 2 */
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    /* Row 3: shift right by 3 (= left by 1) */
    temp = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = temp;
}

/**
 * \brief           AES MixColumns transformation
 */
static void aes_mix_columns(uint8_t* state) {
    for (int i = 0; i < 4; ++i) {
        uint8_t* col = &state[i * 4];
        uint8_t a = col[0], b = col[1], c = col[2], d = col[3];

        col[0] = gf_mul2(a) ^ gf_mul3(b) ^ c ^ d;
        col[1] = a ^ gf_mul2(b) ^ gf_mul3(c) ^ d;
        col[2] = a ^ b ^ gf_mul2(c) ^ gf_mul3(d);
        col[3] = gf_mul3(a) ^ b ^ c ^ gf_mul2(d);
    }
}

/**
 * \brief           AES InvMixColumns transformation
 */
static void aes_inv_mix_columns(uint8_t* state) {
    for (int i = 0; i < 4; ++i) {
        uint8_t* col = &state[i * 4];
        uint8_t a = col[0], b = col[1], c = col[2], d = col[3];

        col[0] = gf_mul(a, 0x0e) ^ gf_mul(b, 0x0b) ^ gf_mul(c, 0x0d) ^
                 gf_mul(d, 0x09);
        col[1] = gf_mul(a, 0x09) ^ gf_mul(b, 0x0e) ^ gf_mul(c, 0x0b) ^
                 gf_mul(d, 0x0d);
        col[2] = gf_mul(a, 0x0d) ^ gf_mul(b, 0x09) ^ gf_mul(c, 0x0e) ^
                 gf_mul(d, 0x0b);
        col[3] = gf_mul(a, 0x0b) ^ gf_mul(b, 0x0d) ^ gf_mul(c, 0x09) ^
                 gf_mul(d, 0x0e);
    }
}

/**
 * \brief           Encrypt a single AES block
 * \param[in,out]   block: 16-byte block to encrypt
 */
static void aes_encrypt_block(uint8_t* block) {
    uint8_t nr = g_crypto_ctx.num_rounds;
    const uint8_t* w = g_crypto_ctx.expanded_key;

    /* Initial round key addition */
    aes_add_round_key(block, w);

    /* Main rounds */
    for (uint8_t round = 1; round < nr; ++round) {
        aes_sub_bytes(block);
        aes_shift_rows(block);
        aes_mix_columns(block);
        aes_add_round_key(block, &w[round * 16]);
    }

    /* Final round (no MixColumns) */
    aes_sub_bytes(block);
    aes_shift_rows(block);
    aes_add_round_key(block, &w[nr * 16]);
}

/**
 * \brief           Decrypt a single AES block
 * \param[in,out]   block: 16-byte block to decrypt
 */
static void aes_decrypt_block(uint8_t* block) {
    uint8_t nr = g_crypto_ctx.num_rounds;
    const uint8_t* w = g_crypto_ctx.expanded_key;

    /* Initial round key addition */
    aes_add_round_key(block, &w[nr * 16]);

    /* Main rounds */
    for (uint8_t round = nr - 1; round > 0; --round) {
        aes_inv_shift_rows(block);
        aes_inv_sub_bytes(block);
        aes_add_round_key(block, &w[round * 16]);
        aes_inv_mix_columns(block);
    }

    /* Final round (no InvMixColumns) */
    aes_inv_shift_rows(block);
    aes_inv_sub_bytes(block);
    aes_add_round_key(block, w);
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

config_status_t config_set_encryption_key(const uint8_t* key, size_t key_len,
                                          config_crypto_algo_t algo) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Validate key length based on algorithm */
    if (algo == CONFIG_CRYPTO_AES128) {
        if (key_len != CONFIG_CRYPTO_AES128_KEY_SIZE) {
            return CONFIG_ERROR_INVALID_PARAM;
        }
    } else if (algo == CONFIG_CRYPTO_AES256) {
        if (key_len != CONFIG_CRYPTO_AES256_KEY_SIZE) {
            return CONFIG_ERROR_INVALID_PARAM;
        }
    } else {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Store key and expand */
    memcpy(g_crypto_ctx.key, key, key_len);
    g_crypto_ctx.key_len = key_len;
    g_crypto_ctx.algo = algo;

    /* Expand key schedule */
    aes_key_expansion(key, key_len, g_crypto_ctx.expanded_key,
                      &g_crypto_ctx.num_rounds);

    g_crypto_ctx.enabled = true;

    /* Seed PRNG with key material for IV generation */
    g_prng_state = 0;
    for (size_t i = 0; i < key_len; ++i) {
        g_prng_state ^= ((uint32_t)key[i]) << ((i % 4) * 8);
    }

    return CONFIG_OK;
}

config_status_t config_clear_encryption_key(void) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    /* Securely clear key material */
    memset(g_crypto_ctx.key, 0, sizeof(g_crypto_ctx.key));
    memset(g_crypto_ctx.expanded_key, 0, sizeof(g_crypto_ctx.expanded_key));
    g_crypto_ctx.key_len = 0;
    g_crypto_ctx.enabled = false;
    g_crypto_ctx.num_rounds = 0;

    return CONFIG_OK;
}

bool config_crypto_is_enabled(void) {
    return g_crypto_ctx.enabled;
}

config_crypto_algo_t config_crypto_get_algo(void) {
    return g_crypto_ctx.algo;
}

void config_crypto_clear(void) {
    memset(&g_crypto_ctx, 0, sizeof(g_crypto_ctx));
}

size_t config_crypto_get_encrypted_size(size_t plaintext_len) {
    /* IV + padded ciphertext */
    /* PKCS7 padding: always add at least 1 byte, up to block size */
    size_t padded_len = ((plaintext_len / CONFIG_CRYPTO_AES_BLOCK_SIZE) + 1) *
                        CONFIG_CRYPTO_AES_BLOCK_SIZE;
    return CONFIG_CRYPTO_IV_SIZE + padded_len;
}

size_t config_crypto_get_decrypted_size(size_t ciphertext_len) {
    if (ciphertext_len <= CONFIG_CRYPTO_IV_SIZE) {
        return 0;
    }
    /* Maximum possible plaintext size (actual may be less due to padding) */
    return ciphertext_len - CONFIG_CRYPTO_IV_SIZE;
}

config_status_t config_crypto_encrypt(const uint8_t* plaintext,
                                      size_t plaintext_len, uint8_t* ciphertext,
                                      size_t* ciphertext_len) {
    if (!g_crypto_ctx.enabled) {
        return CONFIG_ERROR_NO_ENCRYPTION_KEY;
    }

    if (plaintext == NULL || ciphertext == NULL || ciphertext_len == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t required_size = config_crypto_get_encrypted_size(plaintext_len);
    if (*ciphertext_len < required_size) {
        *ciphertext_len = required_size;
        return CONFIG_ERROR_BUFFER_TOO_SMALL;
    }

    /* Generate random IV */
    uint8_t iv[CONFIG_CRYPTO_IV_SIZE];
    generate_iv(iv);

    /* Copy IV to output */
    memcpy(ciphertext, iv, CONFIG_CRYPTO_IV_SIZE);

    /* Calculate padding */
    size_t padding = CONFIG_CRYPTO_AES_BLOCK_SIZE -
                     (plaintext_len % CONFIG_CRYPTO_AES_BLOCK_SIZE);
    size_t padded_len = plaintext_len + padding;

    /* Process blocks with CBC mode */
    uint8_t block[CONFIG_CRYPTO_AES_BLOCK_SIZE];
    uint8_t prev_block[CONFIG_CRYPTO_AES_BLOCK_SIZE];
    memcpy(prev_block, iv, CONFIG_CRYPTO_AES_BLOCK_SIZE);

    size_t offset = 0;
    uint8_t* out = ciphertext + CONFIG_CRYPTO_IV_SIZE;

    while (offset < padded_len) {
        /* Prepare block with plaintext or padding */
        for (size_t i = 0; i < CONFIG_CRYPTO_AES_BLOCK_SIZE; ++i) {
            if (offset + i < plaintext_len) {
                block[i] = plaintext[offset + i];
            } else {
                /* PKCS7 padding */
                block[i] = (uint8_t)padding;
            }
        }

        /* XOR with previous ciphertext block (CBC) */
        for (size_t i = 0; i < CONFIG_CRYPTO_AES_BLOCK_SIZE; ++i) {
            block[i] ^= prev_block[i];
        }

        /* Encrypt block */
        aes_encrypt_block(block);

        /* Copy to output and save for next iteration */
        memcpy(out, block, CONFIG_CRYPTO_AES_BLOCK_SIZE);
        memcpy(prev_block, block, CONFIG_CRYPTO_AES_BLOCK_SIZE);

        out += CONFIG_CRYPTO_AES_BLOCK_SIZE;
        offset += CONFIG_CRYPTO_AES_BLOCK_SIZE;
    }

    *ciphertext_len = required_size;
    return CONFIG_OK;
}

config_status_t config_crypto_decrypt(const uint8_t* ciphertext,
                                      size_t ciphertext_len, uint8_t* plaintext,
                                      size_t* plaintext_len) {
    if (!g_crypto_ctx.enabled) {
        return CONFIG_ERROR_NO_ENCRYPTION_KEY;
    }

    if (ciphertext == NULL || plaintext == NULL || plaintext_len == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Minimum size: IV + one block */
    if (ciphertext_len < CONFIG_CRYPTO_IV_SIZE + CONFIG_CRYPTO_AES_BLOCK_SIZE) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    /* Check alignment */
    size_t data_len = ciphertext_len - CONFIG_CRYPTO_IV_SIZE;
    if (data_len % CONFIG_CRYPTO_AES_BLOCK_SIZE != 0) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    size_t max_plaintext = config_crypto_get_decrypted_size(ciphertext_len);
    if (*plaintext_len < max_plaintext) {
        *plaintext_len = max_plaintext;
        return CONFIG_ERROR_BUFFER_TOO_SMALL;
    }

    /* Extract IV */
    const uint8_t* iv = ciphertext;
    const uint8_t* in = ciphertext + CONFIG_CRYPTO_IV_SIZE;

    /* Process blocks with CBC mode */
    uint8_t block[CONFIG_CRYPTO_AES_BLOCK_SIZE];
    uint8_t prev_block[CONFIG_CRYPTO_AES_BLOCK_SIZE];
    uint8_t curr_cipher[CONFIG_CRYPTO_AES_BLOCK_SIZE];
    memcpy(prev_block, iv, CONFIG_CRYPTO_AES_BLOCK_SIZE);

    size_t offset = 0;
    size_t out_offset = 0;

    while (offset < data_len) {
        /* Save current ciphertext block for next iteration */
        memcpy(curr_cipher, in + offset, CONFIG_CRYPTO_AES_BLOCK_SIZE);

        /* Copy and decrypt block */
        memcpy(block, curr_cipher, CONFIG_CRYPTO_AES_BLOCK_SIZE);
        aes_decrypt_block(block);

        /* XOR with previous ciphertext block (CBC) */
        for (size_t i = 0; i < CONFIG_CRYPTO_AES_BLOCK_SIZE; ++i) {
            block[i] ^= prev_block[i];
        }

        /* Copy to output */
        memcpy(plaintext + out_offset, block, CONFIG_CRYPTO_AES_BLOCK_SIZE);

        /* Save for next iteration */
        memcpy(prev_block, curr_cipher, CONFIG_CRYPTO_AES_BLOCK_SIZE);

        offset += CONFIG_CRYPTO_AES_BLOCK_SIZE;
        out_offset += CONFIG_CRYPTO_AES_BLOCK_SIZE;
    }

    /* Remove PKCS7 padding */
    uint8_t padding = plaintext[out_offset - 1];
    if (padding == 0 || padding > CONFIG_CRYPTO_AES_BLOCK_SIZE) {
        return CONFIG_ERROR_CRYPTO_FAILED; /* Invalid padding */
    }

    /* Verify padding bytes */
    for (size_t i = 0; i < padding; ++i) {
        if (plaintext[out_offset - 1 - i] != padding) {
            return CONFIG_ERROR_CRYPTO_FAILED; /* Invalid padding */
        }
    }

    *plaintext_len = out_offset - padding;
    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Encrypted Storage API Implementation                                      */
/*---------------------------------------------------------------------------*/

config_status_t config_set_str_encrypted(const char* key, const char* value) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!g_crypto_ctx.enabled) {
        return CONFIG_ERROR_NO_ENCRYPTION_KEY;
    }

    /* Calculate sizes */
    size_t plaintext_len = strlen(value) + 1; /* Include null terminator */
    size_t ciphertext_len = config_crypto_get_encrypted_size(plaintext_len);

    /* Check if encrypted size fits in max value size */
    if (ciphertext_len > CONFIG_MAX_MAX_VALUE_SIZE) {
        return CONFIG_ERROR_VALUE_TOO_LARGE;
    }

    /* Encrypt the value */
    uint8_t ciphertext[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t actual_len = sizeof(ciphertext);

    config_status_t status = config_crypto_encrypt(
        (const uint8_t*)value, plaintext_len, ciphertext, &actual_len);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Store encrypted value with encrypted flag */
    return config_store_set(key, CONFIG_TYPE_STRING, ciphertext, actual_len,
                            CONFIG_FLAG_ENCRYPTED, CONFIG_DEFAULT_NAMESPACE_ID);
}

config_status_t config_set_blob_encrypted(const char* key, const void* data,
                                          size_t size) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || data == NULL || size == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!g_crypto_ctx.enabled) {
        return CONFIG_ERROR_NO_ENCRYPTION_KEY;
    }

    /* Calculate encrypted size */
    size_t ciphertext_len = config_crypto_get_encrypted_size(size);

    /* Check if encrypted size fits in max value size */
    if (ciphertext_len > CONFIG_MAX_MAX_VALUE_SIZE) {
        return CONFIG_ERROR_VALUE_TOO_LARGE;
    }

    /* Encrypt the data */
    uint8_t ciphertext[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t actual_len = sizeof(ciphertext);

    config_status_t status = config_crypto_encrypt((const uint8_t*)data, size,
                                                   ciphertext, &actual_len);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Store encrypted value with encrypted flag */
    return config_store_set(key, CONFIG_TYPE_BLOB, ciphertext, actual_len,
                            CONFIG_FLAG_ENCRYPTED, CONFIG_DEFAULT_NAMESPACE_ID);
}

config_status_t config_is_encrypted(const char* key, bool* encrypted) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || encrypted == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    uint8_t flags = 0;
    config_status_t status =
        config_store_get_flags(key, CONFIG_DEFAULT_NAMESPACE_ID, &flags);
    if (status != CONFIG_OK) {
        return status;
    }

    *encrypted = (flags & CONFIG_FLAG_ENCRYPTED) != 0;
    return CONFIG_OK;
}

config_status_t config_rotate_encryption_key(const uint8_t* new_key,
                                             size_t key_len,
                                             config_crypto_algo_t algo) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (new_key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!g_crypto_ctx.enabled) {
        return CONFIG_ERROR_NO_ENCRYPTION_KEY;
    }

    /* Validate new key length */
    if (algo == CONFIG_CRYPTO_AES128) {
        if (key_len != CONFIG_CRYPTO_AES128_KEY_SIZE) {
            return CONFIG_ERROR_INVALID_PARAM;
        }
    } else if (algo == CONFIG_CRYPTO_AES256) {
        if (key_len != CONFIG_CRYPTO_AES256_KEY_SIZE) {
            return CONFIG_ERROR_INVALID_PARAM;
        }
    } else {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Save old key context */
    config_crypto_ctx_t old_ctx;
    memcpy(&old_ctx, &g_crypto_ctx, sizeof(old_ctx));

    /* Set up new key */
    config_status_t status = config_set_encryption_key(new_key, key_len, algo);
    if (status != CONFIG_OK) {
        /* Restore old context on failure */
        memcpy(&g_crypto_ctx, &old_ctx, sizeof(g_crypto_ctx));
        return status;
    }

    /* Iterate through all entries and re-encrypt encrypted ones */
    /* This is a simplified implementation - in production, you'd want
       to handle failures more gracefully */

    /* For now, we just update the key - actual re-encryption would require
       iterating through all encrypted entries, decrypting with old key,
       and re-encrypting with new key. This is left as a TODO for a more
       complete implementation. */

    /* Clear old key material */
    memset(&old_ctx, 0, sizeof(old_ctx));

    return CONFIG_OK;
}
