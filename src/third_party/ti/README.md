# TI Third-Party Vendor Sources

## tifs_mcu_common

TI Foundational Security (TIFS) MCU common repo - provides HS-FS signing scripts
and development keys used to produce `.tiimage` boot containers.

**License:** BSD-3-Clause (see individual file headers).

**Upstream:**
- Repository: https://github.com/TexasInstruments/tifs-mcu-common
- Tag: `REL.MCUSDK.K3.11.02.00.24`
- Commit: `b237ec912feef47a2566f29dac14f5d95057d868`

**xSDK-used paths:**
```
tools/boot/signing/rom_image_gen.py          - SBL .tiimage generation
tools/boot/signing/rom_degenerateKey.pem     - HS-FS degenerate ROM key (dev only)
tools/boot/signing/appimage_x509_cert_gen.py - application image signing (Phase 8)
```

---

## mcupsdk_core

TI MCU+ SDK core source, tracked as a pinned shallow submodule.

**Why it is here:** xSDK targets TI AM243x without requiring a separately installed
TI MCU+ SDK. The required TI-origin source (SBL examples, sciclient, DPL headers,
boot signing scripts, SYSFW binaries, boardcfg) is vendored here so CI and normal
development have a reproducible, self-contained source closure.

**License:** BSD-3-Clause (see `mcupsdk_core/LICENSE` and individual file headers).

**Upstream:**
- Repository: https://github.com/TexasInstruments/mcupsdk-core
- Branch: `release/am64x_am243x_am273x/11.02.00`
- Commit: `6949c8057336cb59915ca51340cdafa6a7960126`

**xSDK-used paths** are listed in `manifest.txt`.

**Integration rule:** TI source and boot assets live under `src/third_party/ti/`.
xSDK-owned adapter code lives under `src/port/am243x/` and `src/components/`.
Do not copy TI files out of this directory into the rest of the xSDK tree.
