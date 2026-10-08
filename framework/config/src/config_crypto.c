/** Authenticated records and recoverable key rotation over maintained providers. */
#include "config_crypto.h"
#include "config/config.h"
#include "config_backend_internal.h"
#include "config_store.h"
#include "security/crypto.h"
#include <stdlib.h>
#include <string.h>

#define KEYRING_CAPACITY 4u
#define NONCE_INVOCATION_LIMIT (1u << 20)

typedef struct {
    bool used;
    config_crypto_algo_t algorithm;
    uint8_t key[CONFIG_CRYPTO_MAX_KEY_SIZE];
    size_t key_size;
    uint8_t id[CONFIG_CRYPTO_KEY_ID_SIZE];
    uint32_t invocations;
    uint8_t previous_nonce[CONFIG_CRYPTO_NONCE_SIZE];
    bool has_previous_nonce;
} crypto_key_t;
static crypto_key_t g_keys[KEYRING_CAPACITY];
static crypto_key_t* g_active;

static config_status_t map_status(nx_crypto_status_t status) {
    if (status == NX_CRYPTO_OK) { return CONFIG_OK; }
    if (status == NX_CRYPTO_UNSUPPORTED) { return CONFIG_ERROR_UNSUPPORTED; }
    if (status == NX_CRYPTO_NO_MEMORY) { return CONFIG_ERROR_NO_MEMORY; }
    if (status == NX_CRYPTO_INVALID_PARAM) { return CONFIG_ERROR_INVALID_PARAM; }
    return CONFIG_ERROR_CRYPTO_FAILED;
}

static bool valid_key(const uint8_t* key, size_t length, config_crypto_algo_t algo) {
    return key && ((algo == CONFIG_CRYPTO_AES128_GCM && length == 16u) ||
                   (algo == CONFIG_CRYPTO_AES256_GCM && length == 32u));
}

static void put32(uint8_t* output, uint32_t value) {
    for (size_t i = 0; i < 4u; ++i) { output[i] = (uint8_t)(value >> (8u * i)); }
}
static uint32_t get32(const uint8_t* input) {
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8) |
           ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
}

static size_t bounded_length(const char* string, size_t limit) {
    size_t size = 0;
    while (size < limit && string[size]) { ++size; }
    return size;
}

config_status_t config_get_encryption_key_id(const uint8_t* key, size_t length,
    config_crypto_algo_t algo, uint8_t id[CONFIG_CRYPTO_KEY_ID_SIZE]) {
    if (!valid_key(key, length, algo) || !id) { return CONFIG_ERROR_INVALID_PARAM; }
    /* Domain separation, algorithm and key form a stable opaque lookup ID.
     * This ID is public metadata, never a credential or proof of possession. */
    uint8_t material[16u + CONFIG_CRYPTO_MAX_KEY_SIZE] = {0};
    static const uint8_t domain[15] = "NexusConfigKey";
    memcpy(material, domain, sizeof(domain));
    material[15] = (uint8_t)algo;
    memcpy(material + 16, key, length);
    uint8_t digest[NX_CRYPTO_SHA256_SIZE];
    nx_crypto_status_t status = nx_crypto_sha256(material, 16u + length, digest);
    if (status == NX_CRYPTO_OK) { memcpy(id, digest, CONFIG_CRYPTO_KEY_ID_SIZE); }
    nx_crypto_secure_zero(material, sizeof(material));
    nx_crypto_secure_zero(digest, sizeof(digest));
    return map_status(status);
}

static crypto_key_t* find_key(const uint8_t id[CONFIG_CRYPTO_KEY_ID_SIZE]) {
    for (size_t i = 0; i < KEYRING_CAPACITY; ++i) {
        if (g_keys[i].used && memcmp(g_keys[i].id, id, CONFIG_CRYPTO_KEY_ID_SIZE) == 0) {
            return &g_keys[i];
        }
    }
    return NULL;
}

config_status_t config_register_encryption_key(const uint8_t* key, size_t length,
    config_crypto_algo_t algo, bool make_active) {
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    uint8_t id[CONFIG_CRYPTO_KEY_ID_SIZE];
    config_status_t status = config_get_encryption_key_id(key, length, algo, id);
    if (status != CONFIG_OK) { return status; }
    crypto_key_t* entry = find_key(id);
    if (entry) {
        if (entry->algorithm != algo || entry->key_size != length ||
            memcmp(entry->key, key, length) != 0) { return CONFIG_ERROR_CRYPTO_FAILED; }
    } else {
        for (size_t i = 0; i < KEYRING_CAPACITY; ++i) {
            if (!g_keys[i].used) { entry = &g_keys[i]; break; }
        }
        if (!entry) { return CONFIG_ERROR_NO_SPACE; }
        memset(entry, 0, sizeof(*entry));
        entry->algorithm = algo;
        entry->key_size = length;
        memcpy(entry->key, key, length);
        memcpy(entry->id, id, sizeof(id));
        entry->used = true;
    }
    if (make_active) { g_active = entry; }
    return CONFIG_OK;
}

config_status_t config_set_encryption_key(const uint8_t* key, size_t length,
    config_crypto_algo_t algo) {
    /* Replace the complete in-memory keyring deliberately. Use registration to
     * reload multiple generations, and rotation to migrate existing records. */
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    uint8_t id[CONFIG_CRYPTO_KEY_ID_SIZE];
    config_status_t status = config_get_encryption_key_id(key, length, algo, id);
    if (status != CONFIG_OK) { return status; }
    crypto_key_t* same = find_key(id);
    if (same && same->algorithm == algo && same->key_size == length &&
        memcmp(same->key, key, length) == 0) { g_active = same; return CONFIG_OK; }
    config_crypto_clear();
    return config_register_encryption_key(key, length, algo, true);
}

config_status_t config_clear_encryption_key(void) {
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    config_crypto_clear();
    return CONFIG_OK;
}
bool config_crypto_is_enabled(void) { return g_active != NULL; }
config_crypto_algo_t config_crypto_get_algo(void) {
    return g_active ? g_active->algorithm : (config_crypto_algo_t)0;
}
void config_crypto_clear(void) {
    nx_crypto_secure_zero(g_keys, sizeof(g_keys));
    g_active = NULL;
}
size_t config_crypto_get_encrypted_size(size_t length) {
    return length > SIZE_MAX - CONFIG_CRYPTO_RECORD_OVERHEAD ? 0 :
           length + CONFIG_CRYPTO_RECORD_OVERHEAD;
}
size_t config_crypto_get_decrypted_size(size_t length) {
    return length >= CONFIG_CRYPTO_RECORD_OVERHEAD ?
           length - CONFIG_CRYPTO_RECORD_OVERHEAD : 0;
}

static config_status_t make_aad(const uint8_t* header, const char* key,
    uint8_t ns, config_type_t type, uint8_t* aad, size_t* length) {
    size_t key_size = key ? bounded_length(key, CONFIG_MAX_MAX_KEY_LEN) : 0;
    if (key_size >= CONFIG_MAX_MAX_KEY_LEN || (unsigned)type > CONFIG_TYPE_BLOB) {
        return CONFIG_ERROR_INVALID_PARAM;
    }
    memcpy(aad, header, CONFIG_CRYPTO_HEADER_SIZE);
    aad[CONFIG_CRYPTO_HEADER_SIZE] = ns;
    aad[CONFIG_CRYPTO_HEADER_SIZE + 1] = (uint8_t)type;
    aad[CONFIG_CRYPTO_HEADER_SIZE + 2] = (uint8_t)key_size;
    if (key_size) { memcpy(aad + CONFIG_CRYPTO_HEADER_SIZE + 3, key, key_size); }
    *length = CONFIG_CRYPTO_HEADER_SIZE + 3u + key_size;
    return CONFIG_OK;
}

static config_status_t encrypt_record(crypto_key_t* secret,
    const uint8_t* plaintext, size_t length, uint8_t* record, size_t* record_size,
    const char* key, uint8_t ns, config_type_t type) {
    if (!secret) { return CONFIG_ERROR_NO_ENCRYPTION_KEY; }
    if ((!plaintext && length) || !record || !record_size ||
        length > CONFIG_MAX_MAX_VALUE_SIZE || length > UINT32_MAX) {
        return CONFIG_ERROR_INVALID_PARAM;
    }
    size_t required = config_crypto_get_encrypted_size(length);
    if (*record_size < required) { *record_size = required; return CONFIG_ERROR_BUFFER_TOO_SMALL; }
    if (secret->invocations >= NONCE_INVOCATION_LIMIT) { return CONFIG_ERROR_CRYPTO_FAILED; }
    uint8_t header[CONFIG_CRYPTO_HEADER_SIZE] = {'N', 'X', 'C', 'F', 1,
        (uint8_t)secret->algorithm, CONFIG_CRYPTO_NONCE_SIZE, CONFIG_CRYPTO_TAG_SIZE};
    memcpy(header + 8, secret->id, CONFIG_CRYPTO_KEY_ID_SIZE);
    put32(header + 24, (uint32_t)length);
    uint8_t aad[CONFIG_CRYPTO_HEADER_SIZE + 3u + CONFIG_MAX_MAX_KEY_LEN];
    size_t aad_size;
    config_status_t status = make_aad(header, key, ns, type, aad, &aad_size);
    if (status != CONFIG_OK) { return status; }
    uint8_t nonce[CONFIG_CRYPTO_NONCE_SIZE];
    status = map_status(nx_crypto_random(nonce, sizeof(nonce)));
    if (status != CONFIG_OK) { return status; }
    /* Catch a broken provider repeating adjacent outputs without hiding failure.
     * General random-nonce collision bounds depend on the maintained CSPRNG. */
    if (secret->has_previous_nonce && memcmp(nonce, secret->previous_nonce, sizeof(nonce)) == 0) {
        nx_crypto_secure_zero(nonce, sizeof(nonce));
        return CONFIG_ERROR_CRYPTO_FAILED;
    }
    memcpy(secret->previous_nonce, nonce, sizeof(nonce));
    secret->has_previous_nonce = true;
    ++secret->invocations;
    memcpy(record, header, sizeof(header));
    memcpy(record + CONFIG_CRYPTO_HEADER_SIZE, nonce, sizeof(nonce));
    uint8_t* output = record + CONFIG_CRYPTO_HEADER_SIZE + CONFIG_CRYPTO_NONCE_SIZE;
    status = map_status(nx_crypto_seal((nx_crypto_algorithm_t)secret->algorithm,
        secret->key, secret->key_size, nonce, aad, aad_size, plaintext, length,
        output, output + length));
    nx_crypto_secure_zero(nonce, sizeof(nonce));
    if (status != CONFIG_OK) { nx_crypto_secure_zero(record, required); return status; }
    *record_size = required;
    return CONFIG_OK;
}

config_status_t config_crypto_encrypt(const uint8_t* plaintext, size_t length,
    uint8_t* record, size_t* record_size) {
    return encrypt_record(g_active, plaintext, length, record, record_size,
        NULL, CONFIG_DEFAULT_NAMESPACE_ID, CONFIG_TYPE_BLOB);
}

config_status_t config_crypto_decrypt_record(const uint8_t* record, size_t length,
    uint8_t* plaintext, size_t* plaintext_size, const char* key,
    uint8_t ns, config_type_t type) {
    if (!g_active) { return CONFIG_ERROR_NO_ENCRYPTION_KEY; }
    if (!record || !plaintext || !plaintext_size) { return CONFIG_ERROR_INVALID_PARAM; }
    if (length < CONFIG_CRYPTO_RECORD_OVERHEAD || memcmp(record, "NXCF", 4) ||
        record[4] != 1 || record[6] != CONFIG_CRYPTO_NONCE_SIZE ||
        record[7] != CONFIG_CRYPTO_TAG_SIZE ||
        (record[5] != CONFIG_CRYPTO_AES128_GCM && record[5] != CONFIG_CRYPTO_AES256_GCM)) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }
    size_t size = get32(record + 24);
    if (size > CONFIG_MAX_MAX_VALUE_SIZE || size != length - CONFIG_CRYPTO_RECORD_OVERHEAD) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }
    crypto_key_t* secret = find_key(record + 8);
    if (!secret || record[5] != (uint8_t)secret->algorithm) { return CONFIG_ERROR_CRYPTO_FAILED; }
    if (*plaintext_size < size) { *plaintext_size = size; return CONFIG_ERROR_BUFFER_TOO_SMALL; }
    uint8_t aad[CONFIG_CRYPTO_HEADER_SIZE + 3u + CONFIG_MAX_MAX_KEY_LEN];
    size_t aad_size;
    config_status_t status = make_aad(record, key, ns, type, aad, &aad_size);
    if (status != CONFIG_OK) { return status; }
    const uint8_t* nonce = record + CONFIG_CRYPTO_HEADER_SIZE;
    const uint8_t* input = nonce + CONFIG_CRYPTO_NONCE_SIZE;
    status = map_status(nx_crypto_open((nx_crypto_algorithm_t)secret->algorithm,
        secret->key, secret->key_size, nonce, aad, aad_size, input, size, input + size, plaintext));
    if (status == CONFIG_OK) { *plaintext_size = size; }
    return status;
}
config_status_t config_crypto_decrypt(const uint8_t* record, size_t length,
    uint8_t* plaintext, size_t* plaintext_size) {
    return config_crypto_decrypt_record(record, length, plaintext, plaintext_size,
        NULL, CONFIG_DEFAULT_NAMESPACE_ID, CONFIG_TYPE_BLOB);
}

config_status_t config_crypto_get_plaintext_size(const char* key, uint8_t ns,
    config_type_t type, size_t* size) {
    if (!key || !size) { return CONFIG_ERROR_INVALID_PARAM; }
    uint8_t record[CONFIG_MAX_MAX_VALUE_SIZE], plaintext[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t record_size = sizeof(record), plaintext_size = sizeof(plaintext);
    config_status_t status = config_store_get(key, NULL, record, &record_size, NULL, ns);
    if (status == CONFIG_OK) {
        status = config_crypto_decrypt_record(record, record_size, plaintext,
            &plaintext_size, key, ns, type);
    }
    if (status == CONFIG_OK && type == CONFIG_TYPE_STRING &&
        (!plaintext_size || plaintext[plaintext_size - 1] != '\0')) {
        status = CONFIG_ERROR_INVALID_FORMAT;
    }
    if (status == CONFIG_OK) { *size = plaintext_size; }
    nx_crypto_secure_zero(record, sizeof(record));
    nx_crypto_secure_zero(plaintext, sizeof(plaintext));
    return status;
}

static config_status_t store_encrypted(const char* key, const uint8_t* input,
    size_t length, config_type_t type) {
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    if (!key || !input) { return CONFIG_ERROR_INVALID_PARAM; }
    if (!g_active) { return CONFIG_ERROR_NO_ENCRYPTION_KEY; }
    size_t key_size = bounded_length(key, CONFIG_MAX_MAX_KEY_LEN);
    if (!key_size || key_size >= CONFIG_MAX_MAX_KEY_LEN) { return CONFIG_ERROR_KEY_TOO_LONG; }
    uint8_t flags = CONFIG_FLAG_ENCRYPTED, previous_flags = 0;
    config_status_t previous = config_store_get_flags(key, CONFIG_DEFAULT_NAMESPACE_ID, &previous_flags);
    if (previous == CONFIG_OK) {
        if (previous_flags & CONFIG_FLAG_READONLY) { return CONFIG_ERROR_READ_ONLY; }
        flags |= previous_flags & CONFIG_FLAG_PERSISTENT;
    } else if (previous != CONFIG_ERROR_NOT_FOUND) { return previous; }
    size_t required = config_crypto_get_encrypted_size(length);
    if (!required || required > CONFIG_MAX_MAX_VALUE_SIZE) { return CONFIG_ERROR_VALUE_TOO_LARGE; }
    uint8_t record[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t record_size = sizeof(record);
    config_status_t status = encrypt_record(g_active, input, length, record, &record_size,
        key, CONFIG_DEFAULT_NAMESPACE_ID, type);
    if (status == CONFIG_OK) {
        status = config_store_set(key, type, record, record_size,
            flags, CONFIG_DEFAULT_NAMESPACE_ID);
    }
    nx_crypto_secure_zero(record, sizeof(record));
    return status;
}
config_status_t config_set_str_encrypted(const char* key, const char* value) {
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    if (!value) { return CONFIG_ERROR_INVALID_PARAM; }
    size_t size = bounded_length(value, CONFIG_MAX_MAX_VALUE_SIZE);
    if (size == CONFIG_MAX_MAX_VALUE_SIZE) { return CONFIG_ERROR_VALUE_TOO_LARGE; }
    return store_encrypted(key, (const uint8_t*)value, size + 1u, CONFIG_TYPE_STRING);
}
config_status_t config_set_blob_encrypted(const char* key, const void* data, size_t size) {
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    if (size == 0) { return CONFIG_ERROR_INVALID_PARAM; }
    return store_encrypted(key, (const uint8_t*)data, size, CONFIG_TYPE_BLOB);
}
config_status_t config_is_encrypted(const char* key, bool* encrypted) {
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    if (!key || !encrypted) { return CONFIG_ERROR_INVALID_PARAM; }
    uint8_t flags;
    config_status_t status = config_store_get_flags(key, CONFIG_DEFAULT_NAMESPACE_ID, &flags);
    if (status == CONFIG_OK) { *encrypted = (flags & CONFIG_FLAG_ENCRYPTED) != 0; }
    return status;
}

typedef struct { const uint8_t* id; bool referenced; } reference_ctx_t;
static bool reference_key(const config_store_entry_info_t* info, void* context) {
    reference_ctx_t* state = (reference_ctx_t*)context;
    if (!(info->flags & CONFIG_FLAG_ENCRYPTED)) { return true; }
    uint8_t value[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t size = sizeof(value);
    if (config_store_get(info->key, NULL, value, &size, NULL, info->namespace_id) != CONFIG_OK ||
        size < CONFIG_CRYPTO_RECORD_OVERHEAD || memcmp(value, "NXCF", 4) != 0) {
        state->referenced = true; return false; /* Unknown formats prevent unsafe retirement. */
    }
    state->referenced = memcmp(value + 8, state->id, CONFIG_CRYPTO_KEY_ID_SIZE) == 0;
    return !state->referenced;
}
config_status_t config_retire_encryption_key(const uint8_t id[CONFIG_CRYPTO_KEY_ID_SIZE]) {
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    if (!id) { return CONFIG_ERROR_INVALID_PARAM; }
    crypto_key_t* entry = find_key(id);
    if (!entry) { return CONFIG_ERROR_NOT_FOUND; }
    if (entry == g_active) { return CONFIG_ERROR_INVALID_PARAM; }
    reference_ctx_t state = {id, false};
    config_status_t status = config_store_iterate(reference_key, &state);
    if (status != CONFIG_OK) { return status; }
    if (state.referenced) { return CONFIG_ERROR_ALREADY_EXISTS; }
    nx_crypto_secure_zero(entry, sizeof(*entry));
    return CONFIG_OK;
}

typedef struct {
    config_store_entry_info_t info;
    uint8_t* original;
    uint8_t* replacement;
} rotation_item_t;
typedef struct {
    rotation_item_t* items;
    size_t count;
    size_t used;
    crypto_key_t* new_key;
    config_status_t status;
} rotation_ctx_t;
static bool count_encrypted(const config_store_entry_info_t* info, void* context) {
    size_t* count = (size_t*)context;
    if (info->flags & CONFIG_FLAG_ENCRYPTED) { ++*count; }
    return true;
}
static bool stage_rotation(const config_store_entry_info_t* info, void* context) {
    rotation_ctx_t* state = (rotation_ctx_t*)context;
    if (!(info->flags & CONFIG_FLAG_ENCRYPTED)) { return true; }
    if (state->used >= state->count) { state->status = CONFIG_ERROR; return false; }
    rotation_item_t* item = &state->items[state->used++];
    item->info = *info;
    item->original = (uint8_t*)malloc(info->value_size);
    item->replacement = (uint8_t*)malloc(info->value_size);
    if (!item->original || !item->replacement) { state->status = CONFIG_ERROR_NO_MEMORY; return false; }
    size_t length = info->value_size;
    state->status = config_store_get(info->key, NULL, item->original, &length, NULL, info->namespace_id);
    uint8_t plaintext[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t plaintext_size = sizeof(plaintext);
    if (state->status == CONFIG_OK) {
        state->status = config_crypto_decrypt_record(item->original, length, plaintext,
            &plaintext_size, info->key, info->namespace_id, info->type);
    }
    if (state->status == CONFIG_OK) {
        size_t replacement_size = info->value_size;
        state->status = encrypt_record(state->new_key, plaintext, plaintext_size,
            item->replacement, &replacement_size, info->key, info->namespace_id, info->type);
        if (state->status == CONFIG_OK && replacement_size != info->value_size) { state->status = CONFIG_ERROR; }
    }
    nx_crypto_secure_zero(plaintext, sizeof(plaintext));
    return state->status == CONFIG_OK;
}

config_status_t config_rotate_encryption_key(const uint8_t* key, size_t length,
    config_crypto_algo_t algo) {
    if (!config_is_initialized()) { return CONFIG_ERROR_NOT_INIT; }
    if (!valid_key(key, length, algo)) { return CONFIG_ERROR_INVALID_PARAM; }
    if (!g_active) { return CONFIG_ERROR_NO_ENCRYPTION_KEY; }
    uint8_t id[CONFIG_CRYPTO_KEY_ID_SIZE];
    config_status_t status = config_get_encryption_key_id(key, length, algo, id);
    if (status != CONFIG_OK) { return status; }
    /* Register without switching; keep both generations even on ambiguous I/O.
     * Their durable protection/reload is the product key-vault responsibility. */
    status = config_register_encryption_key(key, length, algo, false);
    if (status != CONFIG_OK) { return status; }
    crypto_key_t* previous = g_active;
    crypto_key_t* replacement = find_key(id);
    size_t count = 0;
    status = config_store_iterate(count_encrypted, &count);
    if (status != CONFIG_OK) { return status; }
    if (count > CONFIG_MAX_MAX_KEYS) { return CONFIG_ERROR_NO_SPACE; }
    if (!count) { g_active = replacement; return CONFIG_OK; }
    rotation_item_t* items = (rotation_item_t*)calloc(count, sizeof(*items));
    config_store_replacement_t* batch = (config_store_replacement_t*)calloc(count, sizeof(*batch));
    if (!items || !batch) { free(items); free(batch); return CONFIG_ERROR_NO_MEMORY; }
    rotation_ctx_t stage = {items, count, 0, replacement, CONFIG_OK};
    status = config_store_iterate(stage_rotation, &stage);
    if (status == CONFIG_OK) { status = stage.status; }
    if (status == CONFIG_OK && stage.used != count) { status = CONFIG_ERROR; }
    if (status == CONFIG_OK) {
        for (size_t i = 0; i < count; ++i) {
            batch[i].info = items[i].info;
            batch[i].value = items[i].replacement;
            batch[i].size = items[i].info.value_size;
        }
        status = config_store_replace_encrypted(batch, count);
        if (status == CONFIG_OK) {
            g_active = replacement;
            config_backend_set_dirty(true);
            if (config_backend_is_set()) { status = config_commit(); }
            if (status != CONFIG_OK) {
                for (size_t i = 0; i < count; ++i) { batch[i].value = items[i].original; }
                config_status_t rollback = config_store_replace_encrypted(batch, count);
                g_active = previous;
                config_backend_set_dirty(true);
                if (rollback != CONFIG_OK) { status = rollback; }
            }
        }
    }
    for (size_t i = 0; i < stage.used; ++i) {
        if (items[i].original) { nx_crypto_secure_zero(items[i].original, items[i].info.value_size); }
        if (items[i].replacement) { nx_crypto_secure_zero(items[i].replacement, items[i].info.value_size); }
        free(items[i].original); free(items[i].replacement);
    }
    free(batch); free(items);
    return status;
}
