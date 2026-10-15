# Test matrix

Every candidate starts with offline inspection. Only candidates that pass may
be temporarily RAM-booted. No experimental image is written to a persistent
partition.

## Gate A — source and build

- `scripts/check-release-tree.sh` passes.
- Pinned builder identity and complete build command are recorded.
- Final `.config` diff is intentional and reviewed.
- `Image`, `vmlinux`, `System.map`, `Module.symvers`, and build log are archived
  outside Git with SHA-256 identities.
- All 37 required external modules are rebuilt against the candidate tree,
  signed by the matching release key, and pass vermagic/modversion/import
  closure checks.
- RAM-boot package layout and AVB metadata are compared with the golden format.

## Gate B — critical boot path

Use only `fastboot boot <candidate.img>`.

- UFS is detected without timeout or reset storms.
- Android data decrypts and the normal system reaches the lock screen.
- ADB becomes available and reports the expected candidate kernel identity.
- Required modules load; no unknown-symbol, signature, or CRC failure appears.
- `pstore`, `dmesg`, and Android logs contain no panic, Oops, BUG, watchdog, or
  repeated subsystem crash.

Any failure here blocks all later testing and returns the branch to the last
known-good tag.

## Gate C — golden phone functions

| Area | Required check |
| --- | --- |
| Display | cold/warm screen-on, brightness changes, no flicker, no sharpening regression |
| Touch | lock screen, navigation, typing, suspend/resume |
| Wi-Fi | associate, DHCP, DNS, sustained transfer, reconnect |
| Cellular | modem registration and data path; record voice/SMS status if tested |
| Camera | front and rear preview plus capture; no camera-service crash |
| Audio | speaker, microphone, and route changes |
| Storage/data | repeated reboot/decrypt plus read/write/hash check |
| Power/thermal | suspend/resume, charging, idle drain, temperature sanity |

## Gate D — DroidSpaces and containers

- Run the complete DroidSpaces capability check, including TUN/TAP.
- Confirm seccomp filtering and expected namespace/cgroup behavior.
- Start Docker first with the known-compatible iptables legacy baseline, then
  explicitly test the nft backend for a P1 candidate.
- Pull an Alpine image over HTTPS.
- Run Nginx with host port mapping and receive HTTP 200.
- Write, sync, and hash at least 256 MiB in a container volume.
- For ext4-loop work, `docker info` must report `overlay2`; restart both the
  container and daemon and repeat the hash check.
- Exercise pids, memory, CPU, and I/O limits and verify their effects rather
  than only checking that control files exist.

Useful P1 probes:

```sh
nft add table inet ds_test
iptables-nft -t nat -N DS_TEST
iptables-nft -t nat -X DS_TEST

docker info --format '{{.Driver}}'
docker run --rm --memory=64m --cpus=.5 --pids-limit=32 alpine true

ip tuntap add dev ds-tun mode tun
ip link set ds-tun up
ip tuntap del dev ds-tun mode tun
```

## Gate E — soak and promotion

- Reboot the candidate at least three times through RAM boot.
- Complete a 24-hour idle/workload soak with panic/Oops and subsystem error
  counts recorded.
- Preserve test logs and artifact hashes with the candidate tag.
- Keep the prior golden RAM-boot image available for immediate recovery.

Passing compilation or reaching the launcher is never sufficient for release.
