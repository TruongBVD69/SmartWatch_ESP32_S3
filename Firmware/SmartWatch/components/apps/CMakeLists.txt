idf_component_register(
    SRCS
        "apps.c"
        "watchface_app.c"
        "launcher_app.c"
        "settings_app.c"
        "diagnostics_app.c"
    INCLUDE_DIRS "include"
    REQUIRES kernel services ui
)
