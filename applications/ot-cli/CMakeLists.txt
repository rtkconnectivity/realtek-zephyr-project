cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(openthread_cli)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
target_link_options(app
    PUBLIC
        -Wl,--wrap,otPlatReset
        -Wl,--wrap,platformAlarmInit
        -Wl,--wrap,platformAlarmProcess
        -Wl,--wrap,otPlatAlarmMilliGetNow
        -Wl,--wrap,otPlatAlarmMilliStartAt
        -Wl,--wrap,otPlatAlarmMilliStop
        -Wl,--wrap,otPlatAlarmMicroStartAt
        -Wl,--wrap,otPlatAlarmMicroStop
)
