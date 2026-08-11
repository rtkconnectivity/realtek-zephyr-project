IR
==

The IR driver drives the Bee infrared controller, a half-duplex peripheral that
transmits and receives a raw modulated carrier on a single IR line. There is no
standard Zephyr IR API, so the driver is a Bee-specific ``drivers/ir``
subsystem: it implements the IR driver API declared in
``include/zephyr/drivers/ir.h`` (the ``ir_*`` calls and syscalls), which the
application uses to set the carrier, transmit a buffer of carrier-period words,
and enable capture with completion callbacks. This Bee-specific subsystem is
provisional: once the ``pulse_io`` driver is available upstream, the IR driver
is expected to be folded into it.

All three SoCs (rtl87x2g, rtl8752h, rtl87x2j) use the same unified driver
(``ir_bee.c``, compatible ``realtek,bee-ir``).

The same peripheral is time-shared between transmit and receive: the driver
switches between a transmit and a receive configuration on each enable, so only
one direction is active at a time. Both directions move data as an array of
32-bit words, where the top bit selects the carrier state (set for a carrier
burst, cleared for a gap) and the remaining bits hold that segment's duration.
On transmit the duration is expressed in carrier periods; on receive each word
carries the captured segment length in raw 40 MHz sample counts.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Half-duplex transmit and receive on a single IR line, time-shared between the
  two directions.
- Carrier generation with configurable frequency and duty cycle.
- Transmit of a buffer of 32-bit carrier-period words; receive capture of the
  raw carrier envelope into 32-bit words.
- Interrupt-driven FIFO data path, with DMA TX and DMA RX support selected
  independently per direction.
- RX trigger-edge selection (rising or falling) and a configurable idle-count
  threshold that marks the end of a received frame.
- Completion callbacks for transmit-complete, receive-data-ready, and
  receive-stopped events.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``ir``.
- Bindings file: ``dts/bindings/ir/realtek,bee-ir.yaml``
  (compatible ``realtek,bee-ir``).
- Driver API header: ``include/zephyr/drivers/ir.h``.
- Kconfig options: ``IR`` (``drivers/ir/Kconfig``) enables the IR subsystem and
  ``IR_BEE`` (``drivers/ir/Kconfig.bee``) enables the Bee driver.
- Source file: ``drivers/ir/ir_bee.c``.
- Example board files (under ``samples/drivers/ir/boards/``):
  ``rtl87x2g_evb_a_rtl8762gku.overlay`` (rtl87x2g),
  ``rtl8752h_evb_rtl8752hjl.overlay`` (rtl8752h), and
  ``rtl87x2j_evb_rtl8762jth.overlay`` (rtl87x2j).

The peripheral's ``reg``, ``clocks``, and ``interrupts`` are already defined on
the SoC ``ir`` node; a board overlay only has to route the pads, optionally wire
up DMA, and set ``status = "okay"``.

Operation Flow
--------------

Enabling an IR device
~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the ``ir`` node and give it two pinctrl states,
  one for transmit and one for receive. Unlike most peripherals the driver does
  not use a ``default`` state: the states are matched by name, and
  ``pinctrl-names`` must list ``ir-tx`` and ``ir-rx``. The driver applies
  ``ir-tx`` when transmit is enabled and ``ir-rx`` when receive is enabled, so
  neither pad is driven until a direction is started. For receiving high-rate IR
  waveforms (a carrier above roughly 5 kHz) the DMA data path is recommended, so
  the CPU keeps up and the RX FIFO does not overflow and drop data. To use the
  DMA data path, add ``dmas`` entries named ``tx`` and/or ``rx`` (and enable the
  DMA controller); a direction with no DMA entry uses the interrupt/FIFO path
  instead. ``rx-falling-edge-trig`` selects the RX capture edge (see below).

  .. code-block:: devicetree

     #include <dt-bindings/dma/rtl87x2j-dma.h>

     &dma0 {
         status = "okay";
     };

     &ir {
         pinctrl-0 = <&ir_tx>;
         pinctrl-1 = <&ir_rx>;
         pinctrl-names = "ir-tx", "ir-rx";
         dmas = <&dma0 0 BEE_DMA_HANDSHAKE_IR_TX
                 (BEE_DMA_M2P | BEE_DMA_SRC_INC | BEE_DMA_DST_FIXED | BEE_DMA_SRC_WIDTH_32BIT |
                  BEE_DMA_DST_WIDTH_32BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_4) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_4) | BEE_DMA_PRIORITY(0))>,
                <&dma0 0 BEE_DMA_HANDSHAKE_IR_RX
                 (BEE_DMA_P2M | BEE_DMA_SRC_FIXED | BEE_DMA_DST_INC | BEE_DMA_SRC_WIDTH_32BIT |
                  BEE_DMA_DST_WIDTH_32BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_4) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_4) | BEE_DMA_PRIORITY(0))>;
         dma-names = "tx", "rx";
         status = "okay";
     };

     &pinctrl {
         ir_tx: ir_tx {
             group1 {
                 psels = <BEE_PSEL(IRDA_TX, P1_0)>;
                 output-enable;
                 output-low;
                 bias-disable;
             };
         };

         ir_rx: ir_rx {
             group1 {
                 psels = <BEE_PSEL(IRDA_RX, P0_1)>;
                 output-disable;
                 bias-disable;
             };
         };
     };

- *Example*: the ``&ir`` node and its ``ir_tx`` / ``ir_rx`` pinctrl groups in
  the overlays under ``samples/drivers/ir/boards/``.

Transmitting
~~~~~~~~~~~~
- *How to use*: set the carrier with ``ir_set_freq()`` (frequency in Hz and a
  duty-cycle value), enable transmit with ``ir_tx_enable()`` passing a callback,
  then send a buffer of carrier-period words with ``ir_tx()``. When the frame
  has been clocked out the driver reports an ``IR_TX_COMPLETED`` event to the
  callback. If a buffer is larger than the hardware FIFO the application feeds
  the remainder from the callback by calling ``ir_tx()`` again with the unsent
  tail.
- *Example*: the startup transmit loop and ``ir_tx_cb`` in
  ``samples/drivers/ir/src/main.c``.

.. note::

   The requested carrier frequency must be at least 2442 Hz and no greater than
   the source clock; ``ir_set_freq()`` returns ``-ENOTSUP`` for an out-of-range
   value.

Receiving
~~~~~~~~~
- *How to use*: enable receive with ``ir_rx_enable()``, passing a callback, the
  receive length that triggers an ``IR_RX_RECEIVED`` event, and the idle-count
  threshold that ends a frame with an ``IR_RX_STOPPED`` event. The captured
  words are delivered through the callback's ``ir_event_rx`` data. Call
  ``ir_rx_disable()`` to stop capture and collect any remaining data. The RX
  path is set up for a fast sample clock, so the application typically raises
  the carrier setting with ``ir_set_freq()`` before enabling receive.
- *How to configure*: add ``rx-falling-edge-trig`` to the node to capture on the
  falling edge; without it, RX triggers on the rising edge.
- *Example*: ``ir_rx_cb`` and the receive/echo loop in
  ``samples/drivers/ir/src/main.c``.

Power Management (DLPS)
-----------------------
- *Behavior*: on a SoC with a PCK600 (such as rtl87x2j) the PCK600 manages power
  in hardware: while a transmit is in progress neither IR nor the system sleeps,
  and once the transmit completes the PCK600 lets IR sleep automatically; once
  ``ir_rx_enable()`` starts a receive neither IR nor the system sleeps, so the
  application must call ``ir_rx_disable()`` before the system can sleep.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so the driver implements a full ``PM_DEVICE`` DLPS handler instead.
   On suspend it applies the ``sleep`` pinctrl state; on resume it re-applies
   the transmit or receive pin state and restores the IR registers, so entering
   and leaving DLPS is transparent to the application — it needs no
   reconfiguration and keeps using the same active-state API after wakeup. The
   handler does not, however, protect a transfer that is in progress: if the
   system enters DLPS during a transmit or receive the IR IP loses power and the
   transfer stops. The application must manage sleep itself: before entering
   DLPS make sure any transmit has finished, and disable an active receive with
   ``ir_rx_disable()``; after wakeup restart transmission with ``ir_tx()`` or
   re-enable receive with ``ir_rx_enable()``.

Samples and Logs
----------------
Sample: ``samples/drivers/ir``.

How to run
~~~~~~~~~~
- *Wiring*: connect ``ir-tx`` to a logic analyzer to observe the transmitted
  waveform, and connect ``ir-rx`` to a source of a known waveform (for example
  the ``ir-tx`` of a second board flashed with this same sample). The sample
  transmits a few demo frames at startup and then loops: it receives an incoming
  waveform, prints the demodulated data, and echoes it back out.
- *Extra config*: none — the board overlay enables the ``ir`` node, routes the
  pads, and wires the DMA channels.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku samples/drivers/ir

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   samples/drivers/ir

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   samples/drivers/ir

     west flash --port <your-flash-serial-port>

After the startup frames the board waits for IR input. As each frame arrives
from a second board, the receive callback demodulates it and the sample prints
the waveform group (one ``MARK`` or ``SPACE`` line per symbol) and then
re-transmits it, echoing the frame back out over the IR line.

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :doc:`DMA <dma>`
- :ref:`Logging note in the Overview <driver_logging_note>`
