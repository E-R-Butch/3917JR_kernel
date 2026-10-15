#!/usr/bin/env bash
set -euo pipefail

repo_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
  echo "error: run this script inside the Git repository" >&2
  exit 1
}
cd "$repo_root"

scan_dir=$(mktemp -d "${TMPDIR:-/tmp}/3917jr-release-scan.XXXXXX")
trap 'rm -rf "$scan_dir"' EXIT
failed=0

# Scan the history that a normal `git push origin main --follow-tags` would
# publish. Deliberately exclude local donor/upstream remote-tracking refs.
release_revs=(HEAD)
while IFS= read -r tag; do
  release_revs+=("$tag")
done < <(git for-each-ref --format='%(refname)' refs/tags)

git rev-list --objects "${release_revs[@]}" >"$scan_dir/objects"
awk 'NF > 1 { sub(/^[^ ]+ /, ""); print }' "$scan_dir/objects" \
  >"$scan_dir/paths"

grep -E \
  '(^|/)(Image([.].*)?|vmlinux|System[.]map|Module[.]symvers)$|[.]img$|[.]ko([.].*)?$|[.]dtbo?$' \
  "$scan_dir/paths" | grep -v -x 'scripts/Makefile.dtbo' \
  >"$scan_dir/build-artifacts" || true
if [[ -s "$scan_dir/build-artifacts" ]]; then
  echo "error: publishable history contains kernel or boot artifacts:" >&2
  sed -n '1,40p' "$scan_dir/build-artifacts" >&2
  failed=1
fi

grep -E \
  '(^|/)[.]gitmodules$|([.]key|[.]pk8|[.]p12|[.]pfx)$|(^|/)(id_rsa|id_ed25519)$|^(keys|private)/' \
  "$scan_dir/paths" >"$scan_dir/key-paths" || true
if [[ -s "$scan_dir/key-paths" ]]; then
  echo "error: publishable history contains forbidden metadata or likely private-key paths:" >&2
  sed -n '1,40p' "$scan_dir/key-paths" >&2
  failed=1
fi

if ! scripts/audit-git-history-objects.pl "${release_revs[@]}" \
  >"$scan_dir/object-audit"; then
  echo "error: publishable history contains forbidden Git objects:" >&2
  sed -n '1,40p' "$scan_dir/object-audit" >&2
  failed=1
fi

case_pair_count=$(git ls-tree -r --name-only HEAD -- \
  net/netfilter/xt_DSCP.c net/netfilter/xt_dscp.c | wc -l | tr -d ' ')
if [[ "$case_pair_count" != "2" ]]; then
  echo "error: case-sensitive source pair is not represented at HEAD" >&2
  failed=1
fi

if (( failed != 0 )); then
  exit 1
fi

echo "release-tree history check: PASS"
