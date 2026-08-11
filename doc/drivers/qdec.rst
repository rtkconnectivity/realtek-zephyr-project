QDEC
====

The quadrature decoder driver exposes the Bee QDEC peripheral as a
Zephyr **sensor**. It counts the edges of an A/B encoder and reports a raw
count; the application converts that count into the physical quantity it
represents (for example degrees of rotation).

All three SoCs (rtl87x2g, rtl8752h, rtl87x2j) expose the QDEC through the same
Zephyr sensor API.

.. note::

   Each SoC drives a different QDEC hardware variant, selected by the
   devicetree ``compatible`` on the ``qdec`` node. A single ``qdec_bee`` driver
   covers all three hardware variants:

   - **rtl87x2g** — AON QDEC, **X axis only**, compatible ``realtek,bee-aon-qdec``.
   - **rtl8752h** — basic QDEC, **X / Y / Z axes**, compatible
     ``realtek,bee-basic-qdec``.
   - **rtl87x2j** — LP QDEC, **X axis only** (uses the LPQDEC clock), compatible
     ``realtek,bee-lpqdec``.

   The custom sensor channels used to read each axis raw count are declared in
   ``include/zephyr/drivers/sensor/qdec_bee.h``.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: the QDEC is provided by
   separate drivers instead of the unified ``qdec_bee`` driver — the AON variant
   (rtl87x2g) is the ``aon_qdec_bee`` driver with compatible
   ``realtek,bee-aon_qdec``, and the basic variant (rtl8752h) is the ``qdec_bee``
   driver with compatible ``realtek,bee-qdec``.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Decodes one or more A/B quadrature channels (X only on rtl87x2g and
  rtl87x2j; X/Y/Z on rtl8752h).
- Exposed through the Zephyr sensor API: fetch a measurement, read the per-axis
  count, and register a data-ready callback.
- Returns a **raw count**; the application scales it to physical units.
- Bee-specific per-axis raw-count channels are declared in
  ``include/zephyr/drivers/sensor/qdec_bee.h``.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``qdec``.
- Bindings files: ``dts/bindings/sensor/realtek,bee-basic-qdec.yaml``,
  ``dts/bindings/sensor/realtek,bee-aon-qdec.yaml``,
  ``dts/bindings/sensor/realtek,bee-lpqdec.yaml``.
- Kconfig option: ``QDEC_BEE`` (``drivers/sensor/realtek/qdec_bee/Kconfig``).
- Source file: ``drivers/sensor/realtek/qdec_bee/qdec_bee.c``.
- Extension channels header:
  ``include/zephyr/drivers/sensor/qdec_bee.h``.
- Example board files:
  ``samples/sensor/qdec/boards/rtl87x2j_evb_rtl8762jth.overlay``.
- Reference:

  - `Zephyr sensor introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/index.html>`_
  - `Zephyr sensor API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__sensor__interface.html>`_

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: it uses the bindings
   ``dts/bindings/sensor/realtek,bee-qdec.yaml`` (rtl8752h) and
   ``dts/bindings/sensor/realtek,bee-aon_qdec.yaml`` (rtl87x2g), and the AON
   variant (rtl87x2g) is a separate ``aon_qdec_bee`` driver rather than the
   unified ``qdec_bee.c`` source.

Operation Flow
--------------

Enabling a QDEC device
~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the ``qdec`` node and route its A/B pads through
  pinctrl. Enable each axis you want and, optionally, tune its decoding; each
  property exists in an ``x``-, ``y``- and ``z``-axis form (``y`` and ``z`` only
  on the basic QDEC / rtl8752h):

  - ``<axis>-enable``: enable decoding of that axis.
  - ``<axis>-counts-per-revolution``: hardware decoding mode used to derive
    revolutions from the raw counter value. Only ``2`` or ``4`` are accepted;
    ``4`` (default) is the highest-resolution X4 mode.
  - ``<axis>-debounce-time-ms``: debounce time applied to that axis, in
    milliseconds.

  The example below is for rtl87x2j (LP QDEC); the pinctrl signal names differ
  per hardware variant (``LPQDEC_PHA`` / ``LPQDEC_PHB`` on rtl87x2j, the
  corresponding QDEC phase signals on the other variants):

  .. code-block:: devicetree

     &qdec {
         status = "okay";
         pinctrl-0 = <&qdec_default>;
         pinctrl-names = "default";
         x-enable;
         x-counts-per-revolution = <4>;
         x-debounce-time-ms = <5>;
     };

     &pinctrl {
         qdec_default: qdec_default {
             group1 {
                 psels = <BEE_PSEL(LPQDEC_PHA, P3_4)>,
                         <BEE_PSEL(LPQDEC_PHB, P3_5)>;
                 output-disable;
                 bias-pull-up;
             };
         };
     };

  Enable sensor support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_SENSOR=y

- *Example*: the ``&qdec`` node in
  ``samples/sensor/qdec/boards/rtl87x2j_evb_rtl8762jth.overlay``.

Reading the count
~~~~~~~~~~~~~~~~~~
- *How to use*: call ``sensor_sample_fetch()`` to latch the current count, then
  ``sensor_channel_get()`` to read it back. Use ``SENSOR_CHAN_ROTATION`` for the
  enabled axis, or the Bee per-axis raw-count channels
  (``SENSOR_CHAN_QDEC_X_COUNT`` / ``_Y_COUNT`` / ``_Z_COUNT``) from
  ``include/zephyr/drivers/sensor/qdec_bee.h``. Convert the raw count to your
  physical unit in the application.

  .. code-block:: c

     struct sensor_value val;

     sensor_sample_fetch(qdec);
     sensor_channel_get(qdec, SENSOR_CHAN_ROTATION, &val);
     /* val.val1 is the raw count; scale it to degrees in the application */

- *Example*: the periodic position read in ``samples/sensor/qdec``, shown in the
  log below.

.. note::

   On rtl87x2g, the v3.7 driver differs here: for ``SENSOR_CHAN_ROTATION`` the
   AON QDEC returns an angle in degrees rather than the raw count. It computes
   ``FULL_ANGLE / counts-per-revolution * count`` (``count * 90`` at the default
   X4 mode), so the application converts it back to a count if needed. rtl8752h
   returns the raw count directly.

Data-ready trigger
~~~~~~~~~~~~~~~~~~~
- *How to use*: register a ``SENSOR_TRIG_DATA_READY`` trigger with
  ``sensor_trigger_set()`` to be called when new counts arrive; pass the axis in
  the trigger's ``chan`` field. Passing a ``NULL`` handler disarms the trigger.

  .. code-block:: c

     struct sensor_trigger trig = {
         .type = SENSOR_TRIG_DATA_READY,
         .chan = SENSOR_CHAN_ROTATION,
     };

     sensor_trigger_set(qdec, &trig, data_ready_handler);

- *Example*: the driver enables the new-data and illegal-status interrupts when a
  handler is registered (see ``qdec_bee_trigger_set()`` in
  ``drivers/sensor/realtek/qdec_bee/qdec_bee.c``).

Power Management (DLPS)
-----------------------
- *Behavior*: on a SoC with a PCK600 (such as rtl87x2j), the QDEC may stay in
  DLPS while operating and wakes automatically on a phase change to resume
  counting; this is transparent to the application.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: rtl87x2g uses the
   always-on (AON) QDEC and has no ``PM_DEVICE`` handler; rtl8752h uses the
   basic QDEC, which loses power in DLPS, so its driver implements a full
   ``PM_DEVICE`` DLPS handler that accumulates the count on suspend and restores
   the registers on resume. On both SoCs, however, a phase change that occurs
   while the system is in DLPS is not detected, so the application must keep the
   system out of DLPS while it is using the QDEC.

Samples and Logs
----------------
Sample: ``samples/sensor/qdec``.

How to run
~~~~~~~~~~
- *Wiring*: the sample includes a GPIO-based quadrature-encoder emulator, so no
  external encoder is required. Connect the emulator outputs (``phase_a`` /
  ``phase_b``) to the QDEC A/B input pads. The actual pads are set by the
  board overlay and differ per SoC.
- *Extra config*: none beyond the board overlay, which enables ``qdec`` and the
  encoder emulator.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku samples/sensor/qdec

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   samples/sensor/qdec

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   samples/sensor/qdec

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-174-g8855e3ecc827 ***
   Quadrature decoder sensor test
   Quadrature encoder emulator enabled with 100 ms period
   Position = 20 degrees
   Position = 40 degrees
   Position = 60 degrees

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr sensor introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/index.html>`_
- `Zephyr sensor API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__sensor__interface.html>`_
