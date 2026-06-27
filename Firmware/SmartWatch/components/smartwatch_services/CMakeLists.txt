idf_component_register(
    SRCS "smartwatch_services.c"
    INCLUDE_DIRS "include"
    REQUIRES smartwatch_board smartwatch_platform heap
)
