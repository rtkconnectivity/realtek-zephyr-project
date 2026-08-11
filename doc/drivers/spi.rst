SPI
===

The SPI driver exposes the Bee SPI controllers through the standard Zephyr SPI
API.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Controller (master) and peripheral (slave) operation from one driver. Slave
  mode is selected by the ``is-slave`` devicetree property.
- Multiple independent SPI controllers per SoC (see the *Device nodes* list
  under Basic Information for the per-SoC node set).
- Interrupt-driven transfers (``CONFIG_SPI_BEE_INTERRUPT``; off by default,
  enabled automatically when ``CONFIG_SPI_ASYNC`` or ``CONFIG_SPI_BEE_DMA``
  is set) and DMA TX/RX support (``CONFIG_SPI_BEE_DMA``).
- All four clock polarity / phase combinations (CPOL/CPHA) are supported.
- Chip select either in hardware (the controller's ``SS_N`` line routed through
  pinctrl) or in software as a GPIO via ``cs-gpios``.
- Synchronous and asynchronous transfers, plus explicit bus release. The driver
  does **not** implement the RTIO submission API.

.. note::

   The maximum SPI word (frame) size differs by SoC. On **rtl87x2g** and
   **rtl8752h** a frame may be up to **32 bits**; on **rtl87x2j** the maximum
   frame size is **16 bits**. Size ``spi_config.operation`` word length within
   the limit of the SoC you build for.

.. note::

   The ``spi0`` (master) and ``spi0_slave`` (slave) nodes are mutually exclusive
   — only one may be enabled at a time: use ``spi0`` for master operation or
   ``spi0_slave`` for slave operation, not both. Enabling both is rejected at
   build time by a ``BUILD_ASSERT``.

Basic Information
~~~~~~~~~~~~~~~~~
- Device nodes:

  - rtl87x2g: ``spi0`` (or ``spi0_slave``), ``spi1``.
  - rtl8752h: ``spi0`` (or ``spi0_slave``), ``spi1``.
  - rtl87x2j: ``spi0`` (or ``spi0_slave``), ``spi1``, ``spi2``.
- Bindings file: ``dts/bindings/spi/realtek,bee-spi.yaml``
  (compatible ``realtek,bee-spi``).
- Kconfig options: ``SPI_BEE``, ``SPI_BEE_INTERRUPT``, ``SPI_BEE_DMA``,
  ``SPI_BEE_DMA_BUF_SIZE`` (default 512), ``SPI_BEE_DMA_BUF_COUNT`` (default 4)
  (``drivers/spi/Kconfig.bee``).
- Source file: ``drivers/spi/spi_bee.c``.
- Example board files (per-SoC overlays, including the DMA channels):

  - ``tests/drivers/spi/spi_loopback/boards/`` (master interrupt / DMA).
  - ``tests/drivers/spi/spi_controller_peripheral/boards/`` (master / slave
    interrupt / DMA).

- Reference:

  - `Zephyr SPI introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/spi.html>`_
  - `Zephyr SPI API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__spi__interface.html>`_

Operation Flow
--------------

Enabling an SPI device
~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: for master mode, enable an ``spi`` controller node, route
  its pads through pinctrl, and add a child node for the device (its ``reg`` is
  the CS index). The CS line can be driven either in hardware via pinctrl (route
  the controller's ``SS_N`` signal, as shown below) or in software as a GPIO via
  ``cs-gpios`` (see :ref:`Multiple chip selects <spi_cs_gpios>`). See
  :doc:`Pinctrl <pinctrl>` for the pad-configuration syntax.

  .. code-block:: devicetree

     &spi1 {
         status = "okay";
         pinctrl-0 = <&spi1_default>;
         pinctrl-names = "default";

         dut_spi_dt: test-spi-dev@0 {
             compatible = "vnd,spi-device";
             reg = <0>;
             spi-max-frequency = <1000000>;
         };
     };

     &pinctrl {
         spi1_default: spi1_default {
             group1 {
                 psels = <BEE_PSEL(SPI1_CLK_MASTER, P4_0)>,
                         <BEE_PSEL(SPI1_MO_MASTER, P4_1)>,
                         <BEE_PSEL(SPI1_MI_MASTER, P4_2)>,
                         <BEE_PSEL(SPI1_SS_N_0_MASTER, P4_3)>;
                 output-enable;
                 output-high;
                 bias-pull-up;
             };
         };
     };

  Enable SPI support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_SPI=y

- *How to use*: build a ``struct spi_config`` (frequency, mode, and the CS from
  ``SPI_CS_CONTROL_INIT()`` / the child node) and call ``spi_transceive()`` with
  TX/RX ``spi_buf_set`` descriptors. Use ``spi_transceive_async()`` for the
  non-blocking path and ``spi_release()`` to drop a held CS.
- *Example*: the ``&spi1`` node in
  ``tests/drivers/spi/spi_controller_peripheral/boards/``.

Slave mode
~~~~~~~~~~
- *How to configure*: enable the ``spi0_slave`` node and route its pads through
  pinctrl (``spi0_slave`` and ``spi0`` cannot both be enabled at the same time).
  The boolean ``is-slave`` property is what puts the controller in peripheral
  (slave) mode; it is already set on the ``spi0_slave`` node in the SoC
  devicetree, so you normally just set ``status = "okay"`` and do not add it
  yourself. On the application side, set ``SPI_OP_MODE_SLAVE`` in the
  ``spi_config`` operation flags.

  .. code-block:: devicetree

     &spi0_slave {
         status = "okay";
         pinctrl-0 = <&spi0_slave_default>;
         pinctrl-names = "default";
     };

     &pinctrl {
         spi0_slave_default: spi0_slave_default {
             group1 {
                 psels = <BEE_PSEL(SPI0_CLK_SLAVE, P0_4)>,
                         <BEE_PSEL(SPI0_SI_SLAVE, P0_5)>,
                         <BEE_PSEL(SPI0_SO_SLAVE, P0_6)>,
                         <BEE_PSEL(SPI0_SS_N_0_SLAVE, P0_7)>;
                 output-enable;
                 output-high;
                 bias-pull-up;
             };
         };
     };

  Enable SPI and slave support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_SPI=y
     CONFIG_SPI_SLAVE=y

- *How to use*: queue a transfer with ``spi_transceive()`` (or the async
  ``spi_transceive_async()``); the controller clocks the data on the master's
  clock.
- *Example*: the controller/peripheral pairing exercised by the
  ``spi_controller_peripheral`` suite.

DMA transfers
~~~~~~~~~~~~~
- *How to configure*: add **both** a TX and an RX DMA channel to the SPI node
  with the standard ``dmas`` / ``dma-names`` properties — a DMA-enabled SPI node
  must have both — and enable ``dma0``. Each ``dmas`` entry is
  ``<&dma0 <channel> <handshake-slot> <config>>``, with the ``<config>`` cell
  composed from the self-documenting ``BEE_DMA_*`` macros and the
  ``<handshake-slot>`` given by the per-SoC ``BEE_DMA_HANDSHAKE_SPI*`` macro
  instead of a raw hex value (rtl87x2g example — real values from the test
  overlay):

  .. code-block:: devicetree

     #include <dt-bindings/dma/rtl87x2g-dma.h>

     &spi0 {
         status = "okay";
         pinctrl-0 = <&spi0_default>;
         pinctrl-names = "default";
         dmas = <&dma0 5 BEE_DMA_HANDSHAKE_SPI0_TX
                 (BEE_DMA_M2P | BEE_DMA_SRC_INC | BEE_DMA_DST_FIXED | BEE_DMA_SRC_WIDTH_8BIT |
                  BEE_DMA_DST_WIDTH_8BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_1) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_1) | BEE_DMA_PRIORITY(1))>,
                <&dma0 6 BEE_DMA_HANDSHAKE_SPI0_RX
                 (BEE_DMA_P2M | BEE_DMA_SRC_FIXED | BEE_DMA_DST_INC | BEE_DMA_SRC_WIDTH_8BIT |
                  BEE_DMA_DST_WIDTH_8BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_1) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_1) | BEE_DMA_PRIORITY(0))>;
         dma-names = "tx", "rx";
     };

     &dma0 {
         status = "okay";
     };

  The ``tx`` entry is memory-to-peripheral (``BEE_DMA_M2P``) with the source
  address incrementing and the destination fixed; the ``rx`` entry is the
  reverse (``BEE_DMA_P2M``, source fixed, destination incrementing). Both use
  8-bit source and destination widths and a burst size (msize) of 1. Include
  the per-SoC header for your SoC (``rtl87x2g-dma.h``, ``rtl8752h-dma.h``, or
  ``rtl87x2j-dma.h``) so the ``BEE_DMA_HANDSHAKE_SPI0_TX`` / ``_RX`` slot macros
  resolve to that SoC's own IDs. For the full ``<config>`` bit layout and the
  complete ``BEE_DMA_*`` macro list, see the
  :ref:`Memory-to-peripheral/peripheral-to-memory transfer <dma_p2m_m2p>`
  section of the :doc:`DMA <dma>` chapter.

  Enable SPI DMA support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_SPI=y
     CONFIG_SPI_BEE_DMA=y

  The DMA scratch pool is sized by ``CONFIG_SPI_BEE_DMA_BUF_SIZE`` (default 512)
  and ``CONFIG_SPI_BEE_DMA_BUF_COUNT`` (default 4).

- *How to use*: no API change — once both channels are configured the driver
  routes ``spi_transceive()`` through DMA automatically.
- *Example*: the ``dmas`` / ``dma-names`` properties in the ``spi_loopback`` and
  ``spi_controller_peripheral`` board overlays.

.. _spi_cs_gpios:

Multiple chip selects
~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: instead of the hardware ``SS_N`` line, list one or more
  GPIOs in ``cs-gpios`` on the controller node; each child device's ``reg``
  selects which entry it uses.

  .. code-block:: devicetree

     &spi1 {
         cs-gpios = <&gpiob 8 GPIO_ACTIVE_LOW>, <&gpiob 9 GPIO_ACTIVE_LOW>;

         dev0: spi-dev@0 { reg = <0>; };   /* uses cs-gpios[0] */
         dev1: spi-dev@1 { reg = <1>; };   /* uses cs-gpios[1] */
     };

- *How to use*: address each device through its own ``spi_dt_spec`` / child node;
  the driver asserts the matching CS for each transfer.
- *Example*: the ``cs-gpios`` property in the ``spi_loopback`` /
  ``spi_controller_peripheral`` rtl87x2g board overlay.

Power Management (DLPS)
-----------------------
- *Behavior*: on a SoC with a PCK600 (such as rtl87x2j), the PCK600 manages
  power in hardware: the SPI master/slave does not sleep while a transceive is
  in progress and sleeps automatically once it completes. In particular, while a
  slave transfer is ongoing the system cannot enter sleep — it waits for the
  current transfer to finish before sleeping. DLPS is therefore transparent to
  the application — no application-side handling is required.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so the SPI master/slave does not prevent the system from entering
   sleep during a transceive. If the system enters DLPS mid-transfer the SPI
   loses power and the transfer stops, so the application must keep the system
   out of DLPS (for example with a DLPS check flag) while a transfer is in
   progress.

Samples and Logs
----------------
Samples: ``tests/drivers/spi/spi_loopback`` (master interrupt / DMA) and
``tests/drivers/spi/spi_controller_peripheral`` (master / slave interrupt /
DMA). Each provides per-SoC board overlays that also wire the DMA channels.

How to run (spi_loopback)
~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *Wiring*: short the controller's (``spi0``) MO (MOSI) and MI (MISO) pads
  together so it loops back to itself. The actual pads are set by the board
  overlay and differ per SoC.
- *Extra config*: none — the board overlay enables the controller and ``dma0``,
  and the board ``.conf`` enables ``CONFIG_SPI_BEE_DMA``.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/spi/spi_loopback

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/spi/spi_loopback

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/spi/spi_loopback

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-174-g8855e3ecc827 ***
   SPI test on buffers TX/RX 0x2000b9c0/0x2000b9a0, frame size = 8, DMA enabled (without CONFIG_NOCACHE_MEMORY)
   Polling...Running TESTSUITE spi_extra_api_features
   ===================================================================
   START - test_spi_hold_on_cs
    PASS - test_spi_hold_on_cs in 0.004 seconds
   ===================================================================
   START - test_spi_lock_release
    PASS - test_spi_lock_release in 0.002 seconds
   ===================================================================
   TESTSUITE spi_extra_api_features succeeded
   Running TESTSUITE spi_extra_api_features
   ===================================================================
   START - test_spi_hold_on_cs
    PASS - test_spi_hold_on_cs in 0.004 seconds
   ===================================================================
   START - test_spi_lock_release
    PASS - test_spi_lock_release in 0.002 seconds
   ===================================================================
   TESTSUITE spi_extra_api_features succeeded
   Running TESTSUITE spi_loopback
   ===================================================================
   Testing loopback spec: SLOW
   START - test_nop_nil_bufs
    PASS - test_nop_nil_bufs in 0.001 seconds
   ===================================================================
   START - test_spi_async_call
    PASS - test_spi_async_call in 0.138 seconds
   ===================================================================
   START - test_spi_complete_large_transfers
    PASS - test_spi_complete_large_transfers in 0.135 seconds
   ===================================================================
   START - test_spi_complete_loop_mode_0
    PASS - test_spi_complete_loop_mode_0 in 0.002 seconds
   ===================================================================
   START - test_spi_complete_loop_mode_1
    PASS - test_spi_complete_loop_mode_1 in 0.002 seconds
   ===================================================================
   START - test_spi_complete_loop_mode_2
    PASS - test_spi_complete_loop_mode_2 in 0.002 seconds
   ===================================================================
   START - test_spi_complete_loop_mode_3
    PASS - test_spi_complete_loop_mode_3 in 0.002 seconds
   ===================================================================
   START - test_spi_complete_multiple
    PASS - test_spi_complete_multiple in 0.003 seconds
   ===================================================================
   START - test_spi_complete_multiple_timed
   Transfer took 1375 us vs theoretical minimum 864 us
   Latency measurement: 511 us
    PASS - test_spi_complete_multiple_timed in 0.020 seconds
   ===================================================================
   START - test_spi_concurrent_transfer_different_spec
    PASS - test_spi_concurrent_transfer_different_spec in 0.007 seconds
   ===================================================================
   START - test_spi_concurrent_transfer_same_spec
    PASS - test_spi_concurrent_transfer_same_spec in 0.007 seconds
   ===================================================================
   START - test_spi_deinit
     zephyr,user miso-gpios or mosi-gpios are not defined
    SKIP - test_spi_deinit in 0.006 seconds
   ===================================================================
   START - test_spi_null_rx_buf_set
    PASS - test_spi_null_rx_buf_set in 0.002 seconds
   ===================================================================
   START - test_spi_null_tx_buf
    PASS - test_spi_null_tx_buf in 0.002 seconds
   ===================================================================
   START - test_spi_null_tx_buf_set
    PASS - test_spi_null_tx_buf_set in 0.002 seconds
   ===================================================================
   START - test_spi_null_tx_rx_buf_set
    PASS - test_spi_null_tx_rx_buf_set in 0.001 seconds
   ===================================================================
   START - test_spi_rx_bigger_than_tx
    PASS - test_spi_rx_bigger_than_tx in 0.002 seconds
   ===================================================================
   START - test_spi_rx_every_4
    PASS - test_spi_rx_every_4 in 0.002 seconds
   ===================================================================
   START - test_spi_rx_half_end
    PASS - test_spi_rx_half_end in 0.002 seconds
   ===================================================================
   START - test_spi_rx_half_start
    PASS - test_spi_rx_half_start in 0.002 seconds
   ===================================================================
   START - test_spi_same_buf_cmd
    PASS - test_spi_same_buf_cmd in 0.002 seconds
   ===================================================================
   START - test_spi_word_size_16
    PASS - test_spi_word_size_16 in 0.002 seconds
   ===================================================================
   START - test_spi_word_size_24
   E: Data size: 24 is not supported on spi@40012000
   Spi config invalid for this controller
    SKIP - test_spi_word_size_24 in 0.009 seconds
   ===================================================================
   START - test_spi_word_size_32
   E: Data size: 32 is not supported on spi@40012000
   Spi config invalid for this controller
    SKIP - test_spi_word_size_32 in 0.009 seconds
   ===================================================================
   START - test_spi_word_size_7
    PASS - test_spi_word_size_7 in 0.002 seconds
   ===================================================================
   START - test_spi_word_size_9
    PASS - test_spi_word_size_9 in 0.002 seconds
   ===================================================================
   START - test_spi_write_back
    PASS - test_spi_write_back in 0.002 seconds
   ===================================================================
   TESTSUITE spi_loopback succeeded
   Running TESTSUITE spi_loopback
   ===================================================================
   Testing loopback spec: FAST
   START - test_nop_nil_bufs
    PASS - test_nop_nil_bufs in 0.001 seconds
   ===================================================================
   START - test_spi_async_call
    PASS - test_spi_async_call in 0.072 seconds
   ===================================================================
   START - test_spi_complete_large_transfers
    PASS - test_spi_complete_large_transfers in 0.069 seconds
   ===================================================================
   START - test_spi_complete_loop_mode_0
    PASS - test_spi_complete_loop_mode_0 in 0.002 seconds
   ===================================================================
   START - test_spi_complete_loop_mode_1
    PASS - test_spi_complete_loop_mode_1 in 0.002 seconds
   ===================================================================
   START - test_spi_complete_loop_mode_2
    PASS - test_spi_complete_loop_mode_2 in 0.002 seconds
   ===================================================================
   START - test_spi_complete_loop_mode_3
    PASS - test_spi_complete_loop_mode_3 in 0.002 seconds
   ===================================================================
   START - test_spi_complete_multiple
    PASS - test_spi_complete_multiple in 0.002 seconds
   ===================================================================
   START - test_spi_complete_multiple_timed
   Transfer took 938 us vs theoretical minimum 432 us
   Latency measurement: 506 us
    PASS - test_spi_complete_multiple_timed in 0.019 seconds
   ===================================================================
   START - test_spi_concurrent_transfer_different_spec
    PASS - test_spi_concurrent_transfer_different_spec in 0.006 seconds
   ===================================================================
   START - test_spi_concurrent_transfer_same_spec
    PASS - test_spi_concurrent_transfer_same_spec in 0.006 seconds
   ===================================================================
   START - test_spi_deinit
     zephyr,user miso-gpios or mosi-gpios are not defined
    SKIP - test_spi_deinit in 0.006 seconds
   ===================================================================
   START - test_spi_null_rx_buf_set
    PASS - test_spi_null_rx_buf_set in 0.002 seconds
   ===================================================================
   START - test_spi_null_tx_buf
    PASS - test_spi_null_tx_buf in 0.002 seconds
   ===================================================================
   START - test_spi_null_tx_buf_set
    PASS - test_spi_null_tx_buf_set in 0.002 seconds
   ===================================================================
   START - test_spi_null_tx_rx_buf_set
    PASS - test_spi_null_tx_rx_buf_set in 0.001 seconds
   ===================================================================
   START - test_spi_rx_bigger_than_tx
    PASS - test_spi_rx_bigger_than_tx in 0.002 seconds
   ===================================================================
   START - test_spi_rx_every_4
    PASS - test_spi_rx_every_4 in 0.002 seconds
   ===================================================================
   START - test_spi_rx_half_end
    PASS - test_spi_rx_half_end in 0.002 seconds
   ===================================================================
   START - test_spi_rx_half_start
    PASS - test_spi_rx_half_start in 0.002 seconds
   ===================================================================
   START - test_spi_same_buf_cmd
    PASS - test_spi_same_buf_cmd in 0.002 seconds
   ===================================================================
   START - test_spi_word_size_16
    PASS - test_spi_word_size_16 in 0.002 seconds
   ===================================================================
   START - test_spi_word_size_24
   E: Data size: 24 is not supported on spi@40012000
   Spi config invalid for this controller
    SKIP - test_spi_word_size_24 in 0.009 seconds
   ===================================================================
   START - test_spi_word_size_32
   E: Data size: 32 is not supported on spi@40012000
   Spi config invalid for this controller
    SKIP - test_spi_word_size_32 in 0.009 seconds
   ===================================================================
   START - test_spi_word_size_7
    PASS - test_spi_word_size_7 in 0.002 seconds
   ===================================================================
   START - test_spi_word_size_9
    PASS - test_spi_word_size_9 in 0.002 seconds
   ===================================================================
   START - test_spi_write_back
    PASS - test_spi_write_back in 0.002 seconds
   ===================================================================
   TESTSUITE spi_loopback succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [spi_extra_api_features]: pass = 2, fail = 0, skip = 0, total = 2 duration = 0.006 seconds
    - PASS - [spi_extra_api_features.test_spi_hold_on_cs] duration = 0.004 seconds
    - PASS - [spi_extra_api_features.test_spi_lock_release] duration = 0.002 seconds

   SUITE PASS - 100.00% [spi_loopback]: pass = 24, fail = 0, skip = 3, total = 27 duration = 0.368 seconds
    - PASS - [spi_loopback.test_nop_nil_bufs] duration = 0.001 seconds
    - PASS - [spi_loopback.test_spi_async_call] duration = 0.138 seconds
    - PASS - [spi_loopback.test_spi_complete_large_transfers] duration = 0.135 seconds
    - PASS - [spi_loopback.test_spi_complete_loop_mode_0] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_complete_loop_mode_1] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_complete_loop_mode_2] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_complete_loop_mode_3] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_complete_multiple] duration = 0.003 seconds
    - PASS - [spi_loopback.test_spi_complete_multiple_timed] duration = 0.020 seconds
    - PASS - [spi_loopback.test_spi_concurrent_transfer_different_spec] duration = 0.007 seconds
    - PASS - [spi_loopback.test_spi_concurrent_transfer_same_spec] duration = 0.007 seconds
    - SKIP - [spi_loopback.test_spi_deinit] duration = 0.006 seconds
    - PASS - [spi_loopback.test_spi_null_rx_buf_set] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_null_tx_buf] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_null_tx_buf_set] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_null_tx_rx_buf_set] duration = 0.001 seconds
    - PASS - [spi_loopback.test_spi_rx_bigger_than_tx] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_rx_every_4] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_rx_half_end] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_rx_half_start] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_same_buf_cmd] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_word_size_16] duration = 0.002 seconds
    - SKIP - [spi_loopback.test_spi_word_size_24] duration = 0.009 seconds
    - SKIP - [spi_loopback.test_spi_word_size_32] duration = 0.009 seconds
    - PASS - [spi_loopback.test_spi_word_size_7] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_word_size_9] duration = 0.002 seconds
    - PASS - [spi_loopback.test_spi_write_back] duration = 0.002 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

How to run (spi_controller_peripheral)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *Wiring*: short the master (``spi1``) pads to the slave (``spi0_slave``) pads
  — connect the master's CLK, MO and MI to the slave's CLK, SI and SO, and the
  master's CS to the slave's ``SS_N``. The actual pads are set by the board
  overlay and differ per SoC.
- *Extra config*: the board overlay enables the controller and the slave. By
  default the test runs in interrupt mode; the per-SoC overlay/``.conf`` also
  wire the ``dmas`` / ``dma-names`` and ``CONFIG_SPI_BEE_DMA`` to exercise the
  DMA path.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/spi/spi_controller_peripheral

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/spi/spi_controller_peripheral

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/spi/spi_controller_peripheral

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-174-g8855e3ecc827 ***
   Running TESTSUITE spi_controller_peripheral
   ===================================================================
   START - test_basic
    PASS - test_basic in 0.174 seconds
   ===================================================================
   START - test_basic_async
    PASS - test_basic_async in 0.174 seconds
   ===================================================================
   START - test_basic_zero_len
    PASS - test_basic_zero_len in 0.094 seconds
   ===================================================================
   START - test_basic_zero_len_async
    PASS - test_basic_zero_len_async in 0.094 seconds
   ===================================================================
   START - test_only_rx
    PASS - test_only_rx in 0.174 seconds
   ===================================================================
   START - test_only_rx_async
    PASS - test_only_rx_async in 0.174 seconds
   ===================================================================
   START - test_only_rx_in_chunks
    PASS - test_only_rx_in_chunks in 0.174 seconds
   ===================================================================
   START - test_only_rx_in_chunks_async
    PASS - test_only_rx_in_chunks_async in 0.174 seconds
   ===================================================================
   START - test_only_tx
    PASS - test_only_tx in 0.174 seconds
   ===================================================================
   START - test_only_tx_async
    PASS - test_only_tx_async in 0.174 seconds
   ===================================================================
   START - test_only_tx_in_chunks
    PASS - test_only_tx_in_chunks in 0.164 seconds
   ===================================================================
   START - test_only_tx_in_chunks_async
    PASS - test_only_tx_in_chunks_async in 0.164 seconds
   ===================================================================
   START - test_short_rx
    PASS - test_short_rx in 0.176 seconds
   ===================================================================
   START - test_short_rx_async
    PASS - test_short_rx_async in 0.176 seconds
   ===================================================================
   TESTSUITE spi_controller_peripheral succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [spi_controller_peripheral]: pass = 14, fail = 0, skip = 0, total = 14 duration = 2.260 seconds
    - PASS - [spi_controller_peripheral.test_basic] duration = 0.174 seconds
    - PASS - [spi_controller_peripheral.test_basic_async] duration = 0.174 seconds
    - PASS - [spi_controller_peripheral.test_basic_zero_len] duration = 0.094 seconds
    - PASS - [spi_controller_peripheral.test_basic_zero_len_async] duration = 0.094 seconds
    - PASS - [spi_controller_peripheral.test_only_rx] duration = 0.174 seconds
    - PASS - [spi_controller_peripheral.test_only_rx_async] duration = 0.174 seconds
    - PASS - [spi_controller_peripheral.test_only_rx_in_chunks] duration = 0.174 seconds
    - PASS - [spi_controller_peripheral.test_only_rx_in_chunks_async] duration = 0.174 seconds
    - PASS - [spi_controller_peripheral.test_only_tx] duration = 0.174 seconds
    - PASS - [spi_controller_peripheral.test_only_tx_async] duration = 0.174 seconds
    - PASS - [spi_controller_peripheral.test_only_tx_in_chunks] duration = 0.164 seconds
    - PASS - [spi_controller_peripheral.test_only_tx_in_chunks_async] duration = 0.164 seconds
    - PASS - [spi_controller_peripheral.test_short_rx] duration = 0.176 seconds
    - PASS - [spi_controller_peripheral.test_short_rx_async] duration = 0.176 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`DMA <dma>`
- :doc:`Pinctrl <pinctrl>`
- :ref:`DMA memory/peripheral transfer <dma_p2m_m2p>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr SPI introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/spi.html>`_
- `Zephyr SPI API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__spi__interface.html>`_
