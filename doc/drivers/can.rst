CAN
===

The CAN driver exposes the Bee CAN controller through the standard Zephyr CAN
API.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Classic CAN 2.0A/2.0B with 11-bit standard and 29-bit extended identifiers.
- Normal, internal loopback, listen-only, and one-shot mode support.
- 16 hardware message buffers shared between transmit and receive; the split is
  set by ``CONFIG_CAN_REALTEK_BEE_TX_MSG_BUF_NUM`` and
  ``CONFIG_CAN_REALTEK_BEE_RX_MSG_BUF_NUM`` (which must sum to at most 16).
- Per-filter receive: each receive message buffer matches one standard or
  extended identifier under a mask.
- Interrupt-driven transmit and receive with per-buffer completion callbacks.
- Bus-state and error-counter reporting, with a state-change callback.
- Receive-timestamp support (``CONFIG_CAN_RX_TIMESTAMP``), with the timestamp
  clock divider set by the ``rx-timestamp-prescaler`` devicetree property.
- CAN statistics support (``CONFIG_CAN_STATS``).
- Remote-frame acceptance support (``CONFIG_CAN_ACCEPT_RTR``).

.. note::

   The controller is classic CAN only; it does not support CAN FD. A CAN FD test
   suite built against it is skipped at run time.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``can`` (referenced by boards through ``chosen`` property
  ``zephyr,canbus``).
- Bindings file: ``dts/bindings/can/realtek,bee-can.yaml``
  (compatible ``realtek,bee-can``).
- Kconfig options: ``CAN_REALTEK_BEE``, ``CAN_REALTEK_BEE_TX_MSG_BUF_NUM``
  (default 6), ``CAN_REALTEK_BEE_RX_MSG_BUF_NUM`` (default 10)
  (``drivers/can/Kconfig.realtek_bee``).
- Source file: ``drivers/can/can_realtek_bee.c``.
- Example board files:
  ``boards/realtek/rtl87x2g_evb_a/rtl87x2g_evb_a_rtl8762gku.dts`` and
  ``boards/realtek/rtl87x2j_evb/rtl87x2j_evb_rtl8762jth.dts`` (each enables
  ``&can`` and selects it as ``zephyr,canbus``), with the ``can_default``
  pinctrl group defined in
  ``boards/realtek/rtl87x2g_evb_a/rtl87x2g_evb_a_gkh_gku-pinctrl.dtsi`` and
  ``boards/realtek/rtl87x2j_evb/rtl87x2j_evb_rtl8762jth-pinctrl.dtsi``
  respectively.
- Reference:

  - `Zephyr CAN introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/canbus/index.html>`_
  - `Zephyr CAN API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__can__interface.html>`_

The CAN core clock is fixed at 40 MHz; the driver derives the bit timing from
this clock and the requested bitrate.

Operation Flow
--------------

Enabling a CAN device
~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set ``status = "okay"`` on the ``can`` node and give it a
  ``pinctrl-0`` group (named through ``pinctrl-names``); both properties are
  required by the binding. The initial bitrate and sample point come from the
  standard ``bitrate`` / ``sample-point`` properties of ``can-controller.yaml``;
  if unset, the bitrate defaults to ``CONFIG_CAN_DEFAULT_BITRATE``. Route the TX
  and RX pads through pinctrl with the ``CAN_TX`` / ``CAN_RX`` functions. See
  :doc:`Pinctrl <pinctrl>` for the pad-configuration syntax.

  .. code-block:: devicetree

     &can {
         status = "okay";
         pinctrl-0 = <&can_default>;
         pinctrl-names = "default";
         bitrate = <500000>;
     };

     &pinctrl {
         can_default: can_default {
             group1 {
                 psels = <BEE_PSEL(CAN_TX, P7_0)>,
                         <BEE_PSEL(CAN_RX, P7_1)>;
                 output-enable;
                 output-high;
                 bias-disable;
             };
         };
     };

  The block above shows the rtl87x2j ``CAN_TX`` / ``CAN_RX`` functions. On
  rtl87x2g the CAN pads use the ``A2C_TX`` / ``A2C_RX`` functions instead, for
  example ``BEE_PSEL(A2C_TX, P3_4)`` / ``BEE_PSEL(A2C_RX, P3_5)``.

  Enable CAN support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_CAN=y

- *How to use*: confirm the device is ready, optionally set a non-default mode
  and bit timing while the controller is stopped, then start the controller
  before transmitting or receiving. Stopping the controller returns it to the
  configuration state.
- *Example*: the ``&can`` node enabled in
  ``boards/realtek/rtl87x2g_evb_a/rtl87x2g_evb_a_rtl8762gku.dts`` and
  ``boards/realtek/rtl87x2j_evb/rtl87x2j_evb_rtl8762jth.dts``.

Message-buffer allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: the controller has 16 message buffers in total. Choose how
  many are dedicated to transmit and how many to receive with
  ``CONFIG_CAN_REALTEK_BEE_TX_MSG_BUF_NUM`` (default 6) and
  ``CONFIG_CAN_REALTEK_BEE_RX_MSG_BUF_NUM`` (default 10). The two counts must sum
  to at most 16. The transmit count sets how many transmissions can be in flight
  at once, and the receive count sets the maximum number of acceptance filters.

  .. code-block:: kconfig

     CONFIG_CAN_REALTEK_BEE_TX_MSG_BUF_NUM=6
     CONFIG_CAN_REALTEK_BEE_RX_MSG_BUF_NUM=10

- *How to use*: each transmit request occupies one free transmit buffer until it
  completes, and each receive filter you add occupies one receive buffer until
  it is removed. Adding a filter when all receive buffers are in use fails with
  ``-ENOSPC``, and a transmit request with no free transmit buffer blocks until
  one frees up or the supplied timeout expires.

Sending and receiving frames
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: transmit a ``struct can_frame`` on a started controller; the
  transmit call takes an optional completion callback that fires from the CAN
  interrupt when the frame has been sent (or failed). To receive, register a
  receive filter with a ``struct can_filter`` and a callback; the callback is
  invoked from the interrupt for each accepted frame, with the frame's
  identifier, DLC, flags, and payload filled in. Standard and extended
  identifiers and remote frames are handled. Remove a filter when you no longer
  need it to free its receive buffer.
- *Example*: the classic-CAN transmit/receive and filtering paths exercised by
  the ``tests/drivers/can/api`` suite in internal loopback mode.

Bus state and errors
~~~~~~~~~~~~~~~~~~~~~
- *How to use*: query the current bus state (error-active, error-warning,
  error-passive, bus-off, or stopped) together with the transmit and receive
  error counters at any time. Register a state-change callback to be notified
  when the bus state changes; the driver updates the state from the CAN
  interrupt on error and bus-off events, and fails pending transmissions with
  ``-ENETUNREACH`` when the bus goes off.

Receive timestamps
~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable ``CONFIG_CAN_RX_TIMESTAMP`` and, optionally, set the
  ``rx-timestamp-prescaler`` devicetree property (1–256, default 1) to divide the
  CAN source clock that drives the timestamp counter. A value of 1 runs the
  timestamp counter at the full CAN source clock rate.

  .. code-block:: devicetree

     &can {
         status = "okay";
         pinctrl-0 = <&can_default>;
         pinctrl-names = "default";
         rx-timestamp-prescaler = <2>;
     };

- *How to use*: when the option is enabled, each received frame delivered to a
  filter callback carries the capture timestamp in its ``timestamp`` field.

Power Management (DLPS)
-----------------------
- *Behavior*: on a SoC with a PCK600 (such as rtl87x2j), the PCK600 manages
  power in hardware. Once ``can_start()`` has started the controller it is kept
  out of sleep through a hardware handshake — neither the controller nor the
  system enters sleep while CAN is running — so the application must stop the
  controller with ``can_stop()`` before the system can sleep. While the
  controller is stopped the system can still enter DLPS, which power-gates the
  idle CAN block; the driver re-initializes the controller from its retained
  configuration on the next ``can_start()``, so CAN can be used again after
  wakeup with no reconfiguration.

.. note::

   On rtl87x2g, the v3.7 driver differs here: this SoC has no PCK600, so the
   driver implements a full ``PM_DEVICE`` DLPS handler instead. Its
   ``PM_DEVICE`` action suspends the controller on the way into DLPS and resumes
   it on exit, so entering and leaving DLPS is transparent to the application —
   it needs no reconfiguration and keeps using the same active-state API after
   wakeup. The handler does not, however, protect communication that is in
   progress: if the system enters DLPS mid-communication the CAN IP loses power
   and the transfer stops. The application must keep the system out of DLPS
   (for example with a DLPS check flag) while CAN is communicating.

Samples and Logs
----------------
Sample: ``tests/drivers/can/api``.

How to run
~~~~~~~~~~
- *Wiring*: none — the api suite drives the controller in internal loopback
  mode, so no external CAN bus or transceiver connection is required.
- *Extra config*: none — the board devicetree already enables ``&can``, selects
  it as ``zephyr,canbus``, and routes the CAN TX / RX pads (``CAN_TX`` /
  ``CAN_RX`` on rtl87x2j, ``A2C_TX`` / ``A2C_RX`` on rtl87x2g).
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/can/api

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/can/api

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-181-g6f933c042f6a ***
   Running TESTSUITE can_classic
   ===================================================================
   START - test_add_filter
    PASS - test_add_filter in 0.001 seconds
   ===================================================================
   START - test_add_filter_without_callback
    PASS - test_add_filter_without_callback in 0.001 seconds
   ===================================================================
   START - test_add_invalid_ext_filter
   E: invalid filter with extended (29-bit) CAN ID 0x1fffffff, CAN ID mask 0x20000000
   E: invalid filter with extended (29-bit) CAN ID 0x20000000, CAN ID mask 0x1fffffff
    PASS - test_add_invalid_ext_filter in 0.016 seconds
   ===================================================================
   START - test_add_invalid_null_filter
    PASS - test_add_invalid_null_filter in 0.001 seconds
   ===================================================================
   START - test_add_invalid_std_filter
   E: invalid filter with standard (11-bit) CAN ID 0x7ff, CAN ID mask 0x800
   E: invalid filter with standard (11-bit) CAN ID 0x800, CAN ID mask 0x7ff
    PASS - test_add_invalid_std_filter in 0.014 seconds
   ===================================================================
   START - test_bitrate_limits
    PASS - test_bitrate_limits in 0.001 seconds
   ===================================================================
   START - test_classic_get_capabilities
    PASS - test_classic_get_capabilities in 0.001 seconds
   ===================================================================
   START - test_filters_added_while_stopped
    PASS - test_filters_added_while_stopped in 0.003 seconds
   ===================================================================
   START - test_filters_preserved_through_bitrate_change
    PASS - test_filters_preserved_through_bitrate_change in 0.004 seconds
   ===================================================================
   START - test_filters_preserved_through_mode_change
    PASS - test_filters_preserved_through_mode_change in 0.004 seconds
   ===================================================================
   START - test_get_core_clock
    PASS - test_get_core_clock in 0.001 seconds
   ===================================================================
   START - test_get_state
    PASS - test_get_state in 0.001 seconds
   ===================================================================
   START - test_invalid_sample_point
    PASS - test_invalid_sample_point in 0.001 seconds
   ===================================================================
   START - test_max_ext_filters
   E: There is no free CAN RX filter
    PASS - test_max_ext_filters in 0.005 seconds
   ===================================================================
   START - test_max_std_filters
   E: There is no free CAN RX filter
    PASS - test_max_std_filters in 0.005 seconds
   ===================================================================
   START - test_receive_timeout
    PASS - test_receive_timeout in 0.101 seconds
   ===================================================================
   START - test_recover
    PASS - test_recover in 0.001 seconds
   ===================================================================
   START - test_recover_while_stopped
    SKIP - test_recover_while_stopped in 0.001 seconds
   ===================================================================
   START - test_reject_ext_id_rtr
    PASS - test_reject_ext_id_rtr in 0.102 seconds
   ===================================================================
   START - test_reject_std_id_rtr
    PASS - test_reject_std_id_rtr in 0.102 seconds
   ===================================================================
   START - test_send_and_forget
    PASS - test_send_and_forget in 0.002 seconds
   ===================================================================
   START - test_send_callback
    PASS - test_send_callback in 0.002 seconds
   ===================================================================
   START - test_send_ext_id_dlc_of_range
   E: DLC (9) exceeds maximum (8)
    PASS - test_send_ext_id_dlc_of_range in 0.004 seconds
   ===================================================================
   START - test_send_ext_id_out_of_range
   E: invalid frame with extended (29-bit) CAN ID 0x20000000
    PASS - test_send_ext_id_out_of_range in 0.006 seconds
   ===================================================================
   START - test_send_fd_format
   E: Unsupported CAN frame flags (0x4)
    PASS - test_send_fd_format in 0.004 seconds
   ===================================================================
   START - test_send_invalid_dlc
   E: DLC (9) exceeds maximum (8)
    PASS - test_send_invalid_dlc in 0.004 seconds
   ===================================================================
   START - test_send_null_frame
    PASS - test_send_null_frame in 0.001 seconds
   ===================================================================
   START - test_send_receive_ext_id
    PASS - test_send_receive_ext_id in 0.005 seconds
   ===================================================================
   START - test_send_receive_ext_id_masked
    PASS - test_send_receive_ext_id_masked in 0.005 seconds
   ===================================================================
   START - test_send_receive_ext_id_rtr
    SKIP - test_send_receive_ext_id_rtr in 0.001 seconds
   ===================================================================
   START - test_send_receive_msgq
    PASS - test_send_receive_msgq in 0.012 seconds
   ===================================================================
   START - test_send_receive_std_id
    PASS - test_send_receive_std_id in 0.005 seconds
   ===================================================================
   START - test_send_receive_std_id_masked
    PASS - test_send_receive_std_id_masked in 0.005 seconds
   ===================================================================
   START - test_send_receive_std_id_no_data
    PASS - test_send_receive_std_id_no_data in 0.002 seconds
   ===================================================================
   START - test_send_receive_std_id_rtr
    SKIP - test_send_receive_std_id_rtr in 0.001 seconds
   ===================================================================
   START - test_send_receive_wrong_id
    PASS - test_send_receive_wrong_id in 0.102 seconds
   ===================================================================
   START - test_send_std_id_dlc_of_range
   E: DLC (9) exceeds maximum (8)
    PASS - test_send_std_id_dlc_of_range in 0.004 seconds
   ===================================================================
   START - test_send_std_id_out_of_range
   E: invalid frame with standard (11-bit) CAN ID 0x800
    PASS - test_send_std_id_out_of_range in 0.006 seconds
   ===================================================================
   START - test_send_while_stopped
   E: CAN controller is not started
    PASS - test_send_while_stopped in 0.004 seconds
   ===================================================================
   START - test_set_bitrate
    PASS - test_set_bitrate in 0.001 seconds
   ===================================================================
   START - test_set_bitrate_too_high
    PASS - test_set_bitrate_too_high in 0.001 seconds
   ===================================================================
   START - test_set_bitrate_too_low
    SKIP - test_set_bitrate_too_low in 0.001 seconds
   ===================================================================
   START - test_set_bitrate_while_started
    PASS - test_set_bitrate_while_started in 0.001 seconds
   ===================================================================
   START - test_set_mode_while_started
    PASS - test_set_mode_while_started in 0.001 seconds
   ===================================================================
   START - test_set_state_change_callback
    PASS - test_set_state_change_callback in 0.001 seconds
   ===================================================================
   START - test_set_timing_max
    PASS - test_set_timing_max in 0.001 seconds
   ===================================================================
   START - test_set_timing_min
    PASS - test_set_timing_min in 0.001 seconds
   ===================================================================
   START - test_set_timing_while_started
    PASS - test_set_timing_while_started in 0.001 seconds
   ===================================================================
   START - test_start_while_started
    PASS - test_start_while_started in 0.001 seconds
   ===================================================================
   START - test_stop_while_stopped
    PASS - test_stop_while_stopped in 0.001 seconds
   ===================================================================
   TESTSUITE can_classic succeeded
   CAN controller does not support device power managementRunning TESTSUITE can_stats
   ===================================================================
   START - test_can_stats_accessors
    PASS - test_can_stats_accessors in 0.001 seconds
   ===================================================================
   TESTSUITE can_stats succeeded
   CAN transceiver device not readyRunning TESTSUITE can_utilities
   ===================================================================
   START - test_can_bytes_to_dlc
    PASS - test_can_bytes_to_dlc in 0.001 seconds
   ===================================================================
   START - test_can_dlc_to_bytes
    PASS - test_can_dlc_to_bytes in 0.001 seconds
   ===================================================================
   START - test_can_frame_matches_filter
    PASS - test_can_frame_matches_filter in 0.001 seconds
   ===================================================================
   TESTSUITE can_utilities succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [can_classic]: pass = 46, fail = 0, skip = 4, total = 50 duration = 0.552 seconds
    - PASS - [can_classic.test_add_filter] duration = 0.001 seconds
    - PASS - [can_classic.test_add_filter_without_callback] duration = 0.001 seconds
    - PASS - [can_classic.test_add_invalid_ext_filter] duration = 0.016 seconds
    - PASS - [can_classic.test_add_invalid_null_filter] duration = 0.001 seconds
    - PASS - [can_classic.test_add_invalid_std_filter] duration = 0.014 seconds
    - PASS - [can_classic.test_bitrate_limits] duration = 0.001 seconds
    - PASS - [can_classic.test_classic_get_capabilities] duration = 0.001 seconds
    - PASS - [can_classic.test_filters_added_while_stopped] duration = 0.003 seconds
    - PASS - [can_classic.test_filters_preserved_through_bitrate_change] duration = 0.004 seconds
    - PASS - [can_classic.test_filters_preserved_through_mode_change] duration = 0.004 seconds
    - PASS - [can_classic.test_get_core_clock] duration = 0.001 seconds
    - PASS - [can_classic.test_get_state] duration = 0.001 seconds
    - PASS - [can_classic.test_invalid_sample_point] duration = 0.001 seconds
    - PASS - [can_classic.test_max_ext_filters] duration = 0.005 seconds
    - PASS - [can_classic.test_max_std_filters] duration = 0.005 seconds
    - PASS - [can_classic.test_receive_timeout] duration = 0.101 seconds
    - PASS - [can_classic.test_recover] duration = 0.001 seconds
    - SKIP - [can_classic.test_recover_while_stopped] duration = 0.001 seconds
    - PASS - [can_classic.test_reject_ext_id_rtr] duration = 0.102 seconds
    - PASS - [can_classic.test_reject_std_id_rtr] duration = 0.102 seconds
    - PASS - [can_classic.test_send_and_forget] duration = 0.002 seconds
    - PASS - [can_classic.test_send_callback] duration = 0.002 seconds
    - PASS - [can_classic.test_send_ext_id_dlc_of_range] duration = 0.004 seconds
    - PASS - [can_classic.test_send_ext_id_out_of_range] duration = 0.006 seconds
    - PASS - [can_classic.test_send_fd_format] duration = 0.004 seconds
    - PASS - [can_classic.test_send_invalid_dlc] duration = 0.004 seconds
    - PASS - [can_classic.test_send_null_frame] duration = 0.001 seconds
    - PASS - [can_classic.test_send_receive_ext_id] duration = 0.005 seconds
    - PASS - [can_classic.test_send_receive_ext_id_masked] duration = 0.005 seconds
    - SKIP - [can_classic.test_send_receive_ext_id_rtr] duration = 0.001 seconds
    - PASS - [can_classic.test_send_receive_msgq] duration = 0.012 seconds
    - PASS - [can_classic.test_send_receive_std_id] duration = 0.005 seconds
    - PASS - [can_classic.test_send_receive_std_id_masked] duration = 0.005 seconds
    - PASS - [can_classic.test_send_receive_std_id_no_data] duration = 0.002 seconds
    - SKIP - [can_classic.test_send_receive_std_id_rtr] duration = 0.001 seconds
    - PASS - [can_classic.test_send_receive_wrong_id] duration = 0.102 seconds
    - PASS - [can_classic.test_send_std_id_dlc_of_range] duration = 0.004 seconds
    - PASS - [can_classic.test_send_std_id_out_of_range] duration = 0.006 seconds
    - PASS - [can_classic.test_send_while_stopped] duration = 0.004 seconds
    - PASS - [can_classic.test_set_bitrate] duration = 0.001 seconds
    - PASS - [can_classic.test_set_bitrate_too_high] duration = 0.001 seconds
    - SKIP - [can_classic.test_set_bitrate_too_low] duration = 0.001 seconds
    - PASS - [can_classic.test_set_bitrate_while_started] duration = 0.001 seconds
    - PASS - [can_classic.test_set_mode_while_started] duration = 0.001 seconds
    - PASS - [can_classic.test_set_state_change_callback] duration = 0.001 seconds
    - PASS - [can_classic.test_set_timing_max] duration = 0.001 seconds
    - PASS - [can_classic.test_set_timing_min] duration = 0.001 seconds
    - PASS - [can_classic.test_set_timing_while_started] duration = 0.001 seconds
    - PASS - [can_classic.test_start_while_started] duration = 0.001 seconds
    - PASS - [can_classic.test_stop_while_stopped] duration = 0.001 seconds

   SUITE SKIP -   0.00% [can_powermgmt]: pass = 0, fail = 0, skip = 3, total = 3 duration = 0.000 seconds
    - SKIP - [can_powermgmt.test_suspend_resume] duration = 0.000 seconds
    - SKIP - [can_powermgmt.test_suspend_while_filters_added] duration = 0.000 seconds
    - SKIP - [can_powermgmt.test_suspend_while_started] duration = 0.000 seconds

   SUITE PASS - 100.00% [can_stats]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.001 seconds
    - PASS - [can_stats.test_can_stats_accessors] duration = 0.001 seconds

   SUITE SKIP -   0.00% [can_transceiver]: pass = 0, fail = 0, skip = 1, total = 1 duration = 0.000 seconds
    - SKIP - [can_transceiver.test_get_transceiver] duration = 0.000 seconds

   SUITE PASS - 100.00% [can_utilities]: pass = 3, fail = 0, skip = 0, total = 3 duration = 0.003 seconds
    - PASS - [can_utilities.test_can_bytes_to_dlc] duration = 0.001 seconds
    - PASS - [can_utilities.test_can_dlc_to_bytes] duration = 0.001 seconds
    - PASS - [can_utilities.test_can_frame_matches_filter] duration = 0.001 seconds

   SUITE SKIP -   0.00% [canfd]: pass = 0, fail = 0, skip = 15, total = 15 duration = 0.000 seconds
    - SKIP - [canfd.test_canfd_get_capabilities] duration = 0.000 seconds
    - SKIP - [canfd.test_filters_preserved_through_classic_to_fd_mode_change] duration = 0.000 seconds
    - SKIP - [canfd.test_filters_preserved_through_fd_to_classic_mode_change] duration = 0.000 seconds
    - SKIP - [canfd.test_invalid_sample_point] duration = 0.000 seconds
    - SKIP - [canfd.test_send_fd_dlc_out_of_range] duration = 0.000 seconds
    - SKIP - [canfd.test_send_fd_incorrect_esi] duration = 0.000 seconds
    - SKIP - [canfd.test_send_receive_classic] duration = 0.000 seconds
    - SKIP - [canfd.test_send_receive_fd] duration = 0.000 seconds
    - SKIP - [canfd.test_send_receive_mixed] duration = 0.000 seconds
    - SKIP - [canfd.test_set_bitrate_data_too_low] duration = 0.000 seconds
    - SKIP - [canfd.test_set_bitrate_data_while_started] duration = 0.000 seconds
    - SKIP - [canfd.test_set_bitrate_too_high] duration = 0.000 seconds
    - SKIP - [canfd.test_set_timing_data_max] duration = 0.000 seconds
    - SKIP - [canfd.test_set_timing_data_min] duration = 0.000 seconds
    - SKIP - [canfd.test_set_timing_data_while_started] duration = 0.000 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :doc:`Clock Control <clock_control>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr CAN introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/canbus/index.html>`_
- `Zephyr CAN API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__can__interface.html>`_
