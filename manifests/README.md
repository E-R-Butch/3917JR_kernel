# Source snapshot manifests

`R16_SOURCE_SNAPSHOT.sha256` lists every regular file in the preserved R16
source snapshot before repository documentation was added. Its SHA-256 is the
documented file-content/path aggregate:

```text
dd0da7908b7d6528d48648e007c983a11c3fdedf706071cb8567baa2be712ff6
```

The manifest is sorted bytewise by relative path. Each line uses the standard
`sha256sum` format: the content SHA-256, two spaces, then a path relative to the
repository root. Hashing the manifest bytes therefore replays the published
aggregate.

`R16_SOURCE_SYMLINKS.tsv` contains the 38 source-snapshot symlinks as sorted
`path<TAB>target` records. The published symlink aggregate is calculated by
replacing each record's newline with NUL, retaining the tab separator, and
hashing the resulting byte stream:

```text
b6a4f4137e84332000b8071597db5f1c3c6057f0b7935c4aa9e1c900dbda8265
```

Run `scripts/verify-r16-source-snapshot.sh` from the repository root to check
both manifests, every listed file, and every symlink target.
