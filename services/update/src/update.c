#include "nexus/update.h"

#include <string.h>

static void put32(uint8_t* p, uint32_t x) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(x >> (8u * i));
}

static uint32_t get32(const uint8_t* p) {
    uint32_t x = 0;
    for (unsigned i = 0; i < 4; ++i) x |= (uint32_t)p[i] << (8u * i);
    return x;
}

static void put64(uint8_t* p, uint64_t x) {
    for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(x >> (8u * i));
}

static uint64_t get64(const uint8_t* p) {
    uint64_t x = 0;
    for (unsigned i = 0; i < 8; ++i) x |= (uint64_t)p[i] << (8u * i);
    return x;
}

static bool equal_digest(const uint8_t* a, const uint8_t* b) {
    uint8_t difference = 0;
    for (size_t i = 0; i < 32; ++i) difference |= a[i] ^ b[i];
    return difference == 0;
}

nx_update_status_t nx_update_manifest_encode(const nx_update_manifest_t* m,
                                             uint8_t out[NX_UPDATE_MANIFEST_SIZE]) {
    if (!m || !out || m->schema != NX_UPDATE_MANIFEST_SCHEMA || !m->image_size)
        return NX_UPDATE_EINVAL;
    memcpy(out, "NXUF", 4);
    put32(out + 4, m->schema);
    put32(out + 8, m->board_id);
    put32(out + 12, m->board_revision);
    put32(out + 16, m->key_id);
    put32(out + 20, m->image_size);
    put64(out + 24, m->firmware_version);
    put64(out + 32, m->security_version);
    memcpy(out + 40, m->digest, 32);
    memcpy(out + NX_UPDATE_SIGNED_SIZE, m->signature, 64);
    return NX_UPDATE_OK;
}

nx_update_status_t nx_update_manifest_decode(const uint8_t* in, size_t size,
                                             nx_update_manifest_t* m) {
    nx_update_manifest_t decoded;
    if (!in || !m || size != NX_UPDATE_MANIFEST_SIZE) return NX_UPDATE_EINVAL;
    if (memcmp(in, "NXUF", 4) || get32(in + 4) != NX_UPDATE_MANIFEST_SCHEMA ||
        !get32(in + 20)) return NX_UPDATE_ECORRUPT;
    memset(&decoded, 0, sizeof(decoded));
    decoded.schema = get32(in + 4);
    decoded.board_id = get32(in + 8);
    decoded.board_revision = get32(in + 12);
    decoded.key_id = get32(in + 16);
    decoded.image_size = get32(in + 20);
    decoded.firmware_version = get64(in + 24);
    decoded.security_version = get64(in + 32);
    memcpy(decoded.digest, in + 40, 32);
    memcpy(decoded.signature, in + NX_UPDATE_SIGNED_SIZE, 64);
    *m = decoded;
    return NX_UPDATE_OK;
}

static bool zeros(const uint8_t* p, size_t n) {
    uint8_t aggregate = 0;
    for (size_t i = 0; i < n; ++i) aggregate |= p[i];
    return aggregate == 0;
}

static nx_update_status_t encode_state(const nx_update_state_t* s,
                                       uint8_t out[NX_UPDATE_RECORD_SIZE]) {
    memset(out, 0, NX_UPDATE_RECORD_SIZE);
    memcpy(out, "NXUS", 4);
    put32(out + 4, 1);
    put32(out + 8, (uint32_t)s->phase);
    put32(out + 12, (s->has_active ? 1u : 0u) |
                        (s->has_candidate ? 2u : 0u) |
                        (s->confirm_pending ? 4u : 0u));
    put32(out + 16, s->attempts);
    put32(out + 20, s->attempt_limit);
    put32(out + 24, s->active_slot);
    put32(out + 28, s->candidate_slot);
    put32(out + 32, (uint32_t)s->rollback_reason);
    if (s->has_active && nx_update_manifest_encode(&s->active, out + 40))
        return NX_UPDATE_ECORRUPT;
    if (s->has_candidate && nx_update_manifest_encode(&s->candidate, out + 176))
        return NX_UPDATE_ECORRUPT;
    return NX_UPDATE_OK;
}

static nx_update_status_t decode_state(const uint8_t in[NX_UPDATE_RECORD_SIZE],
                                       nx_update_state_t* s) {
    uint32_t flags = get32(in + 12);
    if (memcmp(in, "NXUS", 4) || get32(in + 4) != 1 || flags > 7 ||
        get32(in + 8) > NX_UPDATE_ROLLBACK || !zeros(in + 36, 4))
        return NX_UPDATE_ECORRUPT;
    memset(s, 0, sizeof(*s));
    s->phase = (nx_update_phase_t)get32(in + 8);
    s->has_active = (flags & 1u) != 0;
    s->has_candidate = (flags & 2u) != 0;
    s->confirm_pending = (flags & 4u) != 0;
    s->attempts = get32(in + 16);
    s->attempt_limit = get32(in + 20);
    s->active_slot = get32(in + 24);
    s->candidate_slot = get32(in + 28);
    s->rollback_reason = (nx_update_status_t)get32(in + 32);
    if (!s->attempt_limit || s->attempt_limit > NX_UPDATE_MAX_ATTEMPTS ||
        s->attempts > s->attempt_limit ||
        get32(in + 32) > NX_UPDATE_EXHAUSTED) return NX_UPDATE_ECORRUPT;
    if (s->has_active) {
        if (nx_update_manifest_decode(in + 40, NX_UPDATE_MANIFEST_SIZE, &s->active))
            return NX_UPDATE_ECORRUPT;
    } else if (s->active_slot || !zeros(in + 40, NX_UPDATE_MANIFEST_SIZE)) {
        return NX_UPDATE_ECORRUPT;
    }
    if (s->has_candidate) {
        if (nx_update_manifest_decode(in + 176, NX_UPDATE_MANIFEST_SIZE, &s->candidate))
            return NX_UPDATE_ECORRUPT;
    } else if (s->candidate_slot || !zeros(in + 176, NX_UPDATE_MANIFEST_SIZE)) {
        return NX_UPDATE_ECORRUPT;
    }
    switch (s->phase) {
    case NX_UPDATE_IDLE:
        if (flags || s->attempts || s->rollback_reason) return NX_UPDATE_ECORRUPT;
        break;
    case NX_UPDATE_CONFIRMED:
    case NX_UPDATE_ROLLBACK:
        if (flags != 1 || s->attempts ||
            (s->phase == NX_UPDATE_CONFIRMED && s->rollback_reason))
            return NX_UPDATE_ECORRUPT;
        break;
    case NX_UPDATE_STAGED:
        if (flags != 3 || s->attempts || s->rollback_reason)
            return NX_UPDATE_ECORRUPT;
        break;
    case NX_UPDATE_TRIAL:
        if ((flags != 3 && flags != 7) || !s->attempts || s->rollback_reason)
            return NX_UPDATE_ECORRUPT;
        break;
    }
    if (s->has_candidate &&
        (s->active_slot == s->candidate_slot ||
         s->candidate.firmware_version <= s->active.firmware_version ||
         s->candidate.security_version < s->active.security_version))
        return NX_UPDATE_ECORRUPT;
    return NX_UPDATE_OK;
}

static nx_update_status_t save_state(nx_update_t* u, const nx_update_state_t* s) {
    uint8_t record[NX_UPDATE_RECORD_SIZE];
    nx_update_status_t status = encode_state(s, record);
    if (status != NX_UPDATE_OK) return status;
    if (u->port.save(u->port.user, record, sizeof(record)) != NX_UPDATE_OK) {
        u->initialized = false;
        return NX_UPDATE_ESTORAGE;
    }
    u->state = *s;
    return NX_UPDATE_OK;
}

static nx_update_status_t counter(nx_update_t* u, uint64_t* value) {
    return u->port.counter_read(u->port.user, value) == NX_UPDATE_OK
               ? NX_UPDATE_OK : NX_UPDATE_ECOUNTER;
}

static nx_update_status_t verify(nx_update_t* u, uint32_t slot,
                                 const nx_update_manifest_t* m, uint64_t floor) {
    uint8_t bytes[NX_UPDATE_MANIFEST_SIZE], digest[32];
    if (nx_update_manifest_encode(m, bytes) != NX_UPDATE_OK ||
        m->image_size > u->port.maximum_image_size) return NX_UPDATE_EINVAL;
    if (m->board_id != u->port.board_id ||
        m->board_revision != u->port.board_revision) return NX_UPDATE_EBOARD;
    if (m->security_version < floor) return NX_UPDATE_EDOWNGRADE;
    if (u->port.verify_signature(u->port.user, m->key_id, bytes,
                                 NX_UPDATE_SIGNED_SIZE, m->signature) != NX_UPDATE_OK)
        return NX_UPDATE_EAUTH;
    if (u->port.image_digest(u->port.user, slot, m->image_size, digest) != NX_UPDATE_OK)
        return NX_UPDATE_EDIGEST;
    return equal_digest(digest, m->digest) ? NX_UPDATE_OK : NX_UPDATE_EDIGEST;
}

nx_update_status_t nx_update_init(nx_update_t* u, const nx_update_port_t* p) {
    uint8_t record[NX_UPDATE_RECORD_SIZE];
    nx_update_port_t port;
    nx_update_status_t status;
    if (!u || !p || !p->load || !p->save || !p->image_digest ||
        !p->verify_signature || !p->counter_read || !p->counter_advance ||
        !p->maximum_image_size || !p->trial_limit ||
        p->trial_limit > NX_UPDATE_MAX_ATTEMPTS) return NX_UPDATE_EINVAL;
    port = *p;
    memset(u, 0, sizeof(*u));
    u->port = port;
    status = port.load(port.user, record, sizeof(record));
    if (status == NX_UPDATE_ENOTFOUND) {
        u->state.phase = NX_UPDATE_IDLE;
        u->state.attempt_limit = port.trial_limit;
    } else if (status == NX_UPDATE_OK) {
        status = decode_state(record, &u->state);
        if (status != NX_UPDATE_OK) return status;
    } else {
        return status == NX_UPDATE_ECORRUPT ? status : NX_UPDATE_ESTORAGE;
    }
    u->initialized = true;
    return NX_UPDATE_OK;
}

nx_update_status_t nx_update_provision(nx_update_t* u, uint32_t slot,
                                      const nx_update_manifest_t* m) {
    nx_update_state_t next;
    nx_update_status_t status;
    uint64_t floor;
    if (!u || !u->initialized || !m) return NX_UPDATE_EINVAL;
    if (u->state.phase != NX_UPDATE_IDLE) return NX_UPDATE_ESTATE;
    if ((status = counter(u, &floor)) != NX_UPDATE_OK) return status;
    if (m->security_version != floor) return NX_UPDATE_EDOWNGRADE;
    if ((status = verify(u, slot, m, floor)) != NX_UPDATE_OK) return status;
    next = u->state;
    next.has_active = true;
    next.active = *m;
    next.active_slot = slot;
    next.phase = NX_UPDATE_CONFIRMED;
    return save_state(u, &next);
}

nx_update_status_t nx_update_stage(nx_update_t* u, uint32_t slot,
                                  const nx_update_manifest_t* m) {
    nx_update_state_t next;
    nx_update_status_t status;
    uint64_t floor;
    if (!u || !u->initialized || !m) return NX_UPDATE_EINVAL;
    if (u->state.phase != NX_UPDATE_CONFIRMED && u->state.phase != NX_UPDATE_ROLLBACK)
        return NX_UPDATE_ESTATE;
    if (slot == u->state.active_slot) return NX_UPDATE_EINVAL;
    if ((status = counter(u, &floor)) != NX_UPDATE_OK) return status;
    if ((status = verify(u, u->state.active_slot, &u->state.active, floor)) != NX_UPDATE_OK)
        return status;
    if (m->firmware_version <= u->state.active.firmware_version ||
        m->security_version < u->state.active.security_version)
        return NX_UPDATE_EDOWNGRADE;
    if ((status = verify(u, slot, m, floor)) != NX_UPDATE_OK) return status;
    next = u->state;
    next.phase = NX_UPDATE_STAGED;
    next.has_candidate = true;
    next.confirm_pending = false;
    next.candidate = *m;
    next.candidate_slot = slot;
    next.attempts = 0;
    next.attempt_limit = u->port.trial_limit;
    next.rollback_reason = NX_UPDATE_OK;
    return save_state(u, &next);
}

static nx_update_status_t rollback(nx_update_t* u, nx_update_status_t reason) {
    nx_update_state_t next;
    nx_update_status_t status;
    uint64_t floor;
    if (u->state.phase != NX_UPDATE_STAGED && u->state.phase != NX_UPDATE_TRIAL)
        return NX_UPDATE_ESTATE;
    if ((status = counter(u, &floor)) != NX_UPDATE_OK) return status;
    /* A durable health intent cannot be canceled after its counter committed. */
    if (u->state.confirm_pending && floor > u->state.active.security_version)
        return NX_UPDATE_ECOUNTER;
    if ((status = verify(u, u->state.active_slot, &u->state.active, floor)) != NX_UPDATE_OK)
        return status;
    next = u->state;
    next.phase = NX_UPDATE_ROLLBACK;
    next.has_candidate = false;
    next.confirm_pending = false;
    next.attempts = 0;
    next.candidate_slot = 0;
    next.rollback_reason = reason;
    memset(&next.candidate, 0, sizeof(next.candidate));
    return save_state(u, &next);
}

nx_update_status_t nx_update_rollback(nx_update_t* u) {
    if (!u || !u->initialized) return NX_UPDATE_EINVAL;
    return rollback(u, NX_UPDATE_OK);
}

nx_update_status_t nx_update_confirm(nx_update_t* u, uint32_t running_slot,
                                    const uint8_t running_digest[32]) {
    nx_update_state_t next;
    nx_update_status_t status;
    uint64_t floor, target;
    if (!u || !u->initialized || !running_digest) return NX_UPDATE_EINVAL;
    if (u->state.phase != NX_UPDATE_TRIAL ||
        running_slot != u->state.candidate_slot ||
        !equal_digest(running_digest, u->state.candidate.digest)) return NX_UPDATE_ESTATE;
    if ((status = counter(u, &floor)) != NX_UPDATE_OK) return status;
    target = u->state.candidate.security_version;
    if (floor > target) return NX_UPDATE_ECOUNTER;
    if ((status = verify(u, running_slot, &u->state.candidate, floor)) != NX_UPDATE_OK)
        return status;
    if (!u->state.confirm_pending) {
        next = u->state;
        next.confirm_pending = true;
        if ((status = save_state(u, &next)) != NX_UPDATE_OK) return status;
    }
    if (floor < target &&
        u->port.counter_advance(u->port.user, floor, target) != NX_UPDATE_OK)
        return NX_UPDATE_ECOUNTER;
    if ((status = counter(u, &floor)) != NX_UPDATE_OK) return status;
    if (floor != target) return NX_UPDATE_ECOUNTER;
    next = u->state;
    next.phase = NX_UPDATE_CONFIRMED;
    next.active = next.candidate;
    next.active_slot = next.candidate_slot;
    next.has_candidate = false;
    next.confirm_pending = false;
    next.candidate_slot = 0;
    next.attempts = 0;
    memset(&next.candidate, 0, sizeof(next.candidate));
    return save_state(u, &next);
}

nx_update_status_t nx_update_select(nx_update_t* u, nx_update_choice_t* choice) {
    nx_update_state_t next;
    nx_update_choice_t selected;
    nx_update_status_t status;
    uint64_t floor;
    if (!u || !u->initialized || !choice) return NX_UPDATE_EINVAL;
    if (!u->state.has_active) return NX_UPDATE_ENOTFOUND;
    if (u->state.confirm_pending) {
        status = nx_update_confirm(u, u->state.candidate_slot, u->state.candidate.digest);
        if (status != NX_UPDATE_OK) return status;
    }
    if ((status = counter(u, &floor)) != NX_UPDATE_OK) return status;
    if (u->state.has_candidate) {
        status = verify(u, u->state.candidate_slot, &u->state.candidate, floor);
        if (status == NX_UPDATE_OK && u->state.attempts == u->state.attempt_limit)
            status = NX_UPDATE_EXHAUSTED;
        if (status != NX_UPDATE_OK) {
            status = rollback(u, status);
            if (status != NX_UPDATE_OK) return status;
        } else {
            next = u->state;
            next.phase = NX_UPDATE_TRIAL;
            ++next.attempts;
            if ((status = save_state(u, &next)) != NX_UPDATE_OK) return status;
        }
    }
    memset(&selected, 0, sizeof(selected));
    selected.phase = u->state.phase;
    selected.rollback_reason = u->state.rollback_reason;
    if (u->state.has_candidate) {
        selected.slot = u->state.candidate_slot;
        selected.manifest = u->state.candidate;
    } else {
        if ((status = verify(u, u->state.active_slot, &u->state.active, floor)) != NX_UPDATE_OK)
            return status;
        selected.slot = u->state.active_slot;
        selected.manifest = u->state.active;
    }
    *choice = selected;
    return NX_UPDATE_OK;
}
