idf_component_register(
    SRCS "smartwatch_app.c"
    INCLUDE_DIRS "include"
    REQUIRES smartwatch_board smartwatch_platform smartwatch_services smartwatch_storage smartwatch_ui
)
