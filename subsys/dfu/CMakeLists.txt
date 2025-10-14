FILE(GLOB dfu_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

zephyr_sources_ifdef(
    CONFIG_RTK_DFU
    ${dfu_sources}
)
