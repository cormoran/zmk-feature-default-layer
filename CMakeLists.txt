if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    target_sources_ifdef(CONFIG_ZMK_DEFAULT_LAYER app PRIVATE src/behaviors/behavior_default_layer.c)
endif()
zephyr_include_directories(include)
