/* SEC002 native policy regressions. Ephemeral Ed25519 key, no shipped secrets. */
#include "nexus/update.h"
#include "security/crypto.h"

#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); \
} } while (0)

typedef struct {
    uint8_t record[NX_UPDATE_RECORD_SIZE];
    bool exists;
    uint8_t images[3][128];
    EVP_PKEY* signer;
    uint8_t public_key[32];
    uint64_t counter;
    unsigned save_calls;
    unsigned advance_calls;
    unsigned fail_save_call;
    bool ambiguous_save;
    bool fail_load;
    bool fail_counter_read;
    bool fail_advance;
    bool ambiguous_advance;
    bool dishonest_advance;
    bool revoke_key;
    nx_update_port_t port;
    nx_update_manifest_t baseline;
    nx_update_manifest_t candidate;
} fixture_t;

static nx_update_status_t load(void* opaque, uint8_t* out, size_t size) {
    fixture_t* f = opaque;
    CHECK(size == sizeof(f->record));
    if (f->fail_load) return NX_UPDATE_ESTORAGE;
    if (!f->exists) return NX_UPDATE_ENOTFOUND;
    memcpy(out, f->record, size);
    return NX_UPDATE_OK;
}

static nx_update_status_t save(void* opaque, const uint8_t* in, size_t size) {
    fixture_t* f = opaque;
    CHECK(size == sizeof(f->record));
    ++f->save_calls;
    if (f->save_calls == f->fail_save_call && !f->ambiguous_save)
        return NX_UPDATE_ESTORAGE;
    memcpy(f->record, in, size);
    f->exists = true;
    return f->save_calls == f->fail_save_call ? NX_UPDATE_ESTORAGE : NX_UPDATE_OK;
}

static nx_update_status_t image_digest(void* opaque, uint32_t slot,
                                       uint32_t size, uint8_t out[32]) {
    fixture_t* f = opaque;
    if (slot >= 3 || size > sizeof(f->images[slot])) return NX_UPDATE_EDIGEST;
    return nx_crypto_sha256(f->images[slot], size, out) == NX_CRYPTO_OK
               ? NX_UPDATE_OK : NX_UPDATE_EDIGEST;
}

static nx_update_status_t verify_signature(void* opaque, uint32_t key,
                                           const uint8_t* bytes, size_t size,
                                           const uint8_t signature[64]) {
    fixture_t* f = opaque;
    if (key != 7 || f->revoke_key) return NX_UPDATE_EAUTH;
    return nx_crypto_verify_ed25519(f->public_key, bytes, size, signature) == NX_CRYPTO_OK
               ? NX_UPDATE_OK : NX_UPDATE_EAUTH;
}

static nx_update_status_t counter_read(void* opaque, uint64_t* value) {
    fixture_t* f = opaque;
    if (f->fail_counter_read) return NX_UPDATE_ECOUNTER;
    *value = f->counter;
    return NX_UPDATE_OK;
}

static nx_update_status_t counter_advance(void* opaque, uint64_t expected,
                                         uint64_t target) {
    fixture_t* f = opaque;
    ++f->advance_calls;
    CHECK(expected == f->counter && target > expected);
    if (f->fail_advance && !f->ambiguous_advance) return NX_UPDATE_ECOUNTER;
    if (!f->dishonest_advance) f->counter = target;
    return f->fail_advance ? NX_UPDATE_ECOUNTER : NX_UPDATE_OK;
}

static void sign(fixture_t* f, nx_update_manifest_t* m) {
    uint8_t encoded[NX_UPDATE_MANIFEST_SIZE];
    EVP_MD_CTX* md = EVP_MD_CTX_new();
    size_t size = sizeof(m->signature);
    CHECK(nx_update_manifest_encode(m, encoded) == NX_UPDATE_OK);
    CHECK(md && EVP_DigestSignInit(md, NULL, NULL, NULL, f->signer) == 1);
    CHECK(EVP_DigestSign(md, m->signature, &size, encoded, NX_UPDATE_SIGNED_SIZE) == 1);
    CHECK(size == sizeof(m->signature));
    EVP_MD_CTX_free(md);
}

static void start(fixture_t* f, nx_update_t* u) {
    EVP_PKEY_CTX* key_context;
    size_t public_size = sizeof(f->public_key);
    memset(f, 0, sizeof(*f));
    CHECK(nx_crypto_use_default_provider() == NX_CRYPTO_OK);
    key_context = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    CHECK(key_context && EVP_PKEY_keygen_init(key_context) == 1);
    CHECK(EVP_PKEY_keygen(key_context, &f->signer) == 1);
    EVP_PKEY_CTX_free(key_context);
    CHECK(EVP_PKEY_get_raw_public_key(f->signer, f->public_key, &public_size) == 1);
    CHECK(public_size == sizeof(f->public_key));
    memset(f->images[0], 0x31, sizeof(f->images[0]));
    memset(f->images[1], 0xA6, sizeof(f->images[1]));
    f->counter = 3;
    f->port.user = f;
    f->port.board_id = 0x11223344;
    f->port.board_revision = 2;
    f->port.trial_limit = 2;
    f->port.maximum_image_size = sizeof(f->images[0]);
    f->port.load = load;
    f->port.save = save;
    f->port.image_digest = image_digest;
    f->port.verify_signature = verify_signature;
    f->port.counter_read = counter_read;
    f->port.counter_advance = counter_advance;
    f->baseline.schema = 1;
    f->baseline.board_id = f->port.board_id;
    f->baseline.board_revision = f->port.board_revision;
    f->baseline.key_id = 7;
    f->baseline.image_size = sizeof(f->images[0]);
    f->baseline.firmware_version = 100;
    f->baseline.security_version = f->counter;
    CHECK(image_digest(f, 0, f->baseline.image_size, f->baseline.digest) == NX_UPDATE_OK);
    sign(f, &f->baseline);
    f->candidate = f->baseline;
    f->candidate.firmware_version = 101;
    f->candidate.security_version = 4;
    CHECK(image_digest(f, 1, f->candidate.image_size, f->candidate.digest) == NX_UPDATE_OK);
    sign(f, &f->candidate);
    CHECK(nx_update_init(u, &f->port) == NX_UPDATE_OK);
    CHECK(u->state.phase == NX_UPDATE_IDLE);
    CHECK(nx_update_provision(u, 0, &f->baseline) == NX_UPDATE_OK);
}

static void finish(fixture_t* f) {
    EVP_PKEY_free(f->signer);
}

static void trial(fixture_t* f, nx_update_t* u) {
    nx_update_choice_t choice;
    CHECK(nx_update_stage(u, 1, &f->candidate) == NX_UPDATE_OK);
    CHECK(nx_update_select(u, &choice) == NX_UPDATE_OK);
    CHECK(choice.slot == 1 && choice.phase == NX_UPDATE_TRIAL);
    CHECK(u->state.attempts == 1);
}

static void test_codec_and_baseline(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_manifest_t decoded;
    uint8_t bytes[NX_UPDATE_MANIFEST_SIZE], again[NX_UPDATE_MANIFEST_SIZE];
    nx_update_choice_t choice;
    start(&f, &u);
    CHECK(nx_update_manifest_encode(&f.baseline, bytes) == NX_UPDATE_OK);
    CHECK(bytes[8] == 0x44 && bytes[9] == 0x33 && bytes[10] == 0x22 && bytes[11] == 0x11);
    CHECK(bytes[24] == 100 && bytes[32] == 3);
    CHECK(nx_update_manifest_decode(bytes, sizeof(bytes), &decoded) == NX_UPDATE_OK);
    CHECK(nx_update_manifest_encode(&decoded, again) == NX_UPDATE_OK);
    CHECK(memcmp(bytes, again, sizeof(bytes)) == 0);
    CHECK(nx_update_manifest_decode(bytes, sizeof(bytes) - 1, &decoded) == NX_UPDATE_EINVAL);
    bytes[4] = 2;
    CHECK(nx_update_manifest_decode(bytes, sizeof(bytes), &decoded) == NX_UPDATE_ECORRUPT);
    CHECK(nx_update_provision(&u, 0, &f.baseline) == NX_UPDATE_ESTATE);
    CHECK(nx_update_init(&u, &u.port) == NX_UPDATE_OK);
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK);
    CHECK(choice.slot == 0 && choice.phase == NX_UPDATE_CONFIRMED);
    CHECK(f.advance_calls == 0 && f.counter == 3);
    finish(&f);
}

static void test_manifest_rejections(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_manifest_t bad;
    unsigned saves;
    start(&f, &u);
    saves = f.save_calls;
    bad = f.candidate;
    bad.signature[0] ^= 1;
    CHECK(nx_update_stage(&u, 1, &bad) == NX_UPDATE_EAUTH);
    bad = f.candidate;
    bad.board_id ^= 1;
    sign(&f, &bad);
    CHECK(nx_update_stage(&u, 1, &bad) == NX_UPDATE_EBOARD);
    bad = f.candidate;
    ++bad.board_revision;
    sign(&f, &bad);
    CHECK(nx_update_stage(&u, 1, &bad) == NX_UPDATE_EBOARD);
    bad = f.candidate;
    bad.key_id = 8;
    sign(&f, &bad);
    CHECK(nx_update_stage(&u, 1, &bad) == NX_UPDATE_EAUTH);
    bad = f.candidate;
    bad.security_version = 2;
    sign(&f, &bad);
    CHECK(nx_update_stage(&u, 1, &bad) == NX_UPDATE_EDOWNGRADE);
    bad = f.candidate;
    bad.firmware_version = f.baseline.firmware_version;
    sign(&f, &bad);
    CHECK(nx_update_stage(&u, 1, &bad) == NX_UPDATE_EDOWNGRADE);
    bad = f.candidate;
    bad.digest[31] ^= 1;
    sign(&f, &bad);
    CHECK(nx_update_stage(&u, 1, &bad) == NX_UPDATE_EDIGEST);
    bad = f.candidate;
    ++bad.image_size;
    sign(&f, &bad);
    CHECK(nx_update_stage(&u, 1, &bad) == NX_UPDATE_EINVAL);
    CHECK(nx_update_stage(&u, 0, &f.candidate) == NX_UPDATE_EINVAL);
    CHECK(f.save_calls == saves && u.state.phase == NX_UPDATE_CONFIRMED);
    finish(&f);
}

static void test_restart_trial_budget(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_choice_t choice;
    start(&f, &u);
    CHECK(nx_update_stage(&u, 1, &f.candidate) == NX_UPDATE_OK);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK);
    CHECK(u.state.phase == NX_UPDATE_STAGED && u.state.attempts == 0);
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK && choice.slot == 1);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK);
    CHECK(u.state.attempts == 1);
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK && choice.slot == 1);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK);
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK && choice.slot == 0);
    CHECK(choice.phase == NX_UPDATE_ROLLBACK && choice.rollback_reason == NX_UPDATE_EXHAUSTED);
    CHECK(f.counter == 3 && f.advance_calls == 0);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK);
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK && choice.slot == 0);
    finish(&f);
}

static void test_tamper_and_verified_rollback(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_choice_t choice, original;
    start(&f, &u);
    CHECK(nx_update_stage(&u, 1, &f.candidate) == NX_UPDATE_OK);
    f.images[1][2] ^= 1;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK);
    CHECK(choice.slot == 0 && choice.phase == NX_UPDATE_ROLLBACK);
    CHECK(choice.rollback_reason == NX_UPDATE_EDIGEST);
    f.images[1][2] ^= 1;
    CHECK(nx_update_stage(&u, 1, &f.candidate) == NX_UPDATE_OK);
    f.images[0][3] ^= 1;
    f.images[1][2] ^= 1;
    memset(&choice, 0x55, sizeof(choice));
    original = choice;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_EDIGEST);
    CHECK(memcmp(&choice, &original, sizeof(choice)) == 0);
    CHECK(u.state.phase == NX_UPDATE_STAGED);
    f.images[0][3] ^= 1;
    f.revoke_key = true;
    CHECK(nx_update_rollback(&u) == NX_UPDATE_EAUTH);
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_EAUTH);
    finish(&f);
}

static void test_save_failure_and_ambiguous_commit(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_choice_t choice, original;
    start(&f, &u);
    f.fail_save_call = f.save_calls + 1;
    CHECK(nx_update_stage(&u, 1, &f.candidate) == NX_UPDATE_ESTORAGE);
    CHECK(!u.initialized);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK);
    CHECK(u.state.phase == NX_UPDATE_CONFIRMED);
    f.fail_save_call = 0;
    CHECK(nx_update_stage(&u, 1, &f.candidate) == NX_UPDATE_OK);
    f.fail_save_call = f.save_calls + 1;
    f.ambiguous_save = true;
    memset(&choice, 0x55, sizeof(choice));
    original = choice;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_ESTORAGE);
    CHECK(!u.initialized && memcmp(&choice, &original, sizeof(choice)) == 0);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK);
    CHECK(u.state.phase == NX_UPDATE_TRIAL && u.state.attempts == 1);
    f.fail_save_call = 0;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK && choice.slot == 1);
    CHECK(u.state.attempts == 2);
    finish(&f);
}

static void test_confirm_health_identity_and_intent_failure(void) {
    fixture_t f;
    nx_update_t u;
    uint8_t wrong_digest[32] = {0};
    start(&f, &u);
    CHECK(nx_update_stage(&u, 1, &f.candidate) == NX_UPDATE_OK);
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_ESTATE);
    {
        nx_update_choice_t choice;
        CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK);
    }
    CHECK(nx_update_confirm(&u, 0, f.candidate.digest) == NX_UPDATE_ESTATE);
    CHECK(nx_update_confirm(&u, 1, wrong_digest) == NX_UPDATE_ESTATE);
    f.fail_save_call = f.save_calls + 1;
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_ESTORAGE);
    CHECK(f.counter == 3 && f.advance_calls == 0 && !u.initialized);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK && !u.state.confirm_pending);
    f.fail_save_call = 0;
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_OK);
    CHECK(f.counter == 4 && u.state.phase == NX_UPDATE_CONFIRMED);
    CHECK(u.state.active_slot == 1 && !u.state.has_candidate);
    CHECK(nx_update_stage(&u, 0, &f.baseline) == NX_UPDATE_EDOWNGRADE);
    finish(&f);
}

static void test_counter_failure_and_restart_resume(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_choice_t choice;
    start(&f, &u);
    trial(&f, &u);
    f.fail_advance = true;
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_ECOUNTER);
    CHECK(u.state.confirm_pending && f.counter == 3);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK && u.state.confirm_pending);
    f.fail_advance = false;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK);
    CHECK(choice.slot == 1 && choice.phase == NX_UPDATE_CONFIRMED && f.counter == 4);
    finish(&f);
}

static void test_ambiguous_counter_and_noop_success(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_choice_t choice;
    start(&f, &u);
    trial(&f, &u);
    f.dishonest_advance = true;
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_ECOUNTER);
    CHECK(f.counter == 3 && u.state.confirm_pending);
    f.dishonest_advance = false;
    f.fail_advance = true;
    f.ambiguous_advance = true;
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_ECOUNTER);
    CHECK(f.counter == 4 && u.state.confirm_pending);
    CHECK(nx_update_rollback(&u) == NX_UPDATE_ECOUNTER);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK);
    f.fail_advance = false;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK);
    CHECK(choice.slot == 1 && choice.phase == NX_UPDATE_CONFIRMED);
    CHECK(f.advance_calls == 2);
    finish(&f);
}

static void test_final_commit_failure_and_counter_floor(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_choice_t choice;
    start(&f, &u);
    trial(&f, &u);
    f.fail_save_call = f.save_calls + 2;
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_ESTORAGE);
    CHECK(f.counter == 4 && !u.initialized);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK && u.state.confirm_pending);
    CHECK(nx_update_rollback(&u) == NX_UPDATE_ECOUNTER);
    f.fail_save_call = 0;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK);
    CHECK(choice.slot == 1 && choice.phase == NX_UPDATE_CONFIRMED);
    f.counter = 5;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_EDOWNGRADE);
    finish(&f);
}

static void test_update_without_security_floor_change(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_choice_t choice;
    start(&f, &u);
    f.candidate.security_version = f.baseline.security_version;
    sign(&f, &f.candidate);
    trial(&f, &u);
    f.fail_save_call = f.save_calls + 2;
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_ESTORAGE);
    CHECK(f.counter == 3 && f.advance_calls == 0);
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK && u.state.confirm_pending);
    f.fail_save_call = 0;
    /* Old identity remains above the floor when no counter advanced. */
    CHECK(nx_update_rollback(&u) == NX_UPDATE_OK);
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_OK && choice.slot == 0);
    trial(&f, &u);
    CHECK(nx_update_confirm(&u, 1, f.candidate.digest) == NX_UPDATE_OK);
    CHECK(f.counter == 3 && f.advance_calls == 0 && u.state.active_slot == 1);
    CHECK(nx_update_stage(&u, 0, &f.baseline) == NX_UPDATE_EDOWNGRADE);
    finish(&f);
}

static void test_load_corruption_and_port_failures(void) {
    fixture_t f;
    nx_update_t u;
    nx_update_choice_t choice;
    uint8_t saved[NX_UPDATE_RECORD_SIZE];
    start(&f, &u);
    memcpy(saved, f.record, sizeof(saved));
    f.fail_load = true;
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_ESTORAGE && !u.initialized);
    f.fail_load = false;
    f.record[36] = 1;
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_ECORRUPT && !u.initialized);
    memcpy(f.record, saved, sizeof(saved));
    f.record[8] = 2; /* Claimed trial without candidate/attempt budget. */
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_ECORRUPT);
    memcpy(f.record, saved, sizeof(saved));
    f.record[32] = 0xFF;
    f.record[33] = f.record[34] = f.record[35] = 0xFF;
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_ECORRUPT);
    memcpy(f.record, saved, sizeof(saved));
    CHECK(nx_update_init(&u, &f.port) == NX_UPDATE_OK);
    f.fail_counter_read = true;
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_ECOUNTER);
    CHECK(nx_update_stage(&u, 1, &f.candidate) == NX_UPDATE_ECOUNTER);
    f.fail_counter_read = false;
    CHECK(nx_crypto_set_provider(NULL) == NX_CRYPTO_UNSUPPORTED);
    CHECK(nx_update_select(&u, &choice) == NX_UPDATE_EAUTH);
    CHECK(nx_crypto_use_default_provider() == NX_CRYPTO_OK);
    finish(&f);
}

int main(void) {
    test_codec_and_baseline();
    test_manifest_rejections();
    test_restart_trial_budget();
    test_tamper_and_verified_rollback();
    test_save_failure_and_ambiguous_commit();
    test_confirm_health_identity_and_intent_failure();
    test_counter_failure_and_restart_resume();
    test_ambiguous_counter_and_noop_success();
    test_final_commit_failure_and_counter_floor();
    test_update_without_security_floor_change();
    test_load_corruption_and_port_failures();
    puts("Safe update policy: 11 regression scenarios passed; hardware boot not exercised.");
    return 0;
}
