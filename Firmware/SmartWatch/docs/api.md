# Public API Boundaries

- Hardware: `platform_*.h`
- Framework: `app_manager.h`, `service_manager.h`, `event_bus.h`, `screen_manager.h`
- Device features: `*_service.h`
- UI: `ui_manager.h`, `ui_events.h`, `ui_theme.h`
- Apps expose only their `app_t` descriptors.

Headers from `smartwatch_board`, Waveshare components, and managed components are private to the platform layer.
