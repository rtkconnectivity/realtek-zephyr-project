.. zephyr:code-sample:: rtl87x2j_platform_library
   :name: RTL87X2J External Platform Library

   Build and link the RTL87X2J platform library with an external Makefile.

Overview
********

This is a runnable Zephyr application organized like
``samples/application_development/external_lib``. The application CMake build
exports the selected board's compiler flags and invokes the library's simple
Makefile through ``ExternalProject_Add``. The resulting static archive is
linked into ``app`` and the normal Zephyr ELF and binary files are produced.

The user runs only ``west build``; there is no need to invoke the library
Makefile directly.

Header layout
*************

The platform library separates its public API from headers used only while
building the archive:

.. code-block:: text

   platform_lib/
   |-- include/
   |   `-- rtl87x2j_platform_init.h
   `-- private_include/
       `-- boot_patch_define.h

``include/rtl87x2j_platform_init.h`` and ``include/rtl87x2j_patch.h`` are the
public interfaces. They are used by the library implementation and exported
through the ``rtl87x2j_platform_lib`` CMake target for consumers such as the
RTL87X2J SoC initialization code. There is one canonical copy of each; do not
duplicate them in the Zephyr SoC or HAL trees.

The patch sources mirror the Realtek ``sys_patch`` target. Zephyr must call
``rtl87x2j_sys_patch_init()`` after the image-local OCCD configuration is
available and before a patched ROM/platform API can run.

The file under ``private_include/`` is an implementation detail and is not
exported to consumers. ``boot_patch_section.h`` needs no compatibility changes,
so the build uses the vendor copy from
``${RTK_BASE}/soc/flash_proj/bootloader/inc`` directly.

boot_patch_define.h workaround
******************************

The vendor header is located at
``${RTK_BASE}/soc/flash_proj/bootloader/inc/boot_patch_define.h``. It contains
Keil/Windows-style backslash-relative includes, for example:

.. code-block:: c

   #include "..\\..\\..\\boards\\config\\memory\\rtl87x2j\\mem_config.h"

The ARM GCC build on Linux does not interpret those paths as directory
separators. Merely adding the vendor header directory with ``-I`` therefore
finds the header but then fails while processing its nested includes.

As a temporary compatibility workaround, ``private_include/boot_patch_define.h``
contains the equivalent RTL87X2J definitions and uses portable flat include
names. The Makefile intentionally orders include directories as follows:

.. code-block:: make

   -I$(CURDIR)/private_include
   -I$(CURDIR)/include
   -I$(RTK_SOC)/flash_proj/bootloader/inc

It then force-includes ``boot_patch_define.h``. This causes GCC to select the
private portable header while retaining the vendor bootloader include directory
for other headers. Once the upstream vendor header uses portable include paths,
the private workaround can be removed and the vendor header can be used
directly.

Building
********

From the west workspace root:

.. code-block:: console

   west build -p always -b rtl87x2j_evb/rtl8762jth \
     realtek-zephyr-project/lib/rtl87x2j_platform_lib_sample \
     -- -DRTK_BASE=$HOME/bee5/realtek

The firmware outputs are under ``build/zephyr/``. Flash with the runner selected
for the board, for example:

.. code-block:: console

   west flash

Expected Output
***************

.. code-block:: console

   RTL87X2J external platform library sample running
