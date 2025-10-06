# Firmware Artifacts

This directory contains versioned firmware binaries that can be served to the gateway OTA client.

## Gateway Releases

| Device | Version (major.minor.patch) | Version Code | Binary | SHA256 |
| ------ | --------------------------- | ------------ | ------ | ------ |
| GW-001 | 0.1.1 | 257 | [`gateway/GW-001_v257.bin`](gateway/GW-001_v257.bin) | `fc1e894e27b1964cb97845744030c49791b66fd7545667d219cf5c20a7a71290` |

### Upload instructions

1. Commit the binary together with this README update:
   ```bash
   git add firmware
   git commit -m "Add GW-001 v0.1.1 firmware binary"
   git push origin main
   ```
2. Ensure the raw download URL matches the OTA setting:
   `https://raw.githubusercontent.com/enisyuksel/Farm-Tracking-System/main/firmware/gateway/GW-001_v257.bin`
3. Trigger the OTA by publishing to the MQTT config topic:
   ```json
   {
     "type": "gateway_ota",
     "version": 257
   }
   ```
