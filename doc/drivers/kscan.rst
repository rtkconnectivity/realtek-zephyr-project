KSCAN
=====

The keyscan driver scans an external hardware key matrix on the Bee
family and reports key presses and releases to the application. The controller
drives the row/column lines itself; the application just listens for the
resulting input events.

The keyscan driver plugs into the Zephyr **input** subsystem
(``drivers/input/input_bee_keyscan.c``, compatible ``realtek,bee-keyscan``, node
``keyscan``, Kconfig ``INPUT_BEE_KEYSCAN``): it builds on the input
keyboard-matrix helper (``INPUT_KBD_MATRIX``) and reports ``INPUT_EV_KEY`` /
``INPUT_EV_ABS`` events through the input subsystem, which any input listener
(for example the ``input_dump`` sample) can consume.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: the node is named
   ``kscan`` (compatible ``realtek,bee-kscan``), and two alternative drivers are
   provided for it, selected by Kconfig. ``drivers/input/input_bee_kscan.c``
   (``INPUT_BEE_KSCAN``) reports through the input subsystem, exactly like the
   v4.4 driver; ``drivers/kscan/kscan_bee.c`` (``KSCAN_BEE``) instead implements
   the legacy Zephyr kscan API, which has since been removed entirely from
   upstream Zephyr. Use the node name, compatible and properties that match the
   driver you enable.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Drives an external row/column key matrix.
- Reports key events to the application through the Zephyr input subsystem
  (``INPUT_EV_*``); an input listener such as the ``input_dump`` sample can
  consume the events.
- Auto mode (continuous hardware scanning, for responsive/high-speed scenarios)
  and manual mode (periodic software-triggered scanning, for low power).
- Hardware debounce plus configurable scan and release timing from devicetree.
- Wakeup from DLPS through the row pads: on rtl87x2j the row pads can be armed
  as system wakeup sources in manual mode.
- Matrix size: up to 12 rows × 20 columns on rtl87x2g / rtl8752h, and up to
  18 rows × 20 columns on rtl87x2j.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``keyscan``.
- Bindings file: ``dts/bindings/input/realtek,bee-keyscan.yaml``
  (compatible ``realtek,bee-keyscan``).
- Kconfig option: ``INPUT_BEE_KEYSCAN`` (``drivers/input/Kconfig.bee_keyscan``).
- Source file: ``drivers/input/input_bee_keyscan.c``.
- Example board files: per-SoC overlays under
  ``samples/subsys/input/input_dump/boards/`` —
  ``rtl87x2g_evb_a_rtl8762gku.overlay``, ``rtl8752h_evb_rtl8752hjl.overlay`` and
  ``rtl87x2j_evb_rtl8762jth.overlay``.
- Reference:

  - `Zephyr input introduction <https://docs.zephyrproject.org/latest/services/input/index.html>`_
  - `Zephyr kscan API reference <https://docs.zephyrproject.org/3.7.0/doxygen/html/group__kscan__interface.html>`_

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver uses different files and names:

   - Device node ``kscan``.
   - Binding ``dts/bindings/input/realtek,bee-kscan.yaml``.
   - Kconfig ``drivers/input/Kconfig.bee_kscan`` and
     ``drivers/kscan/Kconfig.bee``.
   - Sources ``drivers/input/input_bee_kscan.c`` and
     ``drivers/kscan/kscan_bee.c``.

   The example overlays are the same ``input_dump`` board overlays listed above.

Operation Flow
--------------

Enabling a KSCAN device
~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the keyscan node, route the row/column pads through
  pinctrl, and declare the matrix size and timing. The main node properties are:

  - ``row-size`` / ``col-size`` (required): the actual key-matrix size. It must
    not exceed the hardware maximum (12 × 20 on rtl87x2g / rtl8752h, 18 × 20 on
    rtl87x2j).
  - ``scan-div`` (optional, default ``1``): scan-clock divider from the 5 MHz
    source clock — ``scan_clock = 5 MHz / (scan-div + 1)``; maximum ``2047``.
  - ``delay-div`` (optional, default ``49``): further divides the scan clock into
    the delay clock used for the debounce, poll-interval and release timers —
    ``delay_clock = scan_clock / (delay-div + 1)``; maximum ``63``.
  - ``poll-period-us`` (optional): scan interval. Unlimited in manual mode (the
    scan is driven by a software timer); in auto mode it is limited by the
    delay-clock period (at most 512 ticks).
  - ``release-time-us`` (optional, default ``5000``): time to detect that all
    keys are released in auto mode.
  - ``debounce-down-ms`` / ``debounce-up-ms`` (kbd-matrix common): software
    debounce applied by the input keyboard-matrix helper.

  See ``dts/bindings/input/realtek,bee-keyscan.yaml`` for more details.

  When configuring pinctrl, the ``KEY_COL_*`` and ``KEY_ROW_*`` functions must be
  numbered consecutively from 0 with no gaps. For high scan rates, configure the
  row pads with a strong pull-up (``bias-pull-strong``) in their pinctrl group for
  better signal integrity.

  .. code-block:: devicetree

     &keyscan {
         pinctrl-0 = <&keyscan_default>;
         pinctrl-names = "default";
         row-size = <2>;
         col-size = <2>;
         debounce-down-ms = <10>;
         debounce-up-ms = <10>;
         poll-period-us = <10000>;
         release-time-us = <5000>;
         status = "okay";
     };

     &pinctrl {
         keyscan_default: keyscan_default {
             group1 {
                 psels = <BEE_PSEL(KEY_COL_0, P2_0)>,
                         <BEE_PSEL(KEY_COL_1, P3_6)>;
                 output-enable;
                 output-low;
                 bias-disable;
             };

             group2 {
                 psels = <BEE_PSEL(KEY_ROW_0, P2_4)>,
                         <BEE_PSEL(KEY_ROW_1, P2_5)>;
                 output-disable;
                 bias-pull-up;
                 bias-pull-strong;
             };
         };
     };

  .. note::

     On rtl87x2g / rtl8752h, the v3.7 driver differs here: the node is named
     ``kscan`` (binding ``dts/bindings/kscan/realtek,bee-kscan.yaml``) and takes
     a different timing model — the required ``max-row-size`` / ``max-col-size``
     (hardware maximum, default ``12`` / ``20``) alongside the actual
     ``row-size`` / ``col-size``, the time properties ``debounce-time-us``,
     ``scan-time-us`` and ``release-time-us`` (each 0–10240 us with 20 us
     resolution), and a ``scan-debounce-cnt`` count. It is still enabled through
     the input subsystem (``CONFIG_INPUT=y``) with the driver symbol
     ``INPUT_BEE_KSCAN`` (see the Kconfig note below). A separate legacy
     kscan-subsystem driver (``KSCAN_BEE`` / ``CONFIG_KSCAN``,
     ``drivers/kscan/kscan_bee.c``) binds the same node but is mutually exclusive
     with ``INPUT_BEE_KSCAN``.

  Enable the input subsystem in ``prj.conf`` with ``CONFIG_INPUT=y``:

  .. code-block:: kconfig

     CONFIG_INPUT=y

- *Kconfig*: the options live in ``drivers/input/Kconfig.bee_keyscan`` —
  ``INPUT_BEE_KEYSCAN`` (enable), ``BEE_INPUT_KEYSCAN_AUTOSCAN_MODE`` (auto vs.
  manual scanning) and ``BEE_INPUT_KEYSCAN_PM_KEY_WAKEUP`` (arm the row pads as
  wakeup sources in manual mode; rtl87x2j only).

  .. note::

     On rtl87x2g / rtl8752h, the v3.7 driver's options live in
     ``drivers/input/Kconfig.bee_kscan`` — ``INPUT_BEE_KSCAN`` (enable),
     ``BEE_INPUT_KSCAN_GHOST_KEY_FILTER`` (whether to filter ghost keys),
     ``BEE_INPUT_KSCAN_AUTOSCAN_MODE`` (auto vs. manual scanning) and
     ``BEE_INPUT_KSCAN_RELEASE_WAKEUP``.

- *Example*: the ``&keyscan`` node in
  ``samples/subsys/input/input_dump/boards/rtl87x2j_evb_rtl8762jth.overlay``.

Auto vs. manual scanning
~~~~~~~~~~~~~~~~~~~~~~~~
In auto mode, scanning is triggered by hardware, giving shorter and more precise
intervals; this suits latency-sensitive scenarios that need a higher scan rate.
The trade-off is that the system cannot enter DLPS between two scans. In manual
mode the system can enter sleep between scans when the PM feature is enabled,
which suits power-sensitive scenarios where energy efficiency is the priority.

- *How to configure*: use ``BEE_INPUT_KEYSCAN_AUTOSCAN_MODE`` to select auto or
  manual scanning. In manual mode the scan period follows the node's
  ``poll-period-us`` property.

  .. note::

     On rtl87x2g / rtl8752h, the v3.7 driver uses
     ``BEE_INPUT_KSCAN_AUTOSCAN_MODE`` instead.

- *How to use*: the driver scans automatically once enabled. The application
  registers an input callback with ``INPUT_CALLBACK_DEFINE()`` (or uses an input
  listener like ``input_dump``).

  .. note::

     On rtl87x2g / rtl8752h, the ``INPUT_BEE_KSCAN`` input driver reports through
     the input subsystem, so the same input-callback approach applies. The
     alternative legacy ``KSCAN_BEE`` driver instead uses the kscan API: register
     a kscan callback with ``kscan_config()`` and enable scanning with
     ``kscan_enable_callback()``.

- *Example*: the event stream from the ``input_dump`` sample, shown in the log
  below.

Debounce
~~~~~~~~
On v4.4 the driver disables the controller's hardware debounce and uses the
input keyboard-matrix helper's software debounce (``debounce-down-ms`` /
``debounce-up-ms``) instead, which gives per-key debounce.

- *How to configure*: set the debounce and scan/release timing through the
  keyscan node's devicetree properties. Software debounce is applied per key by
  the ``INPUT_KBD_MATRIX`` helper.
- *Example*: the debounce and timing properties on the keyscan node in the
  sample overlay.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: in auto-scan mode it
   uses the controller's hardware debounce as the first debounce and then
   applies a software debounce over the number of scans configured in
   devicetree to achieve per-key debounce; in manual mode it uses only that
   devicetree-configured software debounce.

Power Management (DLPS)
-----------------------
- *Behavior*: on a SoC with a PCK600 (such as rtl87x2j), the PCK600 manages
  power in hardware. In auto mode the system cannot enter DLPS while a scan is
  in progress; in manual mode each scan is triggered by a software timer
  and the system may sleep between two scans. With
  ``BEE_INPUT_KEYSCAN_PM_KEY_WAKEUP`` enabled the driver arms the currently
  pressed/released row pads as system wakeup sources before the system sleeps,
  so a key press or release during sleep wakes the system in time to scan;
  with it disabled the system relies solely on the periodic timer to wake and
  detect key changes. This is transparent to the application.

  .. note::

     On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
     PCK600, so the driver implements a full ``PM_DEVICE`` DLPS handler instead.
     DLPS handling is transparent to the application. In auto mode, because
     DLPS cannot be entered between two scans, the system may enter DLPS only
     when all keys are released; as long as any key is pressed the keyscan
     hardware must keep scanning. In manual mode each scan starts a software
     timer to trigger the next scan, and the system may enter DLPS while that
     timer runs, until it expires and triggers the next scan. While the system
     is in DLPS the keyscan pads are switched to the ``sleep`` pinctrl state
     from devicetree, so the row pads can be configured to wake on a low level
     (the driver switches between low/high wakeup according to the current
     press/release state); this lets a key press or release during DLPS wake
     the system in time to scan:

     .. code-block:: devicetree

        &kscan {
            pinctrl-0 = <&kscan_default>;
            pinctrl-1 = <&kscan_sleep>;
            pinctrl-names = "default", "sleep";
            row-size = <2>;
            col-size = <2>;
            status = "okay";
        };

        &pinctrl {
            kscan_default: kscan_default {
                group1 {
                    psels = <BEE_PSEL(KEY_COL_0, P2_6, DIR_OUT, DRV_LOW, PULL_NONE)>,
                            <BEE_PSEL(KEY_COL_1, P2_7, DIR_OUT, DRV_LOW, PULL_NONE)>;
                };

                group2 {
                    psels = <BEE_PSEL(KEY_ROW_0, P2_4, DIR_IN, DRV_LOW, PULL_UP)>,
                            <BEE_PSEL(KEY_ROW_1, P2_5, DIR_IN, DRV_LOW, PULL_UP)>;
                    bias-pull-strong;
                };
            };

            kscan_sleep: kscan_sleep {
                group1 {
                    psels = <BEE_PSEL(SW_MODE, P2_6, DIR_OUT, DRV_LOW, PULL_NONE)>,
                            <BEE_PSEL(SW_MODE, P2_7, DIR_OUT, DRV_LOW, PULL_NONE)>;
                };

                group2 {
                    psels = <BEE_PSEL(SW_MODE, P2_4, DIR_IN, DRV_LOW, PULL_UP)>,
                            <BEE_PSEL(SW_MODE, P2_5, DIR_IN, DRV_LOW, PULL_UP)>;
                    bias-pull-strong;
                    wakeup-low;
                };
            };
        };

Samples and Logs
----------------
Sample: ``samples/subsys/input/input_dump`` (input subsystem).

.. note::

   On rtl87x2g / rtl8752h the same ``input_dump`` sample is used; the v3.7 driver
   is just named ``kscan`` instead of ``keyscan`` (see Basic Information above).

How to run
~~~~~~~~~~
- *Wiring*: connect a 2x2 key matrix (or jumper individual row/column pads) to
  the ``KEY_COL_*`` / ``KEY_ROW_*`` pads configured for the keyscan node.
  The actual pads are set by the board overlay and differ per SoC.
- *Extra config*: none — the board overlay enables the keyscan node and the
  sample enables the input subsystem.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku samples/subsys/input/input_dump

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   samples/subsys/input/input_dump

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   samples/subsys/input/input_dump

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-174-g8855e3ecc827 ***
   Input sample started
   I: input event: dev=keyscan@40004800     type= 3 code=  0 value=0
   I: input event: dev=keyscan@40004800     type= 3 code=  1 value=1
   I: input event: dev=keyscan@40004800 SYN type= 1 code=330 value=1
   I: input event: dev=keyscan@40004800     type= 3 code=  0 value=0
   I: input event: dev=keyscan@40004800     type= 3 code=  1 value=1
   I: input event: dev=keyscan@40004800 SYN type= 1 code=330 value=0
   I: input event: dev=keyscan@40004800     type= 3 code=  0 value=1
   I: input event: dev=keyscan@40004800     type= 3 code=  1 value=0
   I: input event: dev=keyscan@40004800 SYN type= 1 code=330 value=1
   I: input event: dev=keyscan@40004800     type= 3 code=  0 value=1
   I: input event: dev=keyscan@40004800     type= 3 code=  1 value=0
   I: input event: dev=keyscan@40004800 SYN type= 1 code=330 value=0
   I: input event: dev=keyscan@40004800     type= 3 code=  0 value=1
   I: input event: dev=keyscan@40004800     type= 3 code=  1 value=1
   I: input event: dev=keyscan@40004800 SYN type= 1 code=330 value=1
   I: input event: dev=keyscan@40004800     type= 3 code=  0 value=1
   I: input event: dev=keyscan@40004800     type= 3 code=  1 value=1
   I: input event: dev=keyscan@40004800 SYN type= 1 code=330 value=0
   I: input event: dev=keyscan@40004800     type= 3 code=  0 value=0
   I: input event: dev=keyscan@40004800     type= 3 code=  1 value=0
   I: input event: dev=keyscan@40004800 SYN type= 1 code=330 value=1
   I: input event: dev=keyscan@40004800     type= 3 code=  0 value=0
   I: input event: dev=keyscan@40004800     type= 3 code=  1 value=0
   I: input event: dev=keyscan@40004800 SYN type= 1 code=330 value=0

As you press and release different keys on the matrix, the corresponding
press and release events appear in the log.

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr input introduction <https://docs.zephyrproject.org/latest/services/input/index.html>`_
- `Zephyr kscan API reference <https://docs.zephyrproject.org/3.7.0/doxygen/html/group__kscan__interface.html>`_
