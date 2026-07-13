# Source and provenance

## Integrated sources

| Component | Source | Revision / note |
|---|---|---|
| Yulong 3917JR outer kernel | https://github.com/Yulong-RD/3917JR_kernel | `d85e94e81b392193f9bf242e63eba8699fcc5195` |
| SM7250 camera / display / video recovery | https://github.com/slikie/android_kernel_xiaomi_sm7250 | `be6934b9864e02e1c77c54232a19e0b46ec00948`, branch `restart` |
| DroidSpaces requirements and non-GKI fixes | https://github.com/ravindu644/Droidspaces-OSS | Files retained under `reconstruction/patches/droidspaces` |
| Stock configuration | User-owned 3917JR `boot_a` | Text config only; no boot image or proprietary payload is published |

The source-recovery patches under `reconstruction/patches/source-recovery` document the exact glue needed to combine the Yulong outer tree with the public SM7250 techpacks. The same changes are already applied in this branch.

The incomplete Yulong tree recorded these unresolved gitlink object IDs:

- camera: `54a6817f68536895a7bd5528675e8b3604ccd5de`
- display: `408966151f323c226508632a9942ce44bbb4054f`
- video: `81d60391f7818ca4e9e062e666ac6fb81a678550`

## Evidence-only inputs

The connected stock device and a public TWRP device tree were used to identify the model, kernel version, DTB / DTBO behavior, symbols and configuration. Prebuilt kernels, DTBs, ramdisks, firmware, partition dumps and signing material are deliberately excluded.

## Licensing

Kernel and restored driver files retain their original copyright and license headers. This repository does not relicense third-party code. Review the top-level kernel `COPYING` file and the headers in each restored component before redistribution.

The reconstruction notes distinguish original Yulong files, public substitute sources and locally generated configuration. “Builds together” does not mean the substitute source is byte-identical to Yulong's unavailable private implementation.
