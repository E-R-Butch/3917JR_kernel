# macOS workspace

This source tree must live on a case-sensitive filesystem. It contains distinct
paths whose names differ only by letter case, including:

```text
net/netfilter/xt_DSCP.c
net/netfilter/xt_dscp.c
```

A normal case-insensitive macOS volume silently collapses such pairs and creates
an incomplete checkout. Setting `core.ignorecase=false` cannot fix a filesystem
that cannot represent both names.

## Case-sensitive sparse bundle

One practical local setup is a dynamically growing APFS sparse bundle:

```sh
hdiutil create -size 16g -type SPARSEBUNDLE \
  -fs 'Case-sensitive APFS' \
  -volname 3917JR-CIP4.19 \
  3917JR-CIP4.19.case-sensitive.sparsebundle

hdiutil attach -nobrowse 3917JR-CIP4.19.case-sensitive.sparsebundle
```

Clone or work under the mounted `/Volumes/3917JR-CIP4.19` path. The 16 GiB value
is a logical ceiling; the bundle consumes physical storage as data is written.

After a reboot, attach the bundle again before opening the repository. Detach it
only after all Git and build processes have stopped:

```sh
hdiutil detach /Volumes/3917JR-CIP4.19
```

Do not copy the working tree back to a case-insensitive volume. Archives and Git
objects can be stored there, but an extracted working tree cannot.
