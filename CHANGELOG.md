# Changelog

## v1.4.0

Features
- Connection logic for audio devices reworked: prefer installed services, disable→enable per service, handle ERROR_INVALID_PARAMETER with NULL fallback, skip ERROR_SERVICE_DOES_NOT_EXIST, and increase stabilization delays.
- Added AudioSource and Headset services to both connect attempts and GUI disconnect fallback list.
- GUI: prevent auto-reconnect after a manual disconnect; auto-unblock upon manual connect; introduce per-device reconnect cooldown (8s) to avoid rapid retries.

Improvements
- Reduce false disconnects by double-checking state with BluetoothGetDeviceInfo before declaring a disconnect.
- Config device matching now supports substring matches for easier targeting.

Fixes
- GUI link errors fixed by adding user32.lib to build script and source pragmas.
