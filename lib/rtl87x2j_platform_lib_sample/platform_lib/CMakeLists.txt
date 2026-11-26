# Copyright (c) 2025 Realtek Semiconductor Corporation
# SPDX-License-Identifier: Apache-2.0

# CMake adapter for the library's independent Makefile build. The enclosing
# Zephyr application supplies the selected board's generated headers, compiler,
# definitions, and compile options.

zephyr_get_include_directories_for_lang_as_string(       C zephyr_includes)
zephyr_get_system_include_directories_for_lang_as_string(C zephyr_system_includes)
zephyr_get_compile_definitions_for_lang_as_string(       C zephyr_definitions)
zephyr_get_compile_options_for_lang_as_string(           C zephyr_options)

if(DEFINED CMAKE_C_COMPILER_TARGET)
  set(target_flag "--target=${CMAKE_C_COMPILER_TARGET}")
endif()

# PHY headers require GNU empty-variadic-macro behavior. Put gnu17 last so it
# takes precedence over the language mode inherited from Zephyr. Zephyr also
# exports -g/-gdwarf-*; the final -g0 prevents debug sections in this archive.
set(rtl87x2j_platform_cflags
    "${target_flag} ${zephyr_includes} ${zephyr_definitions} ${zephyr_options} ${zephyr_system_includes} -std=gnu17 -g0")

if(NOT RTK_BASE)
  set(RTK_BASE "$ENV{HOME}/bee5/realtek")
endif()
set(RTK_BASE "${RTK_BASE}" CACHE PATH
    "Root of the Realtek bee5 source tree")

if(NOT IS_DIRECTORY "${RTK_BASE}/soc" OR
   NOT IS_DIRECTORY "${RTK_BASE}/kernel" OR
   NOT IS_DIRECTORY "${RTK_BASE}/driver")
  message(FATAL_ERROR
    "RTK_BASE='${RTK_BASE}' is not a valid Realtek source tree. "
    "Set -DRTK_BASE=/path/to/realtek.")
endif()

include(ExternalProject)

set(rtl87x2j_platform_build_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(rtl87x2j_platform_lib_file
    "${rtl87x2j_platform_build_dir}/lib/librtl87x2j_platform_lib.a")

if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
  set(submake "$(MAKE)")
else()
  set(submake "make")
endif()

ExternalProject_Add(
  rtl87x2j_platform_lib_project
  PREFIX             "${rtl87x2j_platform_build_dir}"
  SOURCE_DIR         "${CMAKE_CURRENT_SOURCE_DIR}"
  BINARY_DIR         "${CMAKE_CURRENT_SOURCE_DIR}"
  CONFIGURE_COMMAND  ""
  BUILD_COMMAND
    ${submake}
    PREFIX=${rtl87x2j_platform_build_dir}
    CC=${CMAKE_C_COMPILER}
    AR=${CMAKE_AR}
    CFLAGS=${rtl87x2j_platform_cflags}
    RTK_BASE=${RTK_BASE}
  INSTALL_COMMAND    ""
  BUILD_BYPRODUCTS   "${rtl87x2j_platform_lib_file}"
  BUILD_ALWAYS       TRUE
  USES_TERMINAL_BUILD TRUE
)

add_library(rtl87x2j_platform_lib STATIC IMPORTED GLOBAL)
add_dependencies(rtl87x2j_platform_lib rtl87x2j_platform_lib_project)
set_target_properties(rtl87x2j_platform_lib PROPERTIES
  IMPORTED_LOCATION             "${rtl87x2j_platform_lib_file}"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
  INTERFACE_LINK_LIBRARIES      "m"
)
