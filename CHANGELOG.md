# Changelog

## 0.1.0

Initial PSP port release.

- Rendering pipeline: surface-based path with manual INDEX8 to RGB565 conversion
- Two-step scaling: 640x480 -> 480x272
- Game boots to intros on PSP hardware and PPSSPP
- Debug logging to Memory Stick root

### Known limitations

- Input is not wired up
- Audio is linked but untested
- Save/load depends on input progress
