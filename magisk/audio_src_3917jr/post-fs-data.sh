#!/system/bin/sh
# The overlay in system/vendor/lib/modules replaces the stock audio DLKMs, which
# init.target.rc modprobes during early-init. Two of our modules are not in that
# list (rt1011 powers the speaker amp; wcd9xxx/mbhc/swr_ctrl are pulled in as
# dependencies of the vendor set), so load them explicitly here.

MODDIR=${0%/*}
M=$MODDIR/system/vendor/lib/modules
LOG=/data/local/tmp/audio_src_3917jr.log

exec >>"$LOG" 2>&1
echo "=== $(date) audio_src_3917jr post-fs-data"

for k in audio_snd_event audio_wcd9xxx audio_mbhc audio_swr_ctrl audio_rt1011; do
	[ -f "$M/$k.ko" ] || continue
	# module name inside the .ko keeps the CLO suffix (snd_event_dlkm, ...)
	mod="${k#audio_}_dlkm"
	if lsmod | grep -q "^$mod "; then
		echo "skip $k (already loaded)"
		continue
	fi
	if insmod "$M/$k.ko"; then
		echo "ok   $k"
	else
		echo "FAIL $k"
	fi
done
