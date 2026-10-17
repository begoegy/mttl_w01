# MTTL-W01 2.0.12 — Firmware source

[한국어](README.md) | English

Project-authored code uses the [MIT License](../LICENSE). [Third-party code and license scope](../LICENSING.md)

[Builder usage guide](../builder/README_EN.md)

Extract the source ZIP, select the directory containing `mttl-source.json` in **MTTL-Builder**, and build. The builder compiles a separate working copy rather than your original directory, preserving your source edits.

- Layout and colors: `standalone/ui/index.html`
- UI behavior: `standalone/ui/live.js`, `scheduler.js`
- Translations: `standalone/ui/i18n.js`
- Device control: C/C++ files in `standalone/firmware/`
- Build entry point: `build.py`; compilation list: `analysis/build_minimal_network.py`

No personal flash dump, device Wi-Fi/MQTT settings, stock FWR, SDK or compiler is included. `boot_identity.json` contains only the boot-code size and SHA-256 used to check OTA compatibility, not device-specific data or the boot code itself.

This is the source snapshot for Builder 1.0.0. Third-party licenses apply separately to the SDK and some code. Do not remove existing copyright or license comments. Some SDK integration code originates upstream. Source publication does not grant a blanket license over all code.

The builder downloads the specified SDK and tools from upstream distribution sites. Initial preparation requires internet access; offline builds are available afterward. There is no automatic upload: upload the FWR manually through the device web interface.

To remove dependence on a personal dump, this source package does not include the comparison test that executes the original firmware decoder. It does perform FWR length, checksum, hardware-field and round-trip checks, along with ARM initialization-code emulation checks. A successful build does not guarantee safe or correct device operation. Memory headroom is limited, so adding code may exceed build limits.

Source can execute arbitrary commands; only build sources you trust.

The developer accepts no responsibility for problems or consequences arising from user modifications or self-built firmware. Questions and troubleshooting requests related to user modifications or builds are not supported. Please do not open issues for these topics.
