PWM
===

The PWM driver exposes the Bee timer-based PWM outputs through the standard
Zephyr PWM API. Each PWM output is a **child node of a hardware timer**, which
makes it suitable for general-purpose PWM generation.

The driver has a second back-end, a low-power PWM (LPPWM, ``pwm_bee_lppwm.c``,
compatible ``realtek,bee-lppwm``), used through the same Zephyr PWM API. The
LPPWM lives in the always-on domain and keeps toggling its single output while
the SoC is in a low-power sleep state.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Timer-based PWM: each output is a child of a hardware ``timerN`` node.

  - Driven by a 40 MHz clock; the resolution is 40 MHz divided by the parent
    timer's ``prescaler``.
  - Period and pulse width set directly in timer cycles; the source clock rate
    is reported for cycle-to-time conversion.
  - Active-level (normal / inverted) polarity selection.
  - ``#pwm-cells = <3>`` — the PWM specifier carries ``channel``, ``period`` and
    ``flags``.
  - Capture, latch count, deadzone and DMA are **not** supported yet.
- Low-power PWM (LPPWM): a single always-on output channel (CH0) that keeps
  toggling while the SoC is in a low-power sleep state.

  - Driven by the always-on 32 kHz clock; no prescaler.
  - Period and pulse width set directly in 32 kHz cycles; the high and low
    counts are each 16-bit (``0``–``65535``).
  - Normal / inverted output polarity, as for the timer-based PWM.
- Both back-ends are used through the standard Zephyr PWM API.

Basic Information
~~~~~~~~~~~~~~~~~
- Device nodes:

  - Timer-based PWM: a ``pwm`` child under a ``timerN`` node. The available
    ``timerN`` nodes per SoC are:

    - rtl87x2g: ``timer2``, ``timer3``, ``timer4``, ``timer5``, ``timer6``,
      ``timer7``, ``timer8``, ``timer9``, ``timer10``, ``timer11``.
    - rtl8752h: ``timer2``, ``timer3``, ``timer4``, ``timer5``, ``timer6``,
      ``timer7``.
    - rtl87x2j: ``timer0``, ``timer1``, ``timer2``, ``timer3``, ``timer4``,
      ``timer5``, ``timer6``, ``timer7``, ``timer8``, ``timer9``.
  - LPPWM: ``lppwm``.
- Bindings files: ``dts/bindings/pwm/realtek,bee-pwm.yaml``
  (compatible ``realtek,bee-pwm``) and
  ``dts/bindings/pwm/realtek,bee-lppwm.yaml`` (compatible
  ``realtek,bee-lppwm``).
- Kconfig options: ``PWM_BEE`` and ``PWM_BEE_LPPWM``
  (``drivers/pwm/Kconfig.bee``).
- Source files: ``drivers/pwm/pwm_bee.c``, ``drivers/pwm/pwm_bee_lppwm.c``.
- Example board files: ``tests/drivers/pwm/pwm_api/boards/`` (one overlay per
  board target).
- Reference:

  - `Zephyr PWM introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/pwm.html>`_
  - `Zephyr PWM API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__pwm__interface.html>`_

Operation Flow
--------------

Enabling a PWM device
~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the parent timer node and its ``pwm`` child, and
  route the output pad through the timer's pinctrl group (see
  :doc:`Pinctrl <pinctrl>`). The parent timer's source clock is 40 MHz and the
  PWM resolution is that clock divided by the timer's ``prescaler``. The parent
  may be a basic or an enhanced timer; the driver selects the matching PWM
  block automatically from the parent timer's ``compatible``. The child is a
  single **anonymous** ``pwm`` node.

  .. code-block:: devicetree

     &timer2 {
         status = "okay";
         prescaler = <1>;

         pwm2: pwm {
             status = "okay";
             pinctrl-0 = <&pwm2_default>;
             pinctrl-names = "default";
         };
     };

     &pinctrl {
         pwm2_default: pwm2_default {
             group1 {
                 psels = <BEE_PSEL(PWM2, P0_2)>;
                 output-enable;
                 output-low;
                 bias-pull-down;
             };
         };
     };

  Enable PWM support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_PWM=y

  .. note::

     On rtl87x2g / rtl8752h, the v3.7 driver differs here: the ``pwm`` child is a
     **named** ``pwmN`` node, and its binding adds a boolean ``is-enhanced``
     property (selects the enhanced-timer PWM block) and a ``channels`` property
     (the number of PWM output channels):

     .. code-block:: devicetree

        &timer2 {
            status = "okay";
            prescaler = <1>;

            pwm2: pwm2 {
                status = "okay";
                is-enhanced;
                channels = <1>;
                pinctrl-0 = <&pwm2_default>;
                pinctrl-names = "default";
            };
        };

- *Examples*: the ``&timer2`` / ``pwm2`` nodes on P0_1 in the rtl87x2g overlay,
  the ``&timer6`` / ``pwm6`` nodes on P0_1 in the rtl8752h overlay, and the
  ``&timer2`` / ``pwm2`` nodes on P0_2 in the rtl87x2j overlay, all under
  ``tests/drivers/pwm/pwm_api/boards/``.

Setting period and pulse
~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: call ``pwm_set_cycles()`` to program the period and pulse width
  in timer cycles; use ``pwm_get_cycles_per_sec()`` to learn the clock rate so
  you can convert a time to cycles. Pass ``PWM_POLARITY_NORMAL`` /
  ``PWM_POLARITY_INVERTED`` in the flags to choose the active level.

  .. code-block:: c

     uint64_t rate;

     /* Clock rate (40 MHz), then 50% duty on channel 0. */
     pwm_get_cycles_per_sec(pwm, 0, &rate);
     pwm_set_cycles(pwm, 0, 64000, 32000, PWM_POLARITY_NORMAL);

- *Example*: ``pwm_basic.test_pwm_cycle`` (cycle API) and
  ``pwm_basic.test_pwm_nsec`` (time-based API layered on ``pwm_set_cycles()``).

Using the low-power PWM (LPPWM)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the ``lppwm`` node and route its single output
  (``LPPWM_CH0``) through pinctrl (see :doc:`Pinctrl <pinctrl>`). The LPPWM is
  clocked from the always-on 32 kHz clock and has no prescaler.

  .. code-block:: devicetree

     &lppwm {
         status = "okay";
         pinctrl-0 = <&lppwm_default>;
         pinctrl-names = "default";
     };

     &pinctrl {
         lppwm_default: lppwm_default {
             group1 {
                 psels = <BEE_PSEL(LPPWM_CH0, P0_1)>;
                 output-enable;
                 output-low;
                 bias-disable;
             };
         };
     };

  Enable PWM support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_PWM=y

- *How to use*: drive the output with ``pwm_set_cycles()`` on channel 0; the
  LPPWM exposes a single channel, so any other channel number returns
  ``-EINVAL``. ``pwm_get_cycles_per_sec()`` reports ``32000`` (the always-on
  32 kHz clock), so program the period and pulse in 32 kHz cycles. The high
  count (pulse) and the low count (period minus pulse) are each 16-bit, so both
  must be ``<= 65535``. Pass ``PWM_POLARITY_NORMAL`` / ``PWM_POLARITY_INVERTED``
  to choose the active level.

  .. code-block:: c

     uint64_t rate;

     /* 32 kHz clock, then a 5 ms period (160 cycles) with a 1 ms high time. */
     pwm_get_cycles_per_sec(lppwm, 0, &rate);   /* rate == 32000 */
     pwm_set_cycles(lppwm, 0, 160, 32, PWM_POLARITY_NORMAL);

- *Example*: point the ``pwm-test`` alias in a ``pwm_api`` board overlay at the
  ``&lppwm`` node and route its output with ``&pinctrl`` to exercise the LPPWM
  through the standard PWM test cases.

Power Management (DLPS)
-----------------------
- *Behavior*: the timer-based PWM and the always-on LPPWM behave differently
  across DLPS.
- *Timer-based PWM*: on a SoC with a PCK600 (such as rtl87x2j), a running
  hardware timer keeps the system awake, so while the PWM is actively driving a
  waveform the system cannot enter DLPS. To let the system sleep, set the duty
  cycle to 0 or 100 % with ``pwm_set_cycles()``: at those two settings the
  output is a constant level, so the driver stops the hardware timer and holds
  that level on the output pad (parked in ``SW_MODE``) instead. With the timer
  stopped the system is free to enter DLPS, and the pad keeps driving the level
  across sleep.
- *LPPWM*: the LPPWM is in the always-on domain, so it keeps toggling across
  DLPS and does not keep the system awake.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so the driver implements a full ``PM_DEVICE`` DLPS handler instead. A
   running hardware timer does not keep the system awake, so a running PWM does
   not block DLPS; if the system enters DLPS while the PWM is active, the waveform
   stops immediately. On DLPS entry the driver parks the output pad: if a
   ``sleep`` pinctrl state is configured for the pad it applies that state,
   otherwise it holds the pad at the level closest to the current output — low
   when the duty cycle is ``<= 50 %`` and high when it is ``> 50 %``. On DLPS exit
   it restores the pre-sleep frequency and duty cycle, so PWM can continue to be
   used after wakeup with no extra handling.

Samples and Logs
----------------
Sample: ``tests/drivers/pwm/pwm_api``.

How to run
~~~~~~~~~~
- *Wiring*: none required for the api test; to observe the waveform, probe the
  PWM output pad with a scope or logic analyzer. The actual pad is set by the
  board overlay and differs per SoC.
- *Extra config*: none — the board overlay enables the timer and its ``pwm``
  child and sets the output pinctrl.
- *Build & flash*: the overlay is selected by board target, one per SoC:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/pwm/pwm_api

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/pwm/pwm_api

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/pwm/pwm_api

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-174-g8855e3ecc827 ***
   Running TESTSUITE pwm_basic
   ===================================================================
   START - test_pwm_cycle
   [PWM]: 0, [period]: 64000, [pulse]: 32000
   [PWM]: 0, [period]: 64000, [pulse]: 64000
   [PWM]: 0, [period]: 64000, [pulse]: 0
    PASS - test_pwm_cycle in 3.012 seconds
   ===================================================================
   START - test_pwm_nsec
   [PWM]: 0, [period]: 2000000, [pulse]: 1000000
   [PWM]: 0, [period]: 2000000, [pulse]: 2000000
   [PWM]: 0, [period]: 2000000, [pulse]: 0
    PASS - test_pwm_nsec in 3.013 seconds
   ===================================================================
   TESTSUITE pwm_basic succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [pwm_basic]: pass = 2, fail = 0, skip = 0, total = 2 duration = 6.025 seconds
    - PASS - [pwm_basic.test_pwm_cycle] duration = 3.012 seconds
    - PASS - [pwm_basic.test_pwm_nsec] duration = 3.013 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr PWM introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/pwm.html>`_
- `Zephyr PWM API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__pwm__interface.html>`_
