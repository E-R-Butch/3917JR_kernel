# External module sources

R16 uses 37 replacement modules: 32 built from the Qualcomm external trees
listed below and 5 core modules built from this kernel tree. The stock vendor
directory contained 38 modules; `br_netfilter.ko` is the one not replaced as a
file because R16 builds that feature into the kernel. Binary modules are
deliberately not committed.

The preserved build records pin these CodeLinaro repositories:

| Component | Repository | Recorded commit |
| --- | --- | --- |
| audio-kernel | `https://git.codelinaro.org/clo/la/platform/vendor/opensource/audio-kernel.git` | `161b63ab54be3672a55261319a82f023d34a169b` |
| data-kernel | `https://git.codelinaro.org/clo/la/platform/vendor/qcom-opensource/data-kernel.git` | `1e74b8717c5b6aee0f4acb182c84d25fe881522d` |
| qcacld-3.0 | `https://git.codelinaro.org/clo/la/platform/vendor/qcom-opensource/wlan/qcacld-3.0.git` | `3ba1dc2a474b86306d3ca261dd2f979834f6cfee` |
| fw-api | `https://git.codelinaro.org/clo/la/platform/vendor/qcom-opensource/wlan/fw-api.git` | `97d27fc41d212e9d9010b99c397b0bec6094ae7c` |
| qca-wifi-host-cmn | `https://git.codelinaro.org/clo/la/platform/vendor/qcom-opensource/wlan/qca-wifi-host-cmn.git` | `d420129248fe42cd5210ea9f7ce18090f331db44` |

The recorded Qualcomm release line is `LA.UM.8.13.r1-11100-SAIPAN.0`. One
historical qca-wifi-host-cmn tag mapping was malformed; the commit above is the
build-verified companion revision and should be preferred over guessing a tag.

Before adding submodules or a fetch script, verify that each remote still
serves the pinned object and preserve the applicable license. The external
build glue and the exact R16 patch state are still being reconstructed, so the
commit pins alone are not a complete module reproduction recipe.
