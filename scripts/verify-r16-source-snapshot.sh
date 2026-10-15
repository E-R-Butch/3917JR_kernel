#!/usr/bin/env bash
set -euo pipefail

repo_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
  echo "error: run this script inside the Git repository" >&2
  exit 1
}
cd "$repo_root"

file_manifest=manifests/R16_SOURCE_SNAPSHOT.sha256
link_manifest=manifests/R16_SOURCE_SYMLINKS.tsv
expected_file_aggregate=dd0da7908b7d6528d48648e007c983a11c3fdedf706071cb8567baa2be712ff6
expected_link_aggregate=b6a4f4137e84332000b8071597db5f1c3c6057f0b7935c4aa9e1c900dbda8265

if command -v sha256sum >/dev/null 2>&1; then
  hash_stream() { sha256sum | awk '{print $1}'; }
  sha256sum --quiet -c "$file_manifest"
else
  hash_stream() { shasum -a 256 | awk '{print $1}'; }
  shasum -a 256 --quiet -c "$file_manifest"
fi

actual_file_aggregate=$(hash_stream <"$file_manifest")
if [[ "$actual_file_aggregate" != "$expected_file_aggregate" ]]; then
  echo "error: regular-file manifest aggregate mismatch" >&2
  exit 1
fi

while IFS=$'\t' read -r path expected_target; do
  if [[ ! -L "$path" ]]; then
    echo "error: missing source symlink: $path" >&2
    exit 1
  fi
  actual_target=$(readlink "$path")
  if [[ "$actual_target" != "$expected_target" ]]; then
    echo "error: symlink target mismatch: $path" >&2
    exit 1
  fi
done <"$link_manifest"

actual_link_aggregate=$(
  while IFS=$'\t' read -r path target; do
    printf '%s\t%s\0' "$path" "$target"
  done <"$link_manifest" | hash_stream
)

if [[ "$actual_link_aggregate" != "$expected_link_aggregate" ]]; then
  echo "error: symlink manifest aggregate mismatch" >&2
  exit 1
fi

echo "R16 source snapshot verification: PASS"
