Counter
=======

The counter driver exposes the Bee hardware timers and the RTC through the
standard Zephyr counter API. Two back-ends are provided: a **counter-timer**,
which turns each hardware timer into a counter, and a **counter-rtc**,
which drives the always-on RTC.

Both back-ends are provided by two unified drivers — the counter-timer
(``counter_bee_timer.c``, compatible ``realtek,bee-counter-timer``) and the
counter-rtc (``counter_bee_rtc.c``, compatible
``realtek,bee-counter-rtc``). The configuration below applies to every SoC
except where a note calls out a per-SoC difference.

The counter-rtc is also the back-end of the Bee RTC driver; see the
:doc:`RTC <rtc>` chapter for details.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Counter-timer: each hardware timer becomes a free-running counter with a
  programmable **top value**. It is declared as a ``counter`` child of a
  ``timer@`` node (compatible ``realtek,bee-timer``, which has basic and
  enhanced variants). The source clock (PCLK, 40 MHz) is divided by the timer
  ``prescaler``.
- Counter-rtc (node ``rtc_counter``, ``channels = 4``): drives the
  always-on RTC and supports **alarms** across four channels. The source clock
  (``src-clk-freq``, the 32 kHz internal clock or a 32.768 kHz external
  crystal) is divided by the ``prescaler``.
- Both back-ends are used through the standard Zephyr counter API.

.. note::

   Match the operation to the back-end. The counter-timer does support a single
   **relative** alarm via ``counter_set_channel_alarm()``, but using it is not
   recommended: the timer has a single compare register shared with its top
   value, so an alarm and ``counter_set_top_value()`` cannot be active at the
   same time (setting one while the other is active returns ``-EBUSY``, and
   absolute alarms return ``-ENOTSUP``); the timer also loses power when the
   system enters DLPS, so an alarm does not survive the transition. Prefer the
   always-on counter-rtc when you need an alarm. The counter-rtc's four channels
   **share a single top value**, so it does not support a programmable top
   value: use only ``counter_set_channel_alarm()`` on it, and
   ``counter_set_top_value()`` returns ``-ENOTSUP`` for any value other than the
   fixed maximum.

Basic Information
~~~~~~~~~~~~~~~~~
- Device nodes:

  - Counter-timer: a ``counter`` child under a ``timerN`` node. The available
    ``timerN`` nodes per SoC are:

    - rtl87x2g: ``timer2``, ``timer3``, ``timer4``, ``timer5``, ``timer6``,
      ``timer7``, ``timer8``, ``timer9``, ``timer10``, ``timer11``.
    - rtl8752h: ``timer2``, ``timer3``, ``timer4``, ``timer5``, ``timer6``,
      ``timer7``.
    - rtl87x2j: ``timer0``, ``timer1``, ``timer2``, ``timer3``, ``timer4``,
      ``timer5``, ``timer6``, ``timer7``, ``timer8``, ``timer9``.
  - Counter-rtc: ``rtc_counter`` (``channels = 4``).
- Bindings files: ``dts/bindings/counter/realtek,bee-counter-timer.yaml``,
  ``dts/bindings/counter/realtek,bee-counter-rtc.yaml``; the parent timer
  binding ``dts/bindings/timer/realtek,bee-timer.yaml`` (with the
  ``realtek,bee-basic-timer.yaml`` / ``realtek,bee-enhanced-timer.yaml``
  variants).
- Kconfig options: ``COUNTER_BEE_TIMER`` (``drivers/counter/Kconfig.bee_timer``)
  and ``COUNTER_BEE_RTC`` (``drivers/counter/Kconfig.bee_rtc``).
- Source files: ``drivers/counter/counter_bee_timer.c``,
  ``drivers/counter/counter_bee_rtc.c``.
- Example board files:
  ``tests/drivers/counter/counter_basic_api/boards/`` — per-SoC overlays for
  ``rtl87x2g_evb_a/rtl8762gku``, ``rtl8752h_evb/rtl8752hjl`` and
  ``rtl87x2j_evb/rtl8762jth``.
- Reference:

  - `Zephyr counter introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/counter.html>`_
  - `Zephyr counter API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__counter__interface.html>`_

Operation Flow
--------------

Enabling a counter device
~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the node(s) you want to use in your overlay. For
  the counter-timer, set ``status = "okay"`` on the ``timer@`` node and on its
  ``counter`` child; the counting resolution is the 40 MHz PCLK divided by the
  timer ``prescaler`` (default ``1``; the accepted values differ per SoC — see
  the note below). For the counter-rtc, enable
  ``rtc_counter``; its ``src-clk-freq`` selects the source clock — ``32000``
  (default, internal 32 kHz) or ``32768`` (external crystal) — divided by a
  ``prescaler`` in the range ``1``–``4096``.

  See ``dts/bindings/counter/realtek,bee-counter-timer.yaml`` and
  ``dts/bindings/counter/realtek,bee-counter-rtc.yaml`` for more details.

  .. code-block:: devicetree

     &timer0 {
         prescaler = <1>;
         status = "okay";

         counter {
             status = "okay";
         };
     };

     &rtc_counter {
         prescaler = <1>;
         status = "okay";
     };

  Enable counter support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_COUNTER=y

- *How to use*: start the counter with ``counter_start()`` and stop it with
  ``counter_stop()``; read the current value with ``counter_get_value()``.
  Convert between microseconds and ticks with ``counter_us_to_ticks()`` /
  ``counter_get_frequency()``.
- *Example*: the ``&timer*`` and ``&rtc_counter`` nodes in the per-SoC overlays
  under ``tests/drivers/counter/counter_basic_api/boards/``.

.. note::

   The accepted ``prescaler`` values differ per SoC: ``1``, ``2``, ``4``, ``8``,
   ``16``, ``32``, ``40``, ``64`` on rtl87x2g and rtl87x2j; ``1``, ``2``, ``4``,
   ``8``, ``40``, ``125`` on rtl8752h. See
   ``dts/bindings/timer/realtek,bee-timer.yaml``.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: the counter-timer is the
   ``timer@`` node itself (compatible ``realtek,bee-timer``), not a ``counter``
   child of it. Enable it directly on the timer node, with no nested ``counter``
   node:

   .. code-block:: devicetree

      &timer2 {
          prescaler = <1>;
          status = "okay";
      };

Setting the top value (counter-timer)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: on the counter-timer, program the wrap point with
  ``counter_set_top_value()`` (passing a ``struct counter_top_cfg`` with the
  ticks and an optional callback). The callback fires each time the counter
  wraps. Do not call it on the counter-rtc: its channels share one top
  value, so the top value is fixed at the maximum and
  ``counter_set_top_value()`` returns ``-ENOTSUP`` for any other value.
- *Example*: ``counter_basic.test_set_top_value_with_alarm`` (with callback) and
  ``counter_no_callback.test_set_top_value_without_alarm`` (no callback).

Setting an alarm (counter-rtc)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: on the counter-rtc, arm a one-shot alarm with
  ``counter_set_channel_alarm()``, passing a ``struct counter_alarm_cfg`` with
  the target ticks and a callback; cancel it with
  ``counter_cancel_channel_alarm()``. Both relative and absolute alarms are
  supported, across the four channels.
- *Example*: single-shot, relative, multiple, late, and cancelled alarms are
  covered by ``counter_basic.test_single_shot_alarm_notop``,
  ``counter_basic.test_short_relative_alarm``,
  ``counter_basic.test_multiple_alarms``, ``counter_basic.test_late_alarm`` and
  ``counter_basic.test_cancelled_alarm_does_not_expire``.

Power Management (DLPS)
-----------------------
- *Behavior*: A running hardware timer and the always-on RTC still behave
  differently across DLPS.
- *Counter-timer*: while a counter-timer is running the system does not enter
  DLPS, so the timer keeps counting and its top-value callback fires normally.
- *Counter-rtc*: the RTC is in the always-on domain, so it keeps running
  across DLPS; an armed alarm wakes the system when it expires. Use the RTC when
  you need a timekeeping or wakeup source that survives low-power stop periods.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so a running hardware timer does not keep the system awake. If the
   system enters DLPS while a counter-timer is running, the hardware timer loses
   power and cannot be restored on exit, so the application must keep the system
   out of DLPS (for example with a DLPS check flag) while it uses the
   counter-timer. The always-on RTC is unaffected: it keeps running across DLPS
   and wakes the system when an alarm expires.

Samples and Logs
----------------
Sample: ``tests/drivers/counter/counter_basic_api``.

How to run
~~~~~~~~~~
- *Wiring*: none.
- *Extra config*: none — the per-SoC board overlay enables the counter node(s).
  Note that the full suite runs long enough to trigger a watchdog reset, so
  disable the watchdog (call ``WDG_Disable()``) before running it if necessary.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/counter/counter_basic_api

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/counter/counter_basic_api

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/counter/counter_basic_api

     west flash --port <your-flash-serial-port>

  The matching overlay under ``boards/`` selects that board's counter nodes.

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-172-g6ba6b2556889 ***
   Running TESTSUITE counter_basic
   ===================================================================
   START - test_all_channels
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing rtc@40000768
    PASS - test_all_channels in 1.451 seconds
   ===================================================================
   START - test_cancelled_alarm_does_not_expire
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for rtc@40000768
    SKIP - test_cancelled_alarm_does_not_expire in 1.123 seconds
   ===================================================================
   START - test_late_alarm
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for rtc@40000768
    SKIP - test_late_alarm in 1.124 seconds
   ===================================================================
   START - test_late_alarm_error
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for rtc@40000768
    SKIP - test_late_alarm_error in 1.123 seconds
   ===================================================================
   START - test_multiple_alarms
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Skipped for counter
   Testing rtc@40000768
   E: Unspported set top value
    PASS - test_multiple_alarms in 1.126 seconds
   ===================================================================
   START - test_set_top_value_with_alarm
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   E: Unspported set top value
   Skipped for rtc@40000768
    PASS - test_set_top_value_with_alarm in 2.214 seconds
   ===================================================================
   START - test_short_relative_alarm
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   E: Alarm ticks too short
   Skipped for counter
   Testing rtc@40000768
    PASS - test_short_relative_alarm in 1.257 seconds
   ===================================================================
   START - test_single_shot_alarm_notop
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing rtc@40000768
    PASS - test_single_shot_alarm_notop in 1.891 seconds
   ===================================================================
   START - test_single_shot_alarm_top
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   Testing counter
   E: Alarm ticks exceed top ticks
   E: Unspported set top value
   Skipped for rtc@40000768
    PASS - test_single_shot_alarm_top in 1.853 seconds
   ===================================================================
   START - test_valid_function_without_alarm
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing rtc@40000768
    PASS - test_valid_function_without_alarm in 1.443 seconds
   ===================================================================
   TESTSUITE counter_basic succeeded
   Running TESTSUITE counter_no_callback
   ===================================================================
   START - test_set_top_value_without_alarm
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   Testing counter
   E: Unspported set top value
   Skipped for rtc@40000768
    PASS - test_set_top_value_without_alarm in 1.174 seconds
   ===================================================================
   TESTSUITE counter_no_callback succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [counter_basic]: pass = 7, fail = 0, skip = 3, total = 10 duration = 14.605 seconds
    - PASS - [counter_basic.test_all_channels] duration = 1.451 seconds
    - SKIP - [counter_basic.test_cancelled_alarm_does_not_expire] duration = 1.123 seconds
    - SKIP - [counter_basic.test_late_alarm] duration = 1.124 seconds
    - SKIP - [counter_basic.test_late_alarm_error] duration = 1.123 seconds
    - PASS - [counter_basic.test_multiple_alarms] duration = 1.126 seconds
    - PASS - [counter_basic.test_set_top_value_with_alarm] duration = 2.214 seconds
    - PASS - [counter_basic.test_short_relative_alarm] duration = 1.257 seconds
    - PASS - [counter_basic.test_single_shot_alarm_notop] duration = 1.891 seconds
    - PASS - [counter_basic.test_single_shot_alarm_top] duration = 1.853 seconds
    - PASS - [counter_basic.test_valid_function_without_alarm] duration = 1.443 seconds

   SUITE PASS - 100.00% [counter_no_callback]: pass = 1, fail = 0, skip = 0, total = 1 duration = 1.174 seconds
    - PASS - [counter_no_callback.test_set_top_value_without_alarm] duration = 1.174 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`RTC <rtc>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr counter introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/counter.html>`_
- `Zephyr counter API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__counter__interface.html>`_
