zephyr_include_directories(include)

if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

target_sources_ifdef(CONFIG_ZMK_DEFAULT_LAYER app PRIVATE
    src/default_layer.c
    src/behaviors/behavior_default_layer.c
)

if(CONFIG_ZMK_DEFAULT_LAYER_STUDIO_RPC)
    target_sources(app PRIVATE src/studio/default_layer_handler.c)

    list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
    include(nanopb)
    set(NANOPB_GENERATE_CPP_APPEND_PATH TRUE)
    set(NANOPB_GENERATE_CPP_STANDALONE OFF)

    # NOTE: adding to app target directly causes build issues, so we create a library instead
    zephyr_library()
    file(GLOB_RECURSE PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)

    nanopb_generate_cpp(proto_srcs proto_hdrs RELPATH ${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_FILES})
    target_include_directories(${ZEPHYR_CURRENT_LIBRARY} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
    target_sources(${ZEPHYR_CURRENT_LIBRARY} PRIVATE ${proto_srcs} ${proto_hdrs})

    # app should depend on the generated proto files
    target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/proto)
    add_dependencies(app ${ZEPHYR_CURRENT_LIBRARY})
endif()

endif()
