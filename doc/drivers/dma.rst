DMA
===

The DMA driver exposes the Bee general-purpose DMA controller through the
standard Zephyr DMA API. Applications use it mainly for memory-to-memory
transfers; peripheral transfers (UART, SPI, …) are driven from inside the
peripheral drivers, with the application only wiring a DMA channel on the
peripheral's devicetree node.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Single controller ``dma0``. The application requests channel IDs ``0``–``N-1``.
- Memory-to-memory (M2M) transfers for buffer copies and block moves — this is
  the mode applications use directly.
- Peripheral-to-memory (P2M) and memory-to-peripheral (M2P) transfers, generally
  handled inside the peripheral drivers (for example UART DMA TX/RX).
- Multi-block transfers through linked-list items (LLI).
- Suspend / resume of an in-flight transfer.
- Peripheral-to-peripheral (P2P) transfers are **not** supported (the DMA
  exposes a single handshake slot). Scatter/gather is **not** supported either.

.. note::

   The number of channels differs per SoC and comes from the SoC devicetree
   (``dma-channels``): **rtl87x2g** has **9** channels (IDs 0–8), **rtl8752h**
   has **3** (0–2), and **rtl87x2j** has **6** (0–5). Request only IDs within
   your SoC's range.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``dma0``.
- Bindings file: ``dts/bindings/dma/realtek,bee-dma.yaml``
  (compatible ``realtek,bee-dma``).
- dt-bindings macro headers: ``include/zephyr/dt-bindings/dma/bee-dma.h``
  (common ``config`` field macros) and the per-SoC handshake headers
  ``rtl87x2g-dma.h`` / ``rtl8752h-dma.h`` / ``rtl87x2j-dma.h`` in the same
  directory.
- Kconfig options: ``DMA_BEE``, ``DMA_BEE_LLI_POOL_COUNT``,
  ``DMA_BEE_MAX_BLOCKS_PER_CHANNEL`` (``drivers/dma/Kconfig.bee``).
- Source file: ``drivers/dma/dma_bee.c``.
- Example board files (under ``tests/drivers/dma/loop_transfer/boards/``):
  ``rtl87x2g_evb_a_rtl8762gku.overlay`` (+ ``.conf``),
  ``rtl8752h_evb_rtl8752hjl.overlay`` (+ ``.conf``), and
  ``rtl87x2j_evb_rtl8762jth.overlay`` (+ ``.conf``); plus the ``dmas`` nodes in
  the UART async overlays (``tests/drivers/uart/uart_async_api/boards/*``).
- Reference:

  - `Zephyr DMA introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/dma.html>`_
  - `Zephyr DMA API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__dma__interface.html>`_

.. note::

   The driver stores its linked-list-item (LLI) descriptors in a static
   ``sys_mem_blocks`` pool, so you do **not** need ``CONFIG_HEAP_MEM_POOL_SIZE``
   for DMA. Size the pool with ``CONFIG_DMA_BEE_LLI_POOL_COUNT`` (default 128;
   one LLI per block) and bound a single transaction with
   ``CONFIG_DMA_BEE_MAX_BLOCKS_PER_CHANNEL`` (default 16, range 1–64).

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: it allocates LLI
   descriptors with ``k_malloc`` instead of the static pool, so a multi-block
   transfer needs a system heap — set ``CONFIG_HEAP_MEM_POOL_SIZE`` large enough
   for the LLIs you use.

Operation Flow
--------------

Enabling the DMA controller
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set ``status = "okay"`` on the ``dma0`` node and enable the
  driver in ``prj.conf``:

  .. code-block:: devicetree

     &dma0 {
         status = "okay";
     };

  .. code-block:: kconfig

     CONFIG_DMA=y

- *Example*: the ``&dma0`` node in
  ``tests/drivers/dma/loop_transfer/boards/rtl87x2j_evb_rtl8762jth.overlay``,
  or the ``rtl87x2g_evb_a_rtl8762gku`` / ``rtl8752h_evb_rtl8752hjl`` overlays.

Memory-to-memory transfer
~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: request a channel with ``dma_request_channel()``, describe the
  transfer with a ``struct dma_config`` (set ``channel_direction =
  MEMORY_TO_MEMORY`` and fill one or more ``dma_block_config`` blocks), apply it
  with ``dma_config()`` (set the completion callback there), then start it with
  ``dma_start()``, and release it with ``dma_release_channel()`` when done. The
  channel priority must be ``0``–``9``.
- *Example*: ``dma_m2m_loop.test_tst_dma0_m2m_loop``.

.. _dma_p2m_m2p:

Memory-to-peripheral/peripheral-to-memory transfer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: applications do not configure these directly. Add a DMA
  channel to the peripheral's node via its ``dmas`` / ``dma-names`` properties,
  and enable ``dma0``. Each ``dmas`` entry is ``<&dma0 <channel> <slot> <config>>``:

  - ``channel`` — the DMA channel to use (within the SoC's range).
  - ``slot`` — the peripheral handshake ID. Use the named
    ``BEE_DMA_HANDSHAKE_*`` macro from your SoC's header rather than a raw
    number.
  - ``config`` — a 32-bit bitmask describing the transfer, composed from the
    ``BEE_DMA_*`` field macros.

  The config cell and the handshake slots have dt-bindings macros; compose them
  from named macros instead of writing raw numbers. Two headers are involved:

  - ``<dt-bindings/dma/bee-dma.h>`` provides the ``config`` field macros
    (direction, address adjustment, data width, msize, priority), shared by
    every Bee SoC.
  - ``<dt-bindings/dma/rtl87x2g-dma.h>``, ``<dt-bindings/dma/rtl8752h-dma.h>``
    or ``<dt-bindings/dma/rtl87x2j-dma.h>`` provide the ``BEE_DMA_HANDSHAKE_*``
    slot IDs for that SoC. Each per-SoC header includes ``bee-dma.h``, so
    including the SoC header alone brings in both sets of macros.

  The ``config`` field macros and the bits they set (from
  ``realtek,bee-dma.yaml``) are:

  - bits 0–1, direction: ``BEE_DMA_M2M``, ``BEE_DMA_M2P``, ``BEE_DMA_P2M``
    (P2P is reserved and unsupported).
  - bits 2–3, source address: ``BEE_DMA_SRC_INC``, ``BEE_DMA_SRC_DEC``,
    ``BEE_DMA_SRC_FIXED``.
  - bits 4–5, destination address: ``BEE_DMA_DST_INC``, ``BEE_DMA_DST_DEC``,
    ``BEE_DMA_DST_FIXED``.
  - bits 6–7, source width: ``BEE_DMA_SRC_WIDTH_8BIT``, ``_16BIT``, ``_32BIT``.
  - bits 8–9, destination width: ``BEE_DMA_DST_WIDTH_8BIT``, ``_16BIT``,
    ``_32BIT``.
  - bits 10–12, source msize: ``BEE_DMA_SRC_MSIZE(n)`` where ``n`` is one of
    ``BEE_DMA_MSIZE_1``, ``_4``, ``_8``, ``_16``, ``_32``, ``_64``, ``_128``,
    ``_256``.
  - bits 13–15, destination msize: ``BEE_DMA_DST_MSIZE(n)`` with the same
    ``BEE_DMA_MSIZE_*`` values.
  - bits 16–20, priority: ``BEE_DMA_PRIORITY(n)``, ``n`` from ``0`` to ``9``.

  For example, the UART2 async overlay on rtl87x2g uses:

  .. code-block:: devicetree

     #include <dt-bindings/dma/rtl87x2g-dma.h>

     &uart2 {
         dmas = <&dma0 3 BEE_DMA_HANDSHAKE_UART2_TX
                 (BEE_DMA_M2P | BEE_DMA_SRC_INC | BEE_DMA_DST_FIXED | BEE_DMA_SRC_WIDTH_8BIT |
                  BEE_DMA_DST_WIDTH_8BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_1) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_1) | BEE_DMA_PRIORITY(1))>,
                <&dma0 2 BEE_DMA_HANDSHAKE_UART2_RX
                 (BEE_DMA_P2M | BEE_DMA_SRC_FIXED | BEE_DMA_DST_INC | BEE_DMA_SRC_WIDTH_8BIT |
                  BEE_DMA_DST_WIDTH_8BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_1) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_1) | BEE_DMA_PRIORITY(0))>;
         dma-names = "tx", "rx";
     };

     &dma0 {
         status = "okay";
     };

  Read the two entries as:

  - **tx** — direction M2P, source increment, destination fixed, 8-bit width
    on both, msize 1 on both, priority 1.
  - **rx** — direction P2M, source fixed, destination increment, 8-bit width
    on both, msize 1 on both, priority 0.

  The ``BEE_DMA_HANDSHAKE_UART2_TX`` / ``_RX`` macros resolve to the SoC's own
  slot IDs (``4``/``5`` on rtl87x2g, ``18``/``19`` on rtl8752h and rtl87x2j),
  so the same overlay text is portable by swapping only the included per-SoC
  header. See the :doc:`UART <uart>` chapter for the handshake list.

.. note::

   A peripheral driver reserves its devicetree-assigned channel at init through
   the same allocator ``dma_request_channel()`` uses. Give every peripheral, and
   each direction, its own channel: the second driver to claim an already-taken
   channel fails to initialize and logs the channel number. Because the channel
   is reserved, a memory-to-memory ``dma_request_channel()`` is never handed a
   channel already wired to a peripheral, so application transfers and peripheral
   transfers cannot land on the same channel.

- *How to use*: drive the peripheral through its own Zephyr API (for example the
  UART asynchronous API); the peripheral driver programs and starts the DMA
  channel internally.
- *Examples*: UART + DMA (``tests/drivers/uart/uart_async_api/boards/*``).

Suspend and resume
~~~~~~~~~~~~~~~~~~~
- *How to use*: pause an in-flight transfer with ``dma_suspend()`` and continue
  it with ``dma_resume()``; stop and release the channel with ``dma_stop()``.
- *Example*: ``dma_m2m_loop.test_tst_dma0_m2m_loop_suspend_resume`` and
  ``dma_m2m_loop.test_tst_dma0_m2m_loop_repeated_start_stop``.

Power Management (DLPS)
-----------------------
On a SoC with a PCK600 (such as rtl87x2j), an in-flight transfer is visible to the
power controller through a hardware handshake: while a channel's tx or rx is
active, that DMA and the peripheral it serves cannot auto-sleep, and the system
is held out of sleep as well. No data is lost and the application does not need
to guard the transfer.

On a SoC without a PCK600 (such as rtl87x2g / rtl8752h), the system cannot
detect a running DMA when it enters DLPS, so the DMA is powered off and stops
mid-transfer. The application must manage the DMA run state together with its
DLPS check flag (block DLPS while a transfer is in flight) and must manually
restart the DMA after DLPS exit.

Peripheral drivers that use DMA (for example UART async) re-establish their DMA
channel as part of their own DLPS recovery; see that peripheral's *Power
Management* section.

Samples and Logs
----------------
Sample: ``tests/drivers/dma/loop_transfer``.

How to run
~~~~~~~~~~
- *Wiring*: none — the transfer is memory-to-memory.
- *Extra config*: none for DMA itself — the board overlay enables ``dma0`` and
  the ``.conf`` sets ``CONFIG_DMA_LOOP_TRANSFER_SIZE=4096``. (No heap size is
  needed for DMA; LLIs come from the static pool.)
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/dma/loop_transfer

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/dma/loop_transfer

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/dma/loop_transfer

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-172-g6ba6b2556889 ***
   Running TESTSUITE dma_m2m_loop
   ===================================================================
   START - test_tst_dma0_m2m_loop
   DMA memory to memory transfer started
   Preparing DMA Controller: dma@400012c0
   Starting the transfer on channel 0 and waiting for 1 second
   Each RX buffer should contain the full TX buffer string.
   RX data Loop 0
   RX data Loop 1
   RX data Loop 2
   RX data Loop 3
   Finished DMA: dma@400012c0
    PASS - test_tst_dma0_m2m_loop in 0.285 seconds
   ===================================================================
   START - test_tst_dma0_m2m_loop_repeated_start_stop
   DMA memory to memory transfer started
   Preparing DMA Controller
   Starting the transfer on channel 0 and waiting for 1 second
   Each RX buffer should contain the full TX buffer string.
   RX data Loop 0
   RX data Loop 1
   RX data Loop 2
   RX data Loop 3
   Finished: DMA
    PASS - test_tst_dma0_m2m_loop_repeated_start_stop in 0.282 seconds
   ===================================================================
   START - test_tst_dma0_m2m_loop_suspend_resume
   DMA memory to memory transfer started
   Preparing DMA Controller: dma@400012c0
   Starting the transfer on channel 0 and waiting for 1 second
   suspended after 0 transfers occurred
   resuming after 0 transfers occurred
   Resumed transfers
   Transfer count 4
   Each RX buffer should contain the full TX buffer string.
   RX data Loop 0
   RX data Loop 1
   RX data Loop 2
   RX data Loop 3
   Finished DMA: dma@400012c0
    PASS - test_tst_dma0_m2m_loop_suspend_resume in 0.545 seconds
   ===================================================================
   TESTSUITE dma_m2m_loop succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [dma_m2m_loop]: pass = 3, fail = 0, skip = 0, total = 3 duration = 1.112 seconds
    - PASS - [dma_m2m_loop.test_tst_dma0_m2m_loop] duration = 0.285 seconds
    - PASS - [dma_m2m_loop.test_tst_dma0_m2m_loop_repeated_start_stop] duration = 0.282 seconds
    - PASS - [dma_m2m_loop.test_tst_dma0_m2m_loop_suspend_resume] duration = 0.545 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`UART <uart>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr DMA introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/dma.html>`_
- `Zephyr DMA API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__dma__interface.html>`_
