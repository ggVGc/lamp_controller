idf_component_register(SRCS "lamp_controller.c" "switch_driver.c" "zcl_utility.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver nvs_flash
)
