# Architecture

## Layers

```text
main/app_main.c
        |
        v
apps -> ui -> services -> kernel -> platform -> smartwatch_board -> vendor drivers
```

- `platform`: the only hardware-facing layer. It exposes stable DTOs and hides BSP and sensor handles.
- `kernel`: app/service registries, event bus, screen/input/storage managers, and logging policy.
- `services`: battery, time, sensor, audio, and SD lifecycle APIs.
- `ui`: LVGL ownership, theme, and navigation bridge.
- `apps`: WatchFace, Launcher, Settings, and Diagnostics.
- `third_party`: unmodified local Waveshare and FT3168 components.

Nested components are listed explicitly through `EXTRA_COMPONENT_DIRS`; ESP-IDF does not scan them recursively.

## Dependency Rules

1. Apps never include BSP, PMIC, RTC, IMU, audio codec, or touch driver headers.
2. UI does not perform hardware I/O when a service exists for that capability.
3. Services access hardware only through `platform_*` APIs.
4. Vendor changes are isolated under `platform/smartwatch_board` or `third_party`.
5. Event payloads are copied DTOs and never contain driver handles.

## SquareLine

SquareLine exports go only in `components/ui/squareline`. Generated files are never edited manually. Navigation and business logic remain in `ui_events` and apps.

`ui_squareline.*` is the adapter boundary. Generated widgets call into `ui_squareline_bind_*()` helpers, and application state continues to flow through `ui_events`, services, and apps rather than directly into generated code.

## Startup

`platform_init()` -> `kernel_init()` -> `services_init()` -> `ui_init()` -> `apps_init()` -> `app_manager_start(APP_ID_WATCHFACE)`.

Fatal bootstrap errors cause a delayed restart. Missing assets and optional peripherals must degrade without corrupting persistent data.
