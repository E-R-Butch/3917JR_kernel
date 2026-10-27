# vintf_sysvipc_fix

Overrides `/system/etc/vintf/compatibility_matrix.4.xml` so the framework no
longer demands `CONFIG_SYSVIPC=n`.

## Why

`ActivityTaskManagerService.showSystemReadyErrorDialogsIfNeeded()` calls
`Build.isBuildConsistent()` → `VintfObject.verifyWithoutAvb()`. On this device
the check fails with exactly one mismatch:

```
CONFIG_SYSVIPC   matrix: n   kernel: y
```

The kernel sets `SYSVIPC=y` because Droidspaces needs SysV IPC inside
containers. Killing the option removes the dialog but breaks that use case, so
the requirement is dropped on the framework side instead — the kernel keeps its
config, nothing else in the matrix changes.

Log evidence without this module:

```
E Build : Vendor interface is incompatible, error=1
```

## Contents

```
system/etc/vintf/compatibility_matrix.4.xml   patched copy (SYSVIPC block removed)
tools/patch_vintf_matrix.py                   regenerates the patched copy
```

## Regenerate after a ROM change (mandatory for the Lineage port)

The patched file is a snapshot of *this* ROM's matrix. A new ROM (different
Android version, different target-level, different `compatibility_matrix.*.xml`)
will silently keep using its own unpatched file, so regenerate:

```bash
adb pull /system/etc/vintf/ /tmp/vintf
python3 tools/patch_vintf_matrix.py --all /tmp/vintf /tmp/out
# copy /tmp/out/*.xml into system/etc/vintf/ of this module, reinstall, reboot
```

The script exits 3 when it finds nothing to remove — that means the new ROM
either no longer has the requirement (module is unnecessary) or expresses it
differently (re-inspect before shipping).

## Verify after reboot

```bash
adb shell su -c 'grep -c CONFIG_SYSVIPC /system/etc/vintf/compatibility_matrix.4.xml'   # expect 0
adb shell su -c 'logcat -d -b all | grep -c "Vendor interface is incompatible"'         # expect 0
```

## Rollback

Disable or delete this module and reboot:

```bash
adb shell su -c 'touch /data/adb/modules/vintf_sysvipc_fix/disable'
```
