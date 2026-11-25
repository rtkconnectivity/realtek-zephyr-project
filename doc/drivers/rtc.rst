RTC
===

The RTC driver exposes the Bee real-time clock through the standard
Zephyr RTC API. It provides calendar time (set and get) and alarms with
callbacks. On the Bee family (rtl87x2j, rtl87x2g, rtl8752h) the RTC API is
served by the generic upstream driver ``drivers/rtc/rtc_counter.c`` (compatible
``zephyr,rtc-counter``), layered on top of the RTC-based counter
(``realtek,bee-counter-rtc``, node ``rtc_counter``, 4 channels — see the
:doc:`Counter <counter>` chapter). Alarms map onto the 4 counter channels.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Set and get calendar time.
- Alarms with callbacks, built on the RTC-based counter, so the alarms map onto
  the 4 counter channels — see the :doc:`Counter <counter>` chapter.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: a ``zephyr,rtc-counter`` child of the ``rtc_counter`` node,
  added in the application overlay (the SoC devicetree does not define it).
- Bindings files: ``dts/bindings/rtc/zephyr,rtc-counter.yaml`` for the child,
  over the RTC-based counter binding
  ``dts/bindings/counter/realtek,bee-counter-rtc.yaml``.
- Kconfig options: ``CONFIG_RTC`` enables the RTC subsystem;
  ``RTC_COUNTER`` (``drivers/rtc/Kconfig.counter``) is enabled by default once
  a ``zephyr,rtc-counter`` node is enabled, and selects ``COUNTER``;
  ``CONFIG_RTC_ALARM`` enables alarms.
- Source file: ``drivers/rtc/rtc_counter.c``.
- Example board files: per-SoC overlays and configs under
  ``tests/drivers/rtc/rtc_api/boards/`` —
  ``rtl87x2j_evb_rtl8762jth.{overlay,conf}``,
  ``rtl87x2g_evb_a_rtl8762gku.{overlay,conf}`` and
  ``rtl8752h_evb_rtl8752hjl.{overlay,conf}``.
- Reference:

  - `Zephyr RTC introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/rtc.html>`_
  - `Zephyr RTC API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__rtc__interface.html>`_

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: instead of the generic
   ``rtc_counter`` path it provides a dedicated RTC driver
   ``drivers/rtc/rtc_bee.c`` (compatible ``realtek,bee-rtc``, node ``rtc``,
   binding ``dts/bindings/counter/realtek,bee_rtc.yaml``, Kconfig
   ``CONFIG_RTC_BEE=y`` in addition to ``CONFIG_RTC=y``) that implements the
   Zephyr RTC API directly. The RTC is always-on and keeps running across DLPS,
   so under ``CONFIG_PM_DEVICE`` the driver only arms it as a DLPS wakeup source
   (no register save/restore is needed); it also offers a per-second update
   callback (``rtc_update_set_callback()``). That dedicated driver and the
   RTC-counter back-end share the one hardware RTC and cannot both be enabled.

Operation Flow
--------------

Enabling an RTC device
~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the ``rtc_counter`` node and add a
  ``zephyr,rtc-counter`` child to it; that child is the RTC device. Its
  ``alarms-count`` sets how many alarms the RTC exposes, up to the 4 counter
  channels; without it the RTC has no alarms. The generic ``rtc_counter``
  driver provides the RTC API on top of the RTC-based counter.

  See ``dts/bindings/rtc/zephyr,rtc-counter.yaml`` for more details.

  .. code-block:: devicetree

     / {
         aliases {
             rtc = &rtc;
         };
     };

     &rtc_counter {
         status = "okay";
         prescaler = <1>;

         rtc: rtc {
             compatible = "zephyr,rtc-counter";
             alarms-count = <4>;
             status = "okay";
         };
     };

  Enable RTC support in ``prj.conf``. The RTC must initialize after the
  counter, so ``CONFIG_RTC_INIT_PRIORITY`` must be greater than
  ``CONFIG_COUNTER_INIT_PRIORITY`` (default ``60``), or the build fails:

  .. code-block:: kconfig

     CONFIG_RTC=y
     CONFIG_RTC_ALARM=y
     CONFIG_RTC_INIT_PRIORITY=70

- *Example*: the ``rtc_counter`` node with its ``rtc`` child in
  ``tests/drivers/rtc/rtc_api/boards/rtl87x2j_evb_rtl8762jth.overlay``, and
  ``CONFIG_RTC_ALARM`` / ``CONFIG_RTC_INIT_PRIORITY`` in the matching
  ``rtl87x2j_evb_rtl8762jth.conf``. The rtl87x2g and rtl8752h files in the
  same directory are identical.

Setting and reading time
~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: set the clock with ``rtc_set_time()`` (a ``struct rtc_time``)
  and read it back with ``rtc_get_time()``.
- *Example*: ``rtc_api.test_set_get_time``, ``rtc_api.test_time_counting`` and
  ``rtc_api.test_y2k``.

Alarms
~~~~~~
- *How to use*: configure an alarm with ``rtc_alarm_set_time()`` and register
  its callback with ``rtc_alarm_set_callback()``. The generic ``rtc_counter``
  driver provides alarms through the channels of the underlying RTC-based
  counter, one per alarm up to ``alarms-count``; query the supported fields
  with ``rtc_alarm_get_supported_fields()`` before arming an alarm. Every field
  from second through year day is supported. The per-second update callback
  is not supported: ``rtc_update_set_callback()`` returns ``-ENOTSUP``.
- *Example*: ``rtc_api.test_alarm`` and ``rtc_api.test_alarm_callback``.

Power Management (DLPS)
-----------------------
- *Behavior*: the RTC is in the always-on domain, so it keeps running
  across DLPS; an armed alarm wakes the system when it expires.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, but the RTC is in the always-on domain, so it keeps running across
   DLPS and needs no register save/restore. Under ``CONFIG_PM_DEVICE`` the driver
   arms the RTC overflow and alarms as DLPS wakeup sources. DLPS handling is
   transparent to the application: time and alarms continue to work after
   wakeup with no extra handling.

Samples and Logs
----------------
Sample: ``tests/drivers/rtc/rtc_api``.

How to run
~~~~~~~~~~
- *Wiring*: none.
- *Extra config*: none — the board overlay enables the ``rtc_counter`` node
  and its ``rtc`` child, and the board ``.conf`` enables alarms and sets the
  RTC init priority. Note that the full suite runs long enough to trigger a
  watchdog reset, so disable the watchdog (call ``WDG_Disable()``) before
  running it if necessary.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/rtc/rtc_api

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/rtc/rtc_api

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/rtc/rtc_api

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-174-g8855e3ecc827 ***
   Running TESTSUITE rtc_api
   ===================================================================
   START - test_alarm
    PASS - test_alarm in 26.003 seconds
   ===================================================================
   START - test_alarm_callback
    PASS - test_alarm_callback in 26.003 seconds
   ===================================================================
   START - test_set_get_time
    PASS - test_set_get_time in 0.001 seconds
   ===================================================================
   START - test_time_counting
    PASS - test_time_counting in 10.006 seconds
   ===================================================================
   START - test_y2k
    PASS - test_y2k in 2.001 seconds
   ===================================================================
   TESTSUITE rtc_api succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [rtc_api]: pass = 5, fail = 0, skip = 0, total = 5 duration = 64.014 seconds
    - PASS - [rtc_api.test_alarm] duration = 26.003 seconds
    - PASS - [rtc_api.test_alarm_callback] duration = 26.003 seconds
    - PASS - [rtc_api.test_set_get_time] duration = 0.001 seconds
    - PASS - [rtc_api.test_time_counting] duration = 10.006 seconds
    - PASS - [rtc_api.test_y2k] duration = 2.001 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Counter <counter>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr RTC introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/rtc.html>`_
- `Zephyr RTC API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__rtc__interface.html>`_
