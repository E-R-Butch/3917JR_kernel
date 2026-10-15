# Maintenance roadmap

R16 remains the golden recovery baseline throughout this work. The roadmap
separates source preservation, stable updates, functional configuration, and
CIP adoption so a regression can be assigned to one class of change.

## Phase 0 — freeze the baseline

- Preserve the exact R16 source and configuration in Git.
- Reconstruct a pinned Clang r365631c/binutils 2.34 builder.
- Recover and version the external-module build recipes and RAM-boot packager.
- Decide the release signing-key policy.
- Complete a no-source-change rebuild and the full RAM-boot matrix.

Exit condition: a clean environment can produce a functionally equivalent R16
candidate with documented ABI and artifact identities.

## Phase 1 — prove the stable merge mechanism

- Merge only upstream `v4.19.82`.
- Resolve conflicts without enabling new features.
- Rebuild and sign all external modules.
- Complete the offline and critical on-device gates.

Exit condition: the single-sublevel update behaves exactly like R16 in all
golden functions.

## Phase 2 — incremental upstream 4.19 stable

Integrate small, reviewable sublevel ranges. Build at every range and schedule
full RAM-boot milestones at approximately:

```text
4.19.100 -> 4.19.150 -> 4.19.200 -> 4.19.250 -> 4.19.294 -> 4.19.325
```

Google's SM7250/redbull 4.19 work may be consulted as a conflict-resolution
reference around 4.19.294, but it is not a drop-in base: its Android version,
repository split, DT, KMI assumptions, and userspace contract differ from this
Android 10 device.

Exit condition: upstream 4.19.325 passes the complete matrix with no persistent
boot changes.

## Phase 3 — container feature set P1

Apply `configs/fragments/container-p1.config` as a separate commit and let
`olddefconfig` resolve dependencies. Validate the resulting `.config`, not only
the input fragment.

The intended first set is:

- complete nftables inet/IPv4/IPv6 NAT primitives;
- CFS bandwidth control;
- block-I/O throttling;
- ext4 POSIX ACL support for an ext4 loop-backed Docker data path.

Do not combine pidfd/clone3 backports, a forced cgroup-v2 conversion, OverlayFS
fscrypt semantic changes, rootless user namespaces, or a modern BTF/CO-RE stack
with P1.

## Phase 4 — CIP checkpoints

After 4.19.325 is stable, evaluate the locally audited CIP sequence in stages:

```text
4.19.325-cip116 -> cip120 -> cip125 -> cip130 -> cip135
```

These are checkpoints, not a claim that `cip135` is the newest available tag.
Refresh the official CIP tag list and security scope immediately before each
merge. Keep each CIP range bisectable and apply the same module and RAM-boot
gates as an upstream stable update.

## Later, evidence-driven features

Only add these when a real workload requires them and a dedicated test exists:

- WireGuard compat module or in-tree backport;
- pidfd/clone3 and related UAPI backports;
- additional cgroup delegation work;
- rootless containers;
- CRIU or expanded container networking.
