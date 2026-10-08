/* SEC002: real snapshot storage + authenticated metadata + update policy.
 * File Flash models durable bytes and every erase/program/sync interruption.
 * The counter and image slots are explicit Native models, not hardware claims.
 */
#define _POSIX_C_SOURCE 200809L
#include "nexus/file_flash.h"
#include "nexus/update.h"
#include "security/crypto.h"

#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); \
} } while (0)

#define ENVELOPE_AAD_SIZE 16u
#define ENVELOPE_NONCE_OFFSET ENVELOPE_AAD_SIZE
#define ENVELOPE_CIPHER_OFFSET (ENVELOPE_AAD_SIZE + NX_CRYPTO_NONCE_SIZE)
#define ENVELOPE_TAG_OFFSET (ENVELOPE_CIPHER_OFFSET + NX_UPDATE_RECORD_SIZE)
#define ENVELOPE_SIZE (ENVELOPE_TAG_OFFSET + NX_CRYPTO_TAG_SIZE)

typedef struct {
    char path[128];
    nx_file_flash_t flash;
    nx_storage_t storage;
    nx_update_port_t port;
    nx_update_t update;
    uint8_t metadata_key[32];
    uint8_t public_key[32];
    uint8_t images[2][128];
    EVP_PKEY* signer;
    nx_update_manifest_t baseline;
    nx_update_manifest_t candidate;
    uint64_t counter;
    unsigned counter_advances;
} fixture_t;

static void put32(uint8_t* p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

static nx_update_status_t metadata_load(void* opaque, uint8_t* out, size_t size) {
    fixture_t* f = opaque;
    uint8_t envelope[ENVELOPE_SIZE], expected[ENVELOPE_AAD_SIZE];
    size_t stored_size = sizeof(envelope);
    nx_storage_status_t status;
    CHECK(size == NX_UPDATE_RECORD_SIZE);
    status = nx_storage_load(&f->storage, envelope, &stored_size);
    if (status == NX_STORAGE_NOT_FOUND) return NX_UPDATE_ENOTFOUND;
    if (status == NX_STORAGE_CORRUPT || status == NX_STORAGE_NO_SPACE)
        return NX_UPDATE_ECORRUPT;
    if (status != NX_STORAGE_OK) return NX_UPDATE_ESTORAGE;
    if (stored_size != sizeof(envelope)) return NX_UPDATE_ECORRUPT;
    memcpy(expected, "NXUA", 4);
    put32(expected + 4, 1);
    put32(expected + 8, 7);
    put32(expected + 12, NX_UPDATE_RECORD_SIZE);
    if (memcmp(envelope, expected, sizeof(expected))) return NX_UPDATE_ECORRUPT;
    return nx_crypto_open(NX_CRYPTO_AES256_GCM, f->metadata_key,
        sizeof(f->metadata_key), envelope + ENVELOPE_NONCE_OFFSET,
        envelope, ENVELOPE_AAD_SIZE, envelope + ENVELOPE_CIPHER_OFFSET,
        NX_UPDATE_RECORD_SIZE, envelope + ENVELOPE_TAG_OFFSET, out) == NX_CRYPTO_OK
               ? NX_UPDATE_OK : NX_UPDATE_ECORRUPT;
}

static nx_update_status_t metadata_save(void* opaque, const uint8_t* in, size_t size) {
    fixture_t* f = opaque;
    uint8_t envelope[ENVELOPE_SIZE];
    CHECK(size == NX_UPDATE_RECORD_SIZE);
    memcpy(envelope, "NXUA", 4);
    put32(envelope + 4, 1);
    put32(envelope + 8, 7);
    put32(envelope + 12, NX_UPDATE_RECORD_SIZE);
    /* Real CSPRNG nonce for every attempt, including aborted writes. */
    if (nx_crypto_random(envelope + ENVELOPE_NONCE_OFFSET,
                         NX_CRYPTO_NONCE_SIZE) != NX_CRYPTO_OK)
        return NX_UPDATE_ESTORAGE;
    if (nx_crypto_seal(NX_CRYPTO_AES256_GCM, f->metadata_key,
        sizeof(f->metadata_key), envelope + ENVELOPE_NONCE_OFFSET,
        envelope, ENVELOPE_AAD_SIZE, in, size,
        envelope + ENVELOPE_CIPHER_OFFSET,
        envelope + ENVELOPE_TAG_OFFSET) != NX_CRYPTO_OK)
        return NX_UPDATE_ESTORAGE;
    return nx_storage_save(&f->storage, envelope, sizeof(envelope)) == NX_STORAGE_OK
               ? NX_UPDATE_OK : NX_UPDATE_ESTORAGE;
}

static nx_update_status_t image_digest(void* opaque, uint32_t slot,
                                       uint32_t size, uint8_t out[32]) {
    fixture_t* f = opaque;
    if (slot >= 2 || size > sizeof(f->images[slot])) return NX_UPDATE_EDIGEST;
    return nx_crypto_sha256(f->images[slot], size, out) == NX_CRYPTO_OK
               ? NX_UPDATE_OK : NX_UPDATE_EDIGEST;
}

static nx_update_status_t signature_verify(void* opaque, uint32_t key_id,
                                           const uint8_t* in, size_t size,
                                           const uint8_t signature[64]) {
    fixture_t* f = opaque;
    if (key_id != 7) return NX_UPDATE_EAUTH;
    return nx_crypto_verify_ed25519(f->public_key, in, size, signature) == NX_CRYPTO_OK
               ? NX_UPDATE_OK : NX_UPDATE_EAUTH;
}

static nx_update_status_t counter_read(void* opaque, uint64_t* value) {
    fixture_t* f = opaque;
    *value = f->counter;
    return NX_UPDATE_OK;
}

static nx_update_status_t counter_advance(void* opaque, uint64_t expected,
                                         uint64_t target) {
    fixture_t* f = opaque;
    CHECK(expected == f->counter && target > expected);
    f->counter = target;
    ++f->counter_advances;
    return NX_UPDATE_OK;
}

static void sign_manifest(fixture_t* f, nx_update_manifest_t* m) {
    uint8_t encoded[NX_UPDATE_MANIFEST_SIZE];
    EVP_MD_CTX* md = EVP_MD_CTX_new();
    size_t size = sizeof(m->signature);
    CHECK(nx_update_manifest_encode(m, encoded) == NX_UPDATE_OK);
    CHECK(md && EVP_DigestSignInit(md, NULL, NULL, NULL, f->signer) == 1);
    CHECK(EVP_DigestSign(md, m->signature, &size, encoded, NX_UPDATE_SIGNED_SIZE) == 1);
    CHECK(size == sizeof(m->signature));
    EVP_MD_CTX_free(md);
}

static void make_fixture(fixture_t* f) {
    EVP_PKEY_CTX* key_context;
    size_t public_size;
    memset(f, 0, sizeof(*f));
    f->flash.fd = -1;
    snprintf(f->path, sizeof(f->path), "/tmp/nexus-update-storage-%ld.flash", (long)getpid());
    CHECK(nx_crypto_use_default_provider() == NX_CRYPTO_OK);
    /* Ephemeral test-only key. No hard-coded product signing/manufacturing key. */
    CHECK(nx_crypto_random(f->metadata_key, sizeof(f->metadata_key)) == NX_CRYPTO_OK);
    key_context = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    CHECK(key_context && EVP_PKEY_keygen_init(key_context) == 1);
    CHECK(EVP_PKEY_keygen(key_context, &f->signer) == 1);
    EVP_PKEY_CTX_free(key_context);
    public_size = sizeof(f->public_key);
    CHECK(EVP_PKEY_get_raw_public_key(f->signer, f->public_key, &public_size) == 1);
    CHECK(public_size == sizeof(f->public_key));
    memset(f->images[0], 0x61, sizeof(f->images[0]));
    memset(f->images[1], 0xB7, sizeof(f->images[1]));
    f->baseline.schema = NX_UPDATE_MANIFEST_SCHEMA;
    f->baseline.board_id = 0x407;
    f->baseline.board_revision = 2;
    f->baseline.key_id = 7;
    f->baseline.image_size = sizeof(f->images[0]);
    f->baseline.firmware_version = 10;
    f->baseline.security_version = 3;
    CHECK(image_digest(f, 0, f->baseline.image_size, f->baseline.digest) == NX_UPDATE_OK);
    sign_manifest(f, &f->baseline);
    f->candidate = f->baseline;
    f->candidate.firmware_version = 11;
    f->candidate.security_version = 4;
    CHECK(image_digest(f, 1, f->candidate.image_size, f->candidate.digest) == NX_UPDATE_OK);
    sign_manifest(f, &f->candidate);
    f->port = (nx_update_port_t){f, 0x407, 2, 2, sizeof(f->images[0]),
        metadata_load, metadata_save, image_digest, signature_verify,
        counter_read, counter_advance};
}

static void reopen(fixture_t* f) {
    nx_file_flash_close(&f->flash);
    CHECK(nx_file_flash_open(&f->flash, f->path, 1024, 128, 8) == NX_STORAGE_OK);
    CHECK(nx_storage_open(&f->storage, &f->flash.port, 0, 512) == NX_STORAGE_OK);
    CHECK(nx_update_init(&f->update, &f->port) == NX_UPDATE_OK);
}

static void fresh(fixture_t* f) {
    nx_file_flash_close(&f->flash);
    CHECK(unlink(f->path) == 0 || access(f->path, F_OK) != 0);
    f->counter = 3;
    f->counter_advances = 0;
    reopen(f);
    CHECK(f->update.state.phase == NX_UPDATE_IDLE);
    CHECK(nx_update_provision(&f->update, 0, &f->baseline) == NX_UPDATE_OK);
}

static void stage(fixture_t* f) {
    CHECK(nx_update_stage(&f->update, 1, &f->candidate) == NX_UPDATE_OK);
}

static void trial(fixture_t* f) {
    nx_update_choice_t choice;
    stage(f);
    CHECK(nx_update_select(&f->update, &choice) == NX_UPDATE_OK);
    CHECK(choice.slot == 1 && choice.phase == NX_UPDATE_TRIAL);
}

static void record(fixture_t* f, uint8_t out[NX_UPDATE_RECORD_SIZE]) {
    CHECK(metadata_load(f, out, NX_UPDATE_RECORD_SIZE) == NX_UPDATE_OK);
}

static void old_or_new(fixture_t* f, const uint8_t* old, const uint8_t* newer) {
    uint8_t actual[NX_UPDATE_RECORD_SIZE];
    record(f, actual);
    CHECK(memcmp(actual, old, sizeof(actual)) == 0 ||
          memcmp(actual, newer, sizeof(actual)) == 0);
}

static void candidate_commit_matrix(fixture_t* f) {
    uint8_t before[NX_UPDATE_RECORD_SIZE], after[NX_UPDATE_RECORD_SIZE];
    uint64_t events;
    fresh(f);
    record(f, before);
    nx_file_flash_fail_after(&f->flash, -1);
    stage(f);
    events = f->flash.mutation_events;
    record(f, after);
    for (uint64_t point = 0; point <= events; ++point) {
        nx_update_choice_t choice;
        nx_update_status_t status;
        fresh(f);
        nx_file_flash_fail_after(&f->flash, (int64_t)point);
        status = nx_update_stage(&f->update, 1, &f->candidate);
        CHECK(status == NX_UPDATE_OK || status == NX_UPDATE_ESTORAGE);
        if (status != NX_UPDATE_OK) {
            CHECK(!f->update.initialized);
            CHECK(nx_update_select(&f->update, &choice) == NX_UPDATE_EINVAL);
        }
        reopen(f); /* Explicit reopen resolves a marker that may have committed. */
        old_or_new(f, before, after);
        CHECK(f->update.state.phase == NX_UPDATE_CONFIRMED ||
              f->update.state.phase == NX_UPDATE_STAGED);
        CHECK(nx_update_select(&f->update, &choice) == NX_UPDATE_OK);
        CHECK((choice.slot == 0 && choice.phase == NX_UPDATE_CONFIRMED) ||
              (choice.slot == 1 && choice.phase == NX_UPDATE_TRIAL));
        CHECK(f->counter == 3 && f->counter_advances == 0);
    }
    printf("Authenticated update candidate: %llu erase/program/sync boundaries passed\n",
           (unsigned long long)(events + 1));
}

static void trial_attempt_matrix(fixture_t* f) {
    uint8_t before[NX_UPDATE_RECORD_SIZE], after[NX_UPDATE_RECORD_SIZE];
    nx_update_choice_t choice;
    uint64_t events;
    fresh(f);
    stage(f);
    record(f, before);
    nx_file_flash_fail_after(&f->flash, -1);
    CHECK(nx_update_select(&f->update, &choice) == NX_UPDATE_OK);
    events = f->flash.mutation_events;
    record(f, after);
    for (uint64_t point = 0; point <= events; ++point) {
        nx_update_choice_t sentinel;
        nx_update_status_t status;
        fresh(f);
        stage(f);
        memset(&choice, 0x55, sizeof(choice));
        sentinel = choice;
        nx_file_flash_fail_after(&f->flash, (int64_t)point);
        status = nx_update_select(&f->update, &choice);
        CHECK(status == NX_UPDATE_OK || status == NX_UPDATE_ESTORAGE);
        if (status != NX_UPDATE_OK) {
            CHECK(!f->update.initialized && memcmp(&choice, &sentinel, sizeof(choice)) == 0);
        }
        reopen(f);
        old_or_new(f, before, after);
        CHECK(f->update.state.attempts <= 1);
        CHECK(nx_update_select(&f->update, &choice) == NX_UPDATE_OK);
        CHECK(choice.slot == 1 && choice.phase == NX_UPDATE_TRIAL);
        CHECK(f->update.state.attempts >= 1 && f->update.state.attempts <= 2);
        reopen(f);
        if (f->update.state.attempts == 2) {
            CHECK(nx_update_select(&f->update, &choice) == NX_UPDATE_OK);
            CHECK(choice.slot == 0 && choice.phase == NX_UPDATE_ROLLBACK);
            CHECK(choice.rollback_reason == NX_UPDATE_EXHAUSTED);
        }
        CHECK(f->counter == 3);
    }
    printf("Authenticated trial selection: %llu erase/program/sync boundaries passed\n",
           (unsigned long long)(events + 1));
}

static void confirmation_matrix(fixture_t* f) {
    nx_update_choice_t choice;
    uint64_t events;
    fresh(f);
    trial(f);
    nx_file_flash_fail_after(&f->flash, -1);
    CHECK(nx_update_confirm(&f->update, 1, f->candidate.digest) == NX_UPDATE_OK);
    events = f->flash.mutation_events;
    CHECK(f->counter == 4 && f->counter_advances == 1);
    for (uint64_t point = 0; point <= events; ++point) {
        nx_update_status_t status;
        uint64_t durable_floor;
        fresh(f);
        trial(f);
        nx_file_flash_fail_after(&f->flash, (int64_t)point);
        status = nx_update_confirm(&f->update, 1, f->candidate.digest);
        CHECK(status == NX_UPDATE_OK || status == NX_UPDATE_ESTORAGE);
        CHECK(f->counter == 3 || f->counter == 4);
        durable_floor = f->counter;
        if (status != NX_UPDATE_OK) CHECK(!f->update.initialized);
        reopen(f);
        CHECK(f->counter == durable_floor);
        if (durable_floor == 4) {
            /* A promoted security floor permanently excludes the old slot. */
            CHECK(nx_update_rollback(&f->update) != NX_UPDATE_OK);
        }
        CHECK(nx_update_select(&f->update, &choice) == NX_UPDATE_OK);
        CHECK(choice.slot == 1 && f->counter >= durable_floor);
        if (choice.phase == NX_UPDATE_TRIAL) {
            CHECK(f->counter == 3 && !f->update.state.confirm_pending);
            CHECK(nx_update_confirm(&f->update, 1, f->candidate.digest) == NX_UPDATE_OK);
        } else {
            CHECK(choice.phase == NX_UPDATE_CONFIRMED && f->counter == 4);
        }
        CHECK(f->counter == 4 && f->counter_advances == 1);
        reopen(f);
        CHECK(nx_update_select(&f->update, &choice) == NX_UPDATE_OK);
        CHECK(choice.slot == 1 && choice.phase == NX_UPDATE_CONFIRMED && f->counter == 4);
        CHECK(nx_update_stage(&f->update, 0, &f->baseline) == NX_UPDATE_EDOWNGRADE);
    }
    printf("Authenticated confirm intent/counter/final commit: %llu interruption boundaries passed\n",
           (unsigned long long)(events + 1));
}

static void authenticated_corruption(fixture_t* f) {
    uint8_t envelope[ENVELOPE_SIZE];
    size_t size = sizeof(envelope);
    fresh(f);
    CHECK(nx_storage_load(&f->storage, envelope, &size) == NX_STORAGE_OK);
    CHECK(size == sizeof(envelope));
    envelope[ENVELOPE_CIPHER_OFFSET + 20] ^= 1;
    /* Re-saving calculates a valid storage CRC; AEAD must still reject it. */
    CHECK(nx_storage_save(&f->storage, envelope, size) == NX_STORAGE_OK);
    nx_file_flash_close(&f->flash);
    CHECK(nx_file_flash_open(&f->flash, f->path, 1024, 128, 8) == NX_STORAGE_OK);
    CHECK(nx_storage_open(&f->storage, &f->flash.port, 0, 512) == NX_STORAGE_OK);
    CHECK(nx_update_init(&f->update, &f->port) == NX_UPDATE_ECORRUPT);
    CHECK(!f->update.initialized);
    fresh(f);
    f->metadata_key[0] ^= 1;
    CHECK(nx_update_init(&f->update, &f->port) == NX_UPDATE_ECORRUPT);
    f->metadata_key[0] ^= 1;
    puts("Authenticated metadata: valid storage CRC with forged ciphertext and wrong key rejected.");
}

int main(void) {
    fixture_t fixture;
    make_fixture(&fixture);
    candidate_commit_matrix(&fixture);
    trial_attempt_matrix(&fixture);
    confirmation_matrix(&fixture);
    authenticated_corruption(&fixture);
    nx_file_flash_close(&fixture.flash);
    CHECK(unlink(fixture.path) == 0);
    EVP_PKEY_free(fixture.signer);
    nx_crypto_secure_zero(fixture.metadata_key, sizeof(fixture.metadata_key));
    puts("Update/storage/AEAD integration passed; real boot and secure hardware counter not exercised.");
    return 0;
}
