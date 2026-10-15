# Provenance and reproducibility status

## Source snapshot

The imported kernel tree is the final preserved R16 source snapshot for the
3917JR OEM-port work. It was exported on 2026-08-17 from the retained build
volume and verified before repository metadata was added.

| Property | Verified value |
| --- | --- |
| Regular files | 60,735 |
| Symbolic links | 38 |
| Directories | 4,080 |
| File content + relative path aggregate SHA-256 | `dd0da7908b7d6528d48648e007c983a11c3fdedf706071cb8567baa2be712ff6` |
| Symlink path + target aggregate SHA-256 | `b6a4f4137e84332000b8071597db5f1c3c6057f0b7935c4aa9e1c900dbda8265` |

The host workspace uses a case-sensitive filesystem. This is required because
the Linux tree contains paths such as `net/netfilter/xt_DSCP.c` and
`net/netfilter/xt_dscp.c` simultaneously.

The snapshot has no original `.git` directory. It combines the device baseline
with the hardware fixes that produced R16. Git history before this import must
therefore be reconstructed from the recorded upstream/donor commits rather than
invented retroactively.

## Recorded source repositories and Git anchor

The Qualcomm/SM7250 base was recorded as
[`slikie/android_kernel_xiaomi_sm7250`](https://github.com/slikie/android_kernel_xiaomi_sm7250)
commit `be6934b9864e02e1c77c54232a19e0b46ec00948`, whose tree is
`e27d670bcf256788387af4c3f6daeb3403aaa28f`.

The public repository uses a sanitized, parentless synthetic anchor derived
from that tree, followed by the R16 snapshot import. Its tree is
`5ae97e5cae76c4a7cf1d5b0ab1ce5fec3646203b`. The public-history anchor omits
`AnyKernel3/` (including three prebuilt ELF tools), `.gitmodules`, and the
`scripts/ufdt/libufdt` gitlink. All three paths are absent from R16 itself.

The sanitized commit is not represented as the original vendor commit,
history, or byte-identical tree. It makes the public history independent of
missing vendor ancestors and submodules while keeping the source delta
reviewable and the no-prebuilt-binaries policy enforceable. The original commit
and tree IDs above remain the provenance evidence.

Recorded hardware donors are:

- Pixelworks Iris3/display reference:
  [`Mashopy/android_kernel_tcl_sm6150`](https://github.com/Mashopy/android_kernel_tcl_sm6150),
  commit `7162ba4a4a5adb1cee32ee18c55714f27784134c`;
- 3917JR FocalTech FT3518/device reference:
  [`Yulong-RD/3917JR_kernel`](https://github.com/Yulong-RD/3917JR_kernel),
  commit `d85e94e81b392193f9bf242e63eba8699fcc5195`.

These commit IDs identify source inputs and review references. They do not imply
that every file in the final R16 snapshot is an unmodified copy of a donor.

## Golden R16 identities

The following artifacts are reference evidence and are not committed:

| Artifact | SHA-256 |
| --- | --- |
| R16 `Image` | `1162c3bf197b5395d4488923336cd28b0ed49a6874d2ffd8cf6d70d263d9f58a` |
| R16 RAM-boot image | `7a1e1b93e842824f7df940adaf785fa0ded229f4aa8cee2a22916acd86cae07a` |
| R16 `.config` | `c68f0e88ee3003368e75847b3a47036abe827ce473a49a8b8a33293b2423998a` |
| R16 `Module.symvers` | `c1a157cf0859f9cb62f74794fd856f704029703c4b75bacc70fa900893411fb8` |
| R16 `vmlinux` | `8badf064b18b8ef7c92a798d1f1b0abac02fa2dc02271b0497136fa19d10bf68` |
| R16 `System.map` | `be48de0285118e6fd74dcba8c6fb01d6f8cd9227bfd4b75d52c808d0098db511` |

The RAM-boot manifest records 37 replacement modules, vermagic
`4.19.81-perf+ SMP preempt mod_unload modversions aarch64`, and final module
manifest digest
`e969f6145af17e404797531ef591337b1613edf763ea066e650f30d677f09d3b`.

For stock-input auditing, the corrected component identities are:

| Artifact | SHA-256 |
| --- | --- |
| Stock `boot_a.img` | `777ad426cf17f35b7982c1cc4d6250ccd84eb97407128497a3b76920e4035186` |
| Stock kernel component | `d4568a6d97526542afdd63951c00e38811d9fe3a75e2b027fc0767003b72e6bf` |
| Stock ramdisk component | `95819b1eaf65293fc8ff0d94a822e2dbd8635534963acde568905fde5efbe396` |

The stock image and its components are proprietary inputs and must not be
redistributed by this repository.

## Recorded toolchain

R16 identifies Android Clang `r365631c` (Clang 9.0.8) and GNU binutils 2.34,
including `aarch64-linux-gnu-ld`. The exact R16 environment has not yet been
recovered and validated as a versioned reproducible builder. Retained build
artifacts or a stopped container-data volume do not by themselves establish
that the original image is complete. A clean-room builder still needs to pin
the toolchain, packages, environment, build user/host, and timestamps.

## Reproducibility boundary

The source import and golden configuration are preserved. The project must not
claim bit-identical or release-grade reproducibility until all of these are
complete:

- reconstruct and version the final RAM-boot packager;
- reconstruct the external module build and signing pipeline;
- recover the exact 37-module set from the validated R16 boot and verify its
  manifest;
- establish a release signing key policy without publishing any private key;
- rebuild in a clean pinned environment and compare ABI, module CRCs, boot
  layout, and runtime behavior.

If the original private module-signing key cannot be recovered, a new community
release key is required and the kernel plus all 37 modules must be rebuilt and
signed together. In that case functional reproduction remains possible, but a
bit-identical R16 reproduction does not.
