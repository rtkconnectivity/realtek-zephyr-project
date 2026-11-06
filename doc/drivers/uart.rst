UART
====

The UART driver exposes the Bee serial ports through the standard Zephyr UART
API.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Three transfer models: polling TX/RX, interrupt-driven TX/RX, and DMA-driven
  TX/RX through the Zephyr asynchronous API.
- Runtime reconfiguration of baud rate and frame format.
- Configurable frame format: 1 or 2 stop bits, 7- or 8-bit data, and odd, even,
  or no parity.
- Hardware RTS/CTS flow control via the standard ``hw-flow-control`` devicetree
  property.
- Fixed set of supported baud rates from 9600 up to 3 MHz.
- RX pad wakeup from DLPS, with a keep-active window after the wakeup.
- Deinit support: the UART is stopped and its pads are released.

.. note::

   On rtl87x2j ``uart3`` is reserved for the Realtek internal log, so avoid
   using that node for the application. On rtl87x2g and rtl8752h the reserved
   log UART is ``uart1`` instead. All three EVBs use ``uart2`` as the console
   (see the :ref:`Logging note in the Overview <driver_logging_note>`).

Basic Information
~~~~~~~~~~~~~~~~~
- Device nodes:

  - rtl87x2j: ``uart0``, ``uart1``, ``uart2``, ``uart3``.
  - rtl87x2g: ``uart0``, ``uart1``, ``uart2``, ``uart3``, ``uart4``, ``uart5``.
  - rtl8752h: ``uart0``, ``uart1``, ``uart2``.
- Bindings file: ``dts/bindings/serial/realtek,bee-uart.yaml``
  (compatible ``realtek,bee-uart``).
- Kconfig option: ``UART_BEE`` (``drivers/serial/Kconfig.bee``).
- Source file: ``drivers/serial/uart_bee.c``.
- Example board files:

  - the ``&uart2`` node in
    ``boards/realtek/rtl87x2j_evb/rtl87x2j_evb_rtl8762jth.dts`` (board default
    console setup); on rtl87x2g and rtl8752h the same node is in
    ``boards/realtek/rtl87x2g_evb_a/rtl87x2g_evb_a_common.dtsi`` and
    ``boards/realtek/rtl8752h_evb/rtl8752h_evb_common.dtsi``.
  - ``tests/drivers/uart/uart_async_api/boards/rtl87x2j_evb_rtl8762jth.overlay``
    (DMA-driven); ``rtl87x2g_evb_a_rtl8762gku.overlay`` and
    ``rtl8752h_evb_rtl8752hjl.overlay`` in the same directory do the same.

- Reference:

  - `Zephyr UART introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/uart.html>`_
  - `Zephyr UART API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__uart__interface.html>`_

Operation Flow
--------------

Enabling a UART device
~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set the boot-time frame format on the UART node with the
  standard ``current-speed`` (baud rate), ``parity`` (``"none"`` / ``"odd"`` /
  ``"even"``), ``stop-bits`` (``"1"`` / ``"2"``) and ``data-bits`` (``7`` / ``8``)
  properties, and give the node a ``pinctrl-0`` group.

  .. code-block:: devicetree

     &uart2 {
         pinctrl-0 = <&uart2_default>;
         pinctrl-names = "default";
         current-speed = <115200>;
         parity = "none";
         stop-bits = "1";
         data-bits = <8>;
         status = "okay";
     };

     &pinctrl {
         uart2_default: uart2_default {
             group1 {
                 psels = <BEE_PSEL(UART2_TX, P3_0)>,
                         <BEE_PSEL(UART2_RX, P3_1)>;
                 output-disable;
                 bias-pull-up;
             };
         };
     };

  To use hardware RTS/CTS flow control, add the standard ``hw-flow-control``
  boolean property to the UART node and add the RTS and CTS signals (for example
  ``UART2_RTS`` / ``UART2_CTS``) to the node's ``pinctrl-0`` group alongside TX
  and RX. See the :doc:`Pinctrl <pinctrl>` chapter for how to declare a pinctrl
  group.

  Enable serial support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_SERIAL=y

- *How to use*: these properties take effect at boot; change the baud rate,
  parity, stop bits and flow control later at runtime with ``uart_configure()``
  (see :ref:`Runtime configuration <uart_runtime_config>` below).

.. note::

   The supported baud rates are a fixed set. The driver matches ``current-speed``
   against a fixed baud-rate table (9600, …, up to 3000000). A rate that is not in
   the table is rejected (``uart_configure()`` returns an error and the boot
   configuration fails). Use one of the supported standard rates.

.. note::

   The RX-FIFO interrupt trigger threshold is set with the ``rx-threshold``
   devicetree property on the UART node (an integer, 1–29).
   On rtl87x2g / rtl8752h, the v3.7 driver differs here: the RX-FIFO interrupt
   trigger threshold is set with the global Kconfig option
   ``CONFIG_UART_BEE_RX_THRESHOLD`` instead of the ``rx-threshold`` devicetree
   property.

.. _uart_runtime_config:

Runtime configuration
~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable ``CONFIG_UART_USE_RUNTIME_CONFIGURE=y``.
- *How to use*: change the baud rate, parity, stop bits and flow control at
  runtime with ``uart_configure()``, and read the active settings back with
  ``uart_config_get()``.
- *Example*: ``uart_basic_api.test_uart_configure`` and
  ``uart_basic_api.test_uart_config_get``.

Polling
~~~~~~~
- *How to use*: send a single byte with ``uart_poll_out()`` and read a single
  byte with ``uart_poll_in()``. No interrupt or DMA configuration is required.
- *Example*: ``uart_basic_api.test_uart_poll_out`` and
  ``uart_basic_api.test_uart_poll_in``.

Interrupt-driven
~~~~~~~~~~~~~~~~
- *How to configure*: enable ``CONFIG_UART_INTERRUPT_DRIVEN=y``.
- *How to use*: register an ISR with ``uart_irq_callback_set()``; inside the ISR
  push data with ``uart_fifo_fill()`` when ``uart_irq_tx_ready()`` is set and
  drain it with ``uart_fifo_read()`` when ``uart_irq_rx_ready()`` is set; arm the
  paths with ``uart_irq_tx_enable()`` / ``uart_irq_rx_enable()``.
- *Example*: ``uart_basic_api.test_uart_fifo_fill`` / ``test_uart_fifo_read``,
  and ``uart_basic_api_pending.test_uart_pending``.

.. _uart_dma_driven:

DMA-driven (asynchronous API)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the asynchronous API, attach a DMA channel to the
  UART node for each direction that uses DMA, and enable ``dma0``. Each
  direction is optional: ``dma-names`` may list only ``"tx"`` or only
  ``"rx"``, and calling ``uart_tx()`` or ``uart_rx_enable()`` on a direction
  without a channel returns ``-ENODEV``. The example below configures both.

  In ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_SERIAL=y
     CONFIG_UART_ASYNC_API=y

  In the board overlay (real values from the rtl87x2j test overlay). Each
  ``dmas`` entry is ``<&dma0 <channel> <handshake-slot> <config>>``, with the
  ``<config>`` cell composed from the self-documenting ``BEE_DMA_*`` macros
  instead of a raw hex value:

  .. code-block:: devicetree

     &uart2 {
         pinctrl-0 = <&uart2_default>;
         pinctrl-names = "default";
         parity = "none";
         stop-bits = "1";
         data-bits = <8>;
         dmas = <&dma0 3 BEE_DMA_HANDSHAKE_UART2_TX
                 (BEE_DMA_M2P | BEE_DMA_SRC_INC | BEE_DMA_DST_FIXED | BEE_DMA_SRC_WIDTH_8BIT |
                  BEE_DMA_DST_WIDTH_8BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_1) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_1) | BEE_DMA_PRIORITY(1))>,
                <&dma0 2 BEE_DMA_HANDSHAKE_UART2_RX
                 (BEE_DMA_P2M | BEE_DMA_SRC_FIXED | BEE_DMA_DST_INC | BEE_DMA_SRC_WIDTH_8BIT |
                  BEE_DMA_DST_WIDTH_8BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_1) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_1) | BEE_DMA_PRIORITY(0))>;
         dma-names = "tx", "rx";
         status = "okay";
     };

     &dma0 {
         status = "okay";
     };

  The ``tx`` entry is memory-to-peripheral (``BEE_DMA_M2P``) with the source
  address incrementing and the destination fixed; the ``rx`` entry is the reverse
  (``BEE_DMA_P2M``, source fixed, destination incrementing). Both use 8-bit source
  and destination widths and a burst size (msize) of 1. The
  ``BEE_DMA_HANDSHAKE_UART2_TX`` / ``_RX`` slot macros come from the per-SoC
  header (``rtl87x2j-dma.h``, ``rtl87x2g-dma.h``, or ``rtl8752h-dma.h``),
  which the SoC dtsi already includes, so the overlay needs no ``#include``.
  For the full ``<config>`` bit layout and the complete ``BEE_DMA_*`` macro
  list see the :doc:`DMA <dma>` chapter.

- *How to use*: register an event callback with ``uart_callback_set()``, start a
  transmission with ``uart_tx()``, or enable reception with ``uart_rx_enable()``.
  Completion and buffer events (``UART_TX_DONE``, ``UART_RX_RDY``,
  ``UART_RX_BUF_REQUEST``, ``UART_RX_BUF_RELEASED``, ...) are delivered through
  the callback.
- *Example*: ``uart_async_single_read.test_single_read``,
  ``uart_async_chain_write.test_chained_write``, and the other ``uart_async_*``
  suites.

.. note::

   The ``BEE_DMA_HANDSHAKE_UART2_TX`` / ``_RX`` macros resolve to the SoC's own
   handshake-slot IDs: slots **18 (tx) / 19 (rx)** on rtl87x2j and rtl8752h,
   but **4 (tx) / 5 (rx)** on rtl87x2g. Because each SoC dtsi includes its
   own header, the same overlay text is portable across SoCs without editing
   the slot number.

.. note::

   Polling can be mixed with the asynchronous API on the same UART, for
   example a console printing with ``uart_poll_out()`` while RX runs on DMA.
   Do not mix the interrupt-driven API with the asynchronous API on the same
   UART, not even with one direction each.

Deinit
~~~~~~
- *How to configure*: enable ``CONFIG_DEVICE_DEINIT_SUPPORT=y``.
- *How to use*: call ``device_deinit()`` on the UART. The driver stops the
  UART, gates its clock, and releases its pads. Pinmux is selected per pad,
  so a pad left on a UART function keeps feeding the UART after another pad
  is routed to it. The pads are released with the ``sleep`` pinctrl state if
  the node has one; otherwise they become ``SW_MODE`` inputs with the pull of
  the ``default`` state. Call ``device_init()`` to bring the UART up again.
- *Example*: MCUboot deinitializes its devices before booting the image
  (``CONFIG_MCUBOOT_DEINIT_DEVICES``, enabled by default on Bee), so the
  application can route the UART to other pads.

.. _uart_dlps:

Power Management (DLPS)
-----------------------
On a SoC with a PCK600 (such as rtl87x2j), UART power management is built on
``CONFIG_PM_DEVICE`` and device runtime PM: enable
``CONFIG_PM_DEVICE_RUNTIME`` and add ``zephyr,pm-device-runtime-auto`` to the
UART node, as the rtl87x2j board's ``uart2`` does. When runtime PM suspends
the UART, the driver hands its clock back to the hardware, which gates it once
the UART is idle and lets the PCK600 put the UART to sleep. The registers are
retained, so nothing has to be restored on resume. Without runtime PM the
device is resumed at init and keeps its clock on, so the UART never sleeps.

- *DLPS entry*: the driver holds the UART active per path (TX and RX), and
  runtime PM suspends it once neither path holds it:

  - TX: an asynchronous ``uart_tx()`` holds the UART from the start of the
    transfer until it completes or is aborted. Polling and interrupt-driven TX
    take no hold of their own.
  - RX: enabling RX (``uart_irq_rx_enable()`` or ``uart_rx_enable()``)
    releases the RX hold and arms the RX wakeup pin, which the driver also
    arms once at init. After a pad wakeup the RX path is held again for the
    keep-active window described below. Disabling RX disarms the wakeup pin
    and releases the hold.
  - Without RX wakeup configured, the driver never hands the clock back, so
    the UART never sleeps. This keeps incoming data from being lost on a UART
    that cannot be woken by its RX pad. (The exception is a keep-active
    timeout of ``0``; see below.)

  .. note::

     On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
     PCK600, so UART PM is guarded by ``CONFIG_PM_DEVICE`` and the sleep decision
     is made in software rather than by hardware. The TX path is managed by the
     application: once the platform allows DLPS entry the UART powers off whether
     or not TX has finished. The RX path allows sleep after a period with no
     incoming data following a wakeup, and as long as the RX path allows sleep the
     driver lets that UART sleep.

- *Missed RX while asleep*: while a UART is asleep all data on the line is lost,
  so to keep receiving during sleep the RX pad must be armed as a wakeup source,
  as described below.
- *How to configure RX wakeup*: mark the UART node ``wakeup-source``. The
  driver takes the ``UARTx_RX`` pad from the ``default`` pinctrl state and arms
  it for low-level wakeup, so the start bit of an incoming character wakes the
  SoC; no wakeup property is needed in pinctrl. If the ``default`` state has no
  RX pad, the driver logs ``wakeup-source without an RX pin in its default
  state`` at init and the UART is not woken by RX. The example below is the
  ``uart2`` setup from the rtl87x2j board files:

  .. code-block:: devicetree

     &uart2 {
         pinctrl-0 = <&uart2_default>;
         pinctrl-names = "default";
         wakeup-source;
         zephyr,pm-device-runtime-auto;
         status = "okay";
         /* ... frame format ... */
     };

     &pinctrl {
         uart2_default: uart2_default {
             group1 {
                 psels = <BEE_PSEL(UART2_TX, P3_0)>,
                         <BEE_PSEL(UART2_RX, P3_1)>;
                 output-disable;
                 bias-pull-up;
             };
         };
     };

  In DLPS the RX pad keeps the pull-up of its ``default`` state, so the idle
  line stays high and does not wake the SoC by itself; no
  ``sleep-hardware-state`` group is needed. Keep the RX pad pulled up. The
  UART driver does not use a ``sleep`` pinctrl state across DLPS. See the
  :ref:`Pinctrl DLPS section <pinctrl_dlps>` for details.

  At higher baud rates the first character after wakeup is usually lost, because
  the UART has not been fully restored at the instant the pad wakeup fires. A
  common convention is therefore for the peer to send a dummy wakeup frame
  first to wake the SoC; that frame arrives before the UART is ready, so its
  reception is unreliable and the application should discard it.

- *Staying awake after RX wakeup*: after an RX pad wakeup the driver holds the
  UART active for ``CONFIG_UART_BEE_KEEP_ACTIVE_TIMEOUT_MSEC`` (default
  **5000 ms**) so it can keep receiving, and restarts that window each time
  the RX line goes idle after a frame. When the window expires the driver
  releases the hold and re-arms the wakeup pin. Setting the option to ``0``
  turns RX wakeup off altogether: no wakeup pin is armed and the UART may
  sleep right away, so incoming data can be lost.

- *Application-controlled clock gating*: with the keep-active timeout set to
  ``0`` the UART no longer holds off DLPS by itself, so the application can
  decide when the UART clock is gated by bracketing each period of use with
  the runtime PM calls:

  - ``pm_device_runtime_get()`` before using the UART: the clock is forced on
    and the UART stays active until the matching put.
  - ``pm_device_runtime_put()`` once the application is done with the UART:
    the clock goes back to hardware auto gating, so it is gated once the UART
    is idle and the UART may sleep.

  .. code-block:: kconfig

     CONFIG_PM_DEVICE_RUNTIME=y
     CONFIG_UART_BEE_KEEP_ACTIVE_TIMEOUT_MSEC=0

  .. code-block:: c

     pm_device_runtime_get(uart_dev);
     /* clock forced on: transmit and receive here */
     uart_poll_out(uart_dev, 'A');
     pm_device_runtime_put(uart_dev);

  Runtime PM must also be enabled on the node
  (``zephyr,pm-device-runtime-auto``, as above); otherwise both calls return
  without effect. The calls are reference counted together with the hold that
  an asynchronous ``uart_tx()`` takes, so the clock stays forced on until
  every user has released it. Because no wakeup pin is armed, data that
  arrives after the put, once the clock has been gated, can be lost, so the
  peer should only send while the application holds the UART.

- *DMA and DLPS*: the TX and RX paths are treated differently. The TX path
  holds the UART for the length of each ``uart_tx()`` transfer and releases
  it as soon as the transfer finishes. On the RX path, ``uart_rx_enable()``
  lets the UART receiver raise its own PCK600 active request while data
  arrives, and ``uart_rx_disable()`` stops it again, so the application
  should manage whether RX needs to stay enabled.

  .. note::

     On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
     PCK600, so in DMA-driven mode the DMA cannot continue across DLPS and the
     application must re-establish DMA after exiting DLPS:

     - *TX*: call ``uart_tx()`` again to restart transmission from the beginning.
     - *RX*: from a worker thread, call ``uart_rx_disable()`` then
       ``uart_rx_enable()`` (used as a pair) to restore RX. Do not operate the
       UART directly inside the application's DLPS-exit callback — the UART's own
       restore may not have run yet.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so the driver implements a full ``PM_DEVICE`` DLPS handler instead. The
   handler stores and restores the UART registers across DLPS, with the
   keep-active timeout defaulting to 10000 ms
   (``CONFIG_UART_BEE_KEEP_ACTIVE_TIMEOUT_MSEC``, plus a
   ``CONFIG_UART_BEE_KEEP_ACTIVE_AFTER_RX_WAKEUP`` toggle). RX wakeup is armed
   through the pad's ``wakeup-low`` / ``wakeup-high`` pinctrl property, written
   with the 5-argument ``BEE_PSEL`` form (see the :doc:`Pinctrl <pinctrl>`
   chapter), and no ``wakeup-source`` property is needed.

Samples and Logs
----------------
Samples: ``tests/drivers/uart/uart_basic_api`` (polling / interrupt-driven) and
``tests/drivers/uart/uart_async_api`` (DMA-driven / asynchronous).

How to run (uart_basic_api)
~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *Wiring*: connect ``uart2`` to a USB-serial adapter (cross-wired): the TX pad
  to the adapter RX and the RX pad to the adapter TX. On the rtl87x2j EVB
  these are ``P3_0`` (TX) and ``P3_1`` (RX); the rtl87x2g and rtl8752h EVBs
  use the same pads.
- *Extra config*: none — the test runs over the native console on ``uart2``.
- *Interaction*: when the console prints ``Please send characters to serial
  console``, send characters to drive the interactive tests.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/uart/uart_basic_api

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/uart/uart_basic_api

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/uart/uart_basic_api

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-172-g6ba6b2556889 ***
   Running TESTSUITE uart_basic_api
   ===================================================================
   START - test_uart_config_get
    PASS - test_uart_config_get in 0.004 seconds
   ===================================================================
   START - test_uart_configure
    PASS - test_uart_configure in 0.001 seconds
   ===================================================================
   START - test_uart_fifo_fill
   This is a FIFO test.
    PASS - test_uart_fifo_fill in 0.501 seconds
   ===================================================================
   START - test_uart_fifo_read
   Please send characters to serial console
    PASS - test_uart_fifo_read in 1.019 seconds
   ===================================================================
   START - test_uart_poll_in
   Please send characters to serial console
    PASS - test_uart_poll_in in 0.386 seconds
   ===================================================================
   START - test_uart_poll_out
   This is a POLL test.
    PASS - test_uart_poll_out in 0.003 seconds
   ===================================================================
   TESTSUITE uart_basic_api succeeded
   Running TESTSUITE uart_basic_api_pending
   ===================================================================
   START - test_uart_pending
   Please send characters to serial console
    PASS - test_uart_pending in 0.004 seconds
   ===================================================================
   TESTSUITE uart_basic_api_pending succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [uart_basic_api]: pass = 6, fail = 0, skip = 0, total = 6 duration = 1.914 seconds
    - PASS - [uart_basic_api.test_uart_config_get] duration = 0.004 seconds
    - PASS - [uart_basic_api.test_uart_configure] duration = 0.001 seconds
    - PASS - [uart_basic_api.test_uart_fifo_fill] duration = 0.501 seconds
    - PASS - [uart_basic_api.test_uart_fifo_read] duration = 1.019 seconds
    - PASS - [uart_basic_api.test_uart_poll_in] duration = 0.386 seconds
    - PASS - [uart_basic_api.test_uart_poll_out] duration = 0.003 seconds

   SUITE PASS - 100.00% [uart_basic_api_pending]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.004 seconds
    - PASS - [uart_basic_api_pending.test_uart_pending] duration = 0.004 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

How to run (uart_async_api)
~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *Wiring*: short ``uart2`` TX to RX so the port loops back to itself. On the
  rtl87x2j EVB this means shorting ``P3_0`` to ``P3_1``; the rtl87x2g and
  rtl8752h overlays use the same pads.
- *Extra config*: ``CONFIG_UART_ASYNC_API=y``, plus the ``tx`` / ``rx`` DMA
  channels on ``uart2`` and ``&dma0`` enabled — already set in the board overlay
  (see :ref:`DMA-driven (asynchronous API) <uart_dma_driven>` above).
- *Build & flash*:

  .. code-block:: console

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/uart/uart_async_api

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/uart/uart_async_api

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/uart/uart_async_api

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-172-g6ba6b2556889 ***
   Running TESTSUITE uart_async_chain_read
   ===================================================================
   UART instance:serial@40011800
   START - test_chained_read
   Message 0Message 1Message 2Message 3Message 4Message 5 PASS - test_chained_read in 0.426 seconds
   ===================================================================
   TESTSUITE uart_async_chain_read succeeded
   Running TESTSUITE uart_async_chain_write
   ===================================================================
   UART instance:serial@40011800
   START - test_chained_write
   Message 1Message 2 PASS - test_chained_write in 0.003 seconds
   ===================================================================
   TESTSUITE uart_async_chain_write succeeded
   Running TESTSUITE uart_async_double_buf
   ===================================================================
   UART instance:serial@40011800
   START - test_double_buffer
   000001002003004005006007008009010011012013014015016017018019020021022023024025026027028029030031032033034035036037038039040041042043044045046047048049050051052053054055056057058059060061062063064065066067068069070071072073074075076077078079080081082083084085086087088089090091092093094095096097098099 PASS - test_double_buffer in 2.731 seconds
   ===================================================================
   TESTSUITE uart_async_double_buf succeeded
   Running TESTSUITE uart_async_long_buf
   ===================================================================
   UART instance:serial@40011800
   START - test_long_buffers
    PASS - test_long_buffers in 0.166 seconds
   ===================================================================
   TESTSUITE uart_async_long_buf succeeded
   Running TESTSUITE uart_async_multi_rx
   ===================================================================
   UART instance:serial@40011800
   START - test_multiple_rx_enable
   testtest PASS - test_multiple_rx_enable in 0.705 seconds
   ===================================================================
   TESTSUITE uart_async_multi_rx succeeded
   Running TESTSUITE uart_async_read_abort
   ===================================================================
   UART instance:serial@40011800
   START - test_read_abort
    PASS - test_read_abort in 1.122 seconds
   ===================================================================
   TESTSUITE uart_async_read_abort succeeded
   Running TESTSUITE uart_async_single_read
   ===================================================================
   UART instance:serial@40011800
   START - test_single_read
   0123456789 PASS - test_single_read in 0.707 seconds
   ===================================================================
   TESTSUITE uart_async_single_read succeeded
   Running TESTSUITE uart_async_timeout
   ===================================================================
   UART instance:serial@40011800
   START - test_forever_timeout
    PASS - test_forever_timeout in 3.002 seconds
   ===================================================================
   TESTSUITE uart_async_timeout succeeded
   Running TESTSUITE uart_async_var_buf_length
   ===================================================================
   UART instance:serial@40011800
   START - test_var_buf_length
    PASS - test_var_buf_length in 1.043 seconds
   ===================================================================
   TESTSUITE uart_async_var_buf_length succeeded
   Running TESTSUITE uart_async_write_abort
   ===================================================================
   UART instance:serial@40011800
   START - test_write_abort
    PASS - test_write_abort in 0.134 seconds
   ===================================================================
   TESTSUITE uart_async_write_abort succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [uart_async_chain_read]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.426 seconds
    - PASS - [uart_async_chain_read.test_chained_read] duration = 0.426 seconds

   SUITE PASS - 100.00% [uart_async_chain_write]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.003 seconds
    - PASS - [uart_async_chain_write.test_chained_write] duration = 0.003 seconds

   SUITE PASS - 100.00% [uart_async_double_buf]: pass = 1, fail = 0, skip = 0, total = 1 duration = 2.731 seconds
    - PASS - [uart_async_double_buf.test_double_buffer] duration = 2.731 seconds

   SUITE PASS - 100.00% [uart_async_long_buf]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.166 seconds
    - PASS - [uart_async_long_buf.test_long_buffers] duration = 0.166 seconds

   SUITE PASS - 100.00% [uart_async_multi_rx]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.705 seconds
    - PASS - [uart_async_multi_rx.test_multiple_rx_enable] duration = 0.705 seconds

   SUITE PASS - 100.00% [uart_async_read_abort]: pass = 1, fail = 0, skip = 0, total = 1 duration = 1.122 seconds
    - PASS - [uart_async_read_abort.test_read_abort] duration = 1.122 seconds

   SUITE PASS - 100.00% [uart_async_single_read]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.707 seconds
    - PASS - [uart_async_single_read.test_single_read] duration = 0.707 seconds

   SUITE PASS - 100.00% [uart_async_timeout]: pass = 1, fail = 0, skip = 0, total = 1 duration = 3.002 seconds
    - PASS - [uart_async_timeout.test_forever_timeout] duration = 3.002 seconds

   SUITE PASS - 100.00% [uart_async_var_buf_length]: pass = 1, fail = 0, skip = 0, total = 1 duration = 1.043 seconds
    - PASS - [uart_async_var_buf_length.test_var_buf_length] duration = 1.043 seconds

   SUITE PASS - 100.00% [uart_async_write_abort]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.134 seconds
    - PASS - [uart_async_write_abort.test_write_abort] duration = 0.134 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`DMA <dma>`
- :doc:`Pinctrl <pinctrl>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr UART introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/uart.html>`_
- `Zephyr UART API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__uart__interface.html>`_
- `Zephyr asynchronous UART API <https://docs.zephyrproject.org/latest/doxygen/html/group__uart__async.html>`_
