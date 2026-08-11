SDHC
====

The SDHC driver exposes the Bee SD host controller through the standard Zephyr
SDHC API. The SDHC API is a generic interface to an SD host controller; it is
driven internally by the SD subsystem and is not meant to be called directly by
the application - card access goes through the disk-access or filesystem API,
and the SD stack drives the SDHC layer underneath.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- SD, eMMC, and SDIO card support.
- 1-bit and 4-bit bus widths.
- Bus clock up to 50 MHz, with Legacy and High-Speed timing.
- 3.3 V card signaling.
- Internal DMA (IDMAC) for block data transfers.
- Pad routing through pinctrl pin groups (``pin-group`` 0 or 1).
- Support for a card power switch via ``pwr-gpios``, with ``power-delay-ms``
  controlling the post-power-on settle time.
- SDIO card-interrupt support on the D1 line via ``int-gpios`` and a dedicated
  pinctrl interrupt state (4-bit mode).
- Automatic error recovery on a failed block read/write: the datapath is reset,
  a stop-transmission (CMD12) is issued, and the command is retried.
- Physical erase of SD cards (CMD32/CMD33/CMD38).

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``sdhc0``.
- Bindings file: ``dts/bindings/sdhc/realtek,bee-sdhc.yaml``
  (compatible ``realtek,bee-sdhc``).
- Kconfig options: ``SDHC_BEE``, ``SDHC_BEE_ISR_TRACE``
  (``drivers/sdhc/Kconfig.bee``).
- Source file: ``drivers/sdhc/sdhc_bee.c``.
- Example board file (under ``tests/drivers/disk/disk_access_bee/boards/``):
  ``rtl87x2g_evb_a_rtl8762gku.overlay`` (rtl87x2g).
- Reference:

  - `Zephyr SDHC introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/sdhc.html>`_
  - `Zephyr SDHC API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__sdhc__interface.html>`_

Operation Flow
--------------

Enabling an SDHC device
~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the ``sdhc0`` node, route its pads through
  pinctrl, and pick the bus width and pin group. The node properties are:

  - ``bus-width``: SD data-bus width, ``1`` or ``4`` (default ``4``).
  - ``pin-group`` (required): which pad group carries the SDHC signals, ``0``
    or ``1``. It must match the pads routed in the pinctrl group.
  - ``pwr-gpios`` (optional): a GPIO that switches power to the card, used to
    power-cycle and re-initialize it. ``power-delay-ms`` sets the post-toggle
    settle time (set ``0`` when the hardware allows, to cut init time).
  - ``int-gpios`` (optional): a GPIO for the SDIO card interrupt (see
    :ref:`Using an SDIO device <sdhc_sdio>` below).

  Use a strong pull-up on the SD pads; if communication is unstable, raise the
  drive current with ``bias-pull-strong`` and ``current-level = <1>`` in the
  pinctrl group.

  .. code-block:: devicetree

     &sdhc0 {
         status = "okay";
         pinctrl-0 = <&sdhc0_default>;
         pinctrl-names = "default";
         pin-group = <0>;
         bus-width = <4>;
         max-bus-freq = <50000000>;
         power-delay-ms = <0>;

         sdcard_disk0: sdcard_disk0 {
             compatible = "zephyr,sdmmc-disk";
             disk-name = "SD";
             status = "okay";
         };
     };

     &pinctrl {
         sdhc0_default: sdhc0_default {
             group1 {
                 psels = <BEE_PSEL(SDHC0_CLK_P9_4, P9_4)>,
                         <BEE_PSEL(SDHC0_CMD_P9_3, P9_3)>,
                         <BEE_PSEL(SDHC0_D0_P10_0, P10_0)>,
                         <BEE_PSEL(SDHC0_D1_P9_7, P9_7)>,
                         <BEE_PSEL(SDHC0_D2_P9_6, P9_6)>,
                         <BEE_PSEL(SDHC0_D3_P9_5, P9_5)>;
                 output-disable;
                 bias-pull-up;
                 bias-pull-strong;
                 current-level = <1>;
             };
         };
     };

  Enable SDHC support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_SDHC=y

- *How to use*: access the card through the standard disk-access or filesystem
  APIs; the SD stack runs card identification and drives the SDHC layer for you.

Using an SD or eMMC card
~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: add a ``zephyr,sdmmc-disk`` or ``zephyr,mmc-disk`` child
  node under ``sdhc0`` and set it to ``status = "okay"`` to enable the disk
  driver for that card. Enabling the disk selects ``CONFIG_SDMMC_STACK`` or
  ``CONFIG_MMC_STACK``, which runs the card-identification sequence during
  ``sd_init()``.

  You can declare **both** child nodes under ``sdhc0`` to support either card,
  giving them distinct ``disk-name`` values:

  .. code-block:: devicetree

     &sdhc0 {
         status = "okay";
         pin-group = <0>;
         bus-width = <4>;

         sdcard_disk0: sdcard_disk0 {
             compatible = "zephyr,sdmmc-disk";
             disk-name = "SD";
             status = "okay";
         };

         sdcard_disk1: sdcard_disk1 {
             compatible = "zephyr,mmc-disk";
             disk-name = "SD2";
             bus-width = <4>;
             status = "okay";
         };
     };

  Enable both stacks in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_DISK_DRIVER_SDMMC=y
     CONFIG_DISK_DRIVER_MMC=y

  .. note::
     When both disks are enabled, initialize the MMC volume (``"SD2"``) first
     and fall back to the SD volume (``"SD"``). The MMC disk pins the card type
     before ``sd_init()``, so an SD card fails the MMC probe cleanly. The SDMMC
     disk leaves the type unset and its init falls through to the MMC path, so
     the ``"SD"`` volume would also succeed on an eMMC and then drive it with
     the SD-only erase opcodes. Probing MMC first avoids that mis-binding.

- *How to use*: try each volume name and keep whichever initializes; the one
  matching the inserted card succeeds.

  .. code-block:: c

     #include <zephyr/storage/disk_access.h>

     static const char *const candidates[] = { "SD2", "SD" };
     const char *disk = NULL;

     for (int i = 0; i < ARRAY_SIZE(candidates); i++) {
         if (disk_access_init(candidates[i]) == 0) {
             disk = candidates[i];   /* matches the inserted card */
             break;
         }
     }
     /* disk == NULL means no supported card is present */

Erasing an SD card
~~~~~~~~~~~~~~~~~~~
- *How to use*: issue a physical erase of a sector range on an SD card through
  the disk-access API:

  .. code-block:: c

     disk_access_erase("SD", start_sector, num_sectors,
                       DISK_ACCESS_ERASE_PHYSICAL);

  The erase runs CMD32/CMD33/CMD38 and resets the card's flash translation
  layer over that range.

  .. note::
     A whole-card erase (CMD38) holds the card busy on DAT0 for far longer than
     the 200 ms default command timeout, after which the host reports a
     response timeout and the still-erasing card poisons later commands. Raise
     ``CONFIG_SD_CMD_TIMEOUT`` (for example to ``30000``) so a whole-card erase
     completes within a single CMD38.

  .. note::
     eMMC erase is not supported: eMMC uses group-erase opcodes (CMD35/CMD36)
     that the SD subsystem does not implement, and the MMC disk exposes no erase
     operation, so a physical erase on the eMMC volume returns ``-EINVAL``.

.. _sdhc_sdio:

Using an SDIO device
~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: SDIO cards have no disk layer, so the application enables
  ``CONFIG_SDIO_STACK=y`` manually. An optional card interrupt on the D1 line is
  configured with ``int-gpios`` plus a dedicated pinctrl interrupt state. In
  4-bit mode the D1 pad is declared both as an ``int-gpios`` GPIO and in the
  interrupt-mode pinctrl state: the driver switches D1 from the SDIO function to
  a GPIO interrupt input to detect the card interrupt, then back to the SDIO
  function before each command.
- *How to use*: enable and disable the card interrupt at runtime with the
  standard SDHC interrupt calls; the only accepted source is ``SDHC_INT_SDIO``.

Power Management (DLPS)
-----------------------
- *Behavior*: the driver has no ``PM_DEVICE`` handler.

.. note::
   On rtl87x2g, the v3.7 driver differs here: it implements a full
   ``PM_DEVICE`` DLPS handler that switches the pads to the sleep pinctrl state
   on suspend, and restores the default state and re-enables the SDHC clock on
   resume.

Samples and Logs
----------------
Sample: ``tests/drivers/disk/disk_access_bee``.

How to run
~~~~~~~~~~
- *Wiring*: insert an SD or eMMC card wired to the ``sdhc0`` pads - CLK (P9_4),
  CMD (P9_3), D0 (P10_0), D1 (P9_7), D2 (P9_6), D3 (P9_5).
- *Extra config*: none beyond the board overlay and ``prj.conf``, which enable
  disk access and raise ``CONFIG_SD_CMD_TIMEOUT`` so a whole-card erase fits in
  one command.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/disk/disk_access_bee

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-201-g89ae9adfd6b1 ***
   Running TESTSUITE disk_driver
   ===================================================================
   I: SDHC I/O: dev: sdhc@40150000, bus width 1, clock 0Hz, card power OFF, voltage 3.3V
   I: Bus width set to 1 bit
   I: SDHC I/O: dev: sdhc@40150000, bus width 1, clock 0Hz, card power ON, voltage 3.3V
   I: SDHC I/O: dev: sdhc@40150000, bus width 1, clock 400000Hz, card power ON, voltage 3.3V
   I: Bus clock set to 400 kHz
   E: cmd 1 error: RINTSTS=0x00000104 IDSTS=0x00000000
   E: SDHC send command 1 error -5
   disk "SD2" init failed (rc=-134), trying next
   I: SDHC I/O: dev: sdhc@40150000, bus width 1, clock 0Hz, card power OFF, voltage 3.3V
   I: SDHC I/O: dev: sdhc@40150000, bus width 1, clock 0Hz, card power ON, voltage 3.3V
   I: SDHC I/O: dev: sdhc@40150000, bus width 1, clock 400000Hz, card power ON, voltage 3.3V
   I: SDHC I/O: dev: sdhc@40150000, bus width 1, clock 25000000Hz, card power ON, voltage 3.3V
   I: Bus clock set to 25000 kHz
   I: SDHC I/O: dev: sdhc@40150000, bus width 1, clock 50000000Hz, card power ON, voltage 3.3V
   I: Bus clock set to 50000 kHz
   Detected disk via "SD"
   Disk: 249856 sectors x 512 bytes
   START - test_read
   Testing reads of 8 sectors
   Testing reads of 1 sectors
   Testing reads of 29 sectors
   Testing reads of 31 sectors
    PASS - test_read in 0.060 seconds
   ===================================================================
   START - test_write
   Testing writes of 8 sectors
   Testing writes of 1 sectors
   Testing writes of 29 sectors
   Testing writes of 31 sectors
    PASS - test_write in 0.221 seconds
   ===================================================================
   TESTSUITE disk_driver succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [disk_driver]: pass = 2, fail = 0, skip = 0, total = 2 duration = 0.281 seconds
    - PASS - [disk_driver.test_read] duration = 0.060 seconds
    - PASS - [disk_driver.test_write] duration = 0.221 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

The setup phase detects the inserted card and binds it to the matching volume
(``"SD"`` for an SD card, ``"SD2"`` for an eMMC), then the read and write tests
exercise several transfer sizes across the start, middle and end of the card.

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :doc:`GPIO <gpio>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr SDHC introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/sdhc.html>`_
- `Zephyr SDHC API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__sdhc__interface.html>`_
