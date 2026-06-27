# Firmware Architecture

## Dependency Direction

```text
main
└── smartwatch_app
    ├── smartwatch_ui
    ├── smartwatch_services
    ├── smartwatch_storage
    ├── smartwatch_platform
    └── smartwatch_board
        └── vendor drivers and BSP
```

Dependencies point down this graph. UI and feature components must not include vendor BSP or sensor headers. Hardware ownership remains in `smartwatch_board`.

## Startup

1. Platform creates the default event loop and applies logging policy.
2. Storage initializes NVS and mounts the assets partition.
3. Board initializes display and optional hardware.
4. UI registers event handlers before device services start.
5. Power, input, RTC, and health services publish initial state and subsequent changes.
6. App publishes `SMARTWATCH_EVENT_BOOT_COMPLETED`.

Storage failure is degraded operation. Display or board startup failure is fatal and causes a delayed restart. Optional device failure is reported through service-state events without aborting the application.

## Event Policy

- Events carry copied DTO payloads and never expose driver handles.
- Producers use bounded waits and must not block indefinitely on the event loop.
- Service-state events are emitted only on transitions.
- Periodic telemetry is limited to one power snapshot and one health snapshot every 30 seconds.
- UI callbacks acquire the LVGL lock and perform no driver I/O.

## Watchdog Policy

Device service tasks subscribe to the ESP-IDF task watchdog and feed it at least once per second. The timeout is eight seconds and triggers a panic so the flash coredump captures task state. Tasks must not disable the watchdog to hide latency problems.

## Logging Policy

- Default and maximum compiled level is `INFO`.
- Normal lifecycle transitions use `INFO`.
- Recoverable degradation uses `WARN` and is logged only on state transition.
- Startup failure, data corruption, and failed controlled shutdown use `ERROR`.
- High-frequency sensor samples and button polling are never logged.
- Credentials, tokens, microphone samples, and personal data must never be logged.

## Recovery Policy

- NVS is erased only for `NO_FREE_PAGES` or incompatible on-flash versions.
- Assets are never automatically formatted; missing assets result in degraded mode.
- Device services enter degraded state after three consecutive failures and recover after the next successful operation.
- Fatal startup retries are bounded by a five-second restart delay.
