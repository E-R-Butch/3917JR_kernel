# Public release policy

## Allowed in Git

- source code and original license files;
- reviewed patches and configuration fragments;
- build recipes that do not embed proprietary inputs or credentials;
- public certificates and checksums;
- documentation and redacted test logs.

## Never commit

- stock or modified boot/vendor/system images;
- `Image`, `vmlinux`, `System.map`, `Module.symvers`, DTB/DTBO, or `.ko` output;
- proprietary firmware, extracted vendor ramdisks, or device partitions;
- compiler/toolchain archives;
- module/AVB private keys, PKCS#12/PKCS#8 files, tokens, passwords, or device
  credentials;
- crash logs containing personal data, serial numbers, Wi-Fi credentials, or
  account identifiers.

Reference artifacts stay outside Git. Record their hashes in `manifests/` and
publish binaries only in a deliberately reviewed GitHub Release if their
license and redistribution status permit it.

## Candidate naming

Use names that distinguish an import, test candidate, and validated release.
Examples:

```text
r16-source-import-20260817
v4.19.82-gaea-test1
v4.19.325-cip116-gaea-rc1
```

Do not label the current source import as bit-identical, reproducible, stable,
or flashable.

## Required release evidence

- source commit and configuration SHA-256;
- builder/toolchain identity;
- kernel, boot package, and complete module-set hashes;
- ABI/vermagic/module-closure report;
- completed `docs/TEST_MATRIX.md` with log locations;
- explicit `RAM-BOOT ONLY / NEVER FLASH` warning for experimental candidates;
- known limitations and a tested recovery path.
