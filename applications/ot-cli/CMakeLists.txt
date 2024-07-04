cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(openthread_cli)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
target_link_options(app
    PUBLIC
        -Wl,--wrap,mac_EnterCritical_rom
        -Wl,--wrap,mac_ExitCritical_rom
        -Wl,--wrap,mac_RstRF_rom
        -Wl,--wrap,otPlatReset
)
