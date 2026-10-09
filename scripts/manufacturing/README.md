# Manufacturing station contract

`provision_device.py` runs only with a passing **enterprise** gate and revalidates the
upstream inventory, policy, physical HIL, reviews and protected-service signature before
touching equipment. No actual station, HSM or device provisioning is configured here.

```sh
python scripts/manufacturing/provision_device.py --manifest /station/job.json \
  --audit-root /station/audit --leases /station/leases --report /station/job-result.json
```

Schema 1 `manufacturing_station` jobs require `lot_id`, `station_id`, `operator_role`,
`board: {id, profile, revision, probe_serial}`, `enterprise_gate`, pinned `adapter_files`,
`operation_timeout_s`, `key_reference`, `identity_id`, `calibration_requirements`,
and argv `commands` for `identify`, `flash`, `verify_flash`, `calibrate`, `provision`,
`attest`, `reset`, `cleanup`. The key reference is an opaque HSM/service object ID;
it is not key bytes, a credential or an enrollment token.

Identification returns schema 1, observed `board` and unique chip `device_uid`. Before
flash/provision, the runner durably reserves that UID and the product `identity_id`.
Duplicate or interrupted identities remain reserved for supervised recovery. A second
attempt cannot overwrite a prior audit. The station cannot silently reuse an identity
after a partial provisioning failure.
Failed teardown also quarantines the hardware lease for supervised station recovery.

`verify_flash` returns schema 1, observed `device_uid` and readback `firmware_sha256`.
`calibrate` returns schema 1, `status: pass`, `limits_id` and actual
`measurements: {metric: {value, unit}}`; they must meet the job's reviewed
`calibration_requirements: {limits_id, measurements: {metric: {minimum, maximum, unit}}}`.
`provision` returns schema 1, `status: pass`, observed `device_uid` and `identity_id`.
`attest` returns those fields plus public `public_key_sha256` and `certificate_sha256`.
The trusted adapter/HSM must verify device possession and certificate issuance; merely
returning expected strings is not physical attestation.

Substitutions are HIL board/firmware fields plus `{device_uid}`, `{identity_id}` and
`{key_reference}`. The identify command cannot depend on an unobserved UID. No shell is
used. Keys stay in the station's protected service; credential-shaped response fields
are rejected before public audit writes. The key reference itself is hashed in audit.

Durable intent, calibration and result states retain lot/station/role/board/image/config/
source/public-identity relationships and preceding state digests. Sign/retain those
public audit states with the manufacturing service and restrict filesystem writers:
hash links alone do not make mutable files tamper-proof. Certificate chains, HSM policy,
calibration fixture qualification, electrical safety, production acceptance and secure
audit storage remain actual station integration tasks.
