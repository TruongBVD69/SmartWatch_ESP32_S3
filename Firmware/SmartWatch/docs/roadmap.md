# Roadmap

## Phase 1: Architecture

- Layered platform, kernel, services, UI, and apps
- WatchFace and basic navigation
- Diagnostics replacement for the old hardware-test entrypoint
- OTA-ready partition layout and flash coredump

## Phase 2: Runtime Foundation

- Service health and retry state machine
- App pause/resume and navigation history
- Settings persistence and display timeout
- Expanded diagnostics for touch, audio, SD, RTC, IMU, and PMIC

## Phase 3: Product UI

- SquareLine-generated views integrated behind `ui_manager`
- Launcher grid, settings screens, notifications, and production watch faces
- Asset packaging into the `assets` partition

## Later

- WiFi, BLE, OTA transport, media apps, and AI are separate milestones after power and UI lifecycle stabilization.
