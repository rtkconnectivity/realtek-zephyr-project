.. _pinctrl:

Pinctrl
=======

The pin controller driver configures the Bee pads through the standard Zephyr
pinctrl API. A pad can be put in *pinmux mode* (the pad is driven directly by
the peripheral hardware) or in *software mode* (``SW_MODE``), which sets the
pad's electrical properties — pull, direction, drive level, drive current, and
wakeup. Pin configuration is grouped per device and per device *state* (for
example ``default`` and ``sleep``), and each peripheral driver applies its pins
automatically (at init, and during PM actions), so application code normally
never calls the pinctrl API directly.

This chapter also explains how pads, pinmux, pinctrl, and GPIO relate, and how
pinctrl underpins the cross-peripheral DLPS (deep low-power state) behavior used
elsewhere in this chapter.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Pinmux: route a supported peripheral function to a pad (for example
  ``UART2_TX`` to ``P3_2``). The full function and pad list is in the per-SoC
  pinctrl header.
- Per-pad electrical configuration: pull up / down / none, input or output
  direction, drive level (high/low), a four-level driving current, and support
  for a strong pull resistor.
- Per-device states: each device declares one configuration node per state
  (``default``, ``sleep``, ...), and the driver applies the matching state on
  request (the ``sleep`` state is applied automatically across DLPS).
- DLPS wakeup: a pad can be configured to wake the SoC on a high or low level
  during DLPS. Only the UART, GPIO, and KSCAN drivers implement the pad-wakeup
  handling that acts on this configuration.
- Special pad modes: software-controlled mode (``SW_MODE``) and per-SoC
  dedicated function muxes (see the per-SoC note below).

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``pinctrl`` (shared by all on-chip peripherals).
- Bindings file: ``dts/bindings/pinctrl/realtek,bee-pinctrl.yaml``
  (compatible ``realtek,bee-pinctrl``).
- BEE_PSEL macro: ``include/zephyr/dt-bindings/pinctrl/bee-pinctrl.h``.
- Per-SoC function-ID / pad-name headers:
  ``include/zephyr/dt-bindings/pinctrl/rtl87x2g-pinctrl.h``,
  ``rtl8752h-pinctrl.h``, and ``rtl87x2j-pinctrl.h``.
- SoC pinctrl data / decoding: ``soc/realtek/bee/common/pinctrl_soc.h``.
- Kconfig option: ``PINCTRL_BEE`` (``drivers/pinctrl/Kconfig.bee``).
- Source file: ``drivers/pinctrl/pinctrl_bee.c``.
- Example board files: the per-board ``*-pinctrl.dtsi``, for example
  ``boards/realtek/rtl87x2g_evb_a/rtl87x2g_evb_a_gkh_gku-pinctrl.dtsi``,
  ``boards/realtek/rtl8752h_evb/rtl8752h_evb-pinctrl.dtsi``, and
  ``boards/realtek/rtl87x2j_evb/rtl87x2j_evb_rtl8762jth-pinctrl.dtsi``.
- Reference:

  - `Zephyr pin control introduction <https://docs.zephyrproject.org/latest/hardware/pinctrl/index.html>`_
  - `Zephyr pin control API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__pinctrl__interface.html>`_

.. note::

   Per-SoC differences:

   - **Available pads differ:** rtl87x2g exposes pads ``P0_0``–``P10_2``,
     rtl8752h ``P0_0``–``P5_2``, and rtl87x2j ``P0_0``–``P7_1``.
   - **Available functions differ:** each SoC's ``*-pinctrl.h`` header defines
     its own function set.
   - **Drive current:** ``current-level`` (0–3) maps to different milliamp values
     on each SoC and depends on ``VDDIO``. See the binding for the exact
     per-SoC/VDDIO table.
   - **Convenience aliases:** all three SoCs provide ready-made
     ``BEE_PSEL_GPIOA_<n>_Px_y`` aliases for GPIO pad mapping. (On the v3.7
     rtl87x2g / rtl8752h driver these aliases do not exist.)
   - **sleep-hardware-state:** this property applies only to a SoC with a
     PCK600 (such as rtl87x2j). On such a SoC the peripheral drivers have no
     DLPS enter/exit callback, so the ``sleep`` state cannot be switched in on
     each transition; instead the ``sleep`` pads are programmed into the pad
     hardware at init and held automatically across sleep. rtl87x2g / rtl8752h have
     no ``sleep-hardware-state`` property.

Pads, pinmux, pinctrl, and GPIO
-------------------------------
These four terms are easy to confuse, but they form a layered picture.

**PINMUX** (pin multiplexing): because the SoC has a limited number of pins, pin
multiplexing lets it reuse those pins for various functions such as SPI, I2C,
and GPIO.

**PAD** controls the electrical behavior of a pin — pull up / down, output high
or low level, and wakeup.

The PINMUX circuit and IO modules are in the **core domain** and are powered
down during low-power mode, so they cannot work during DLPS. The PAD circuit is
in the **AON (always-on) domain** and is not powered down, so it keeps working
during DLPS; the PAD is used to hold the pin output state or to wake the system
in low-power mode. A pad can be in PINMUX mode or software mode — only in PINMUX
mode can the pin connect to the core domain to achieve pin multiplexing.

The layers, from the physical pin up to the software that configures them:

- **Pad** — the physical pin on the package (named ``P<port>_<n>``, for example
  ``P3_2``). One pad can be driven by exactly one function at a time.
- **Pinmux** — the hardware multiplexer that decides *which* peripheral function
  a pad carries (for example ``UART2_TX``, or a GPIO).
- **Pinctrl** — the Zephyr layer that configures both, declaratively from
  devicetree: it picks the pinmux function for a pad and sets the pad's
  electrical properties (pull, direction, drive, current, wakeup).
- **GPIO** — the case where a pad is used as generic I/O.

.. note::

   A pad used through the GPIO driver **does** need a pinctrl group. The GPIO
   node takes ``pinctrl-0`` (required by ``realtek,bee-gpio``) and you map each
   logical GPIO pin to a pad with ``psels`` (for example
   ``BEE_PSEL_GPIOA_0_P0_0``). A logical GPIO line can be reached from more than
   one pad, and this pad-to-GPIO routing is a hardware configuration, so it
   belongs in devicetree: select the pad by configuring the matching
   ``BEE_PSEL_GPIOA_<n>_<Px_y>`` alias for the pad you wired (only one pad may
   back a given GPIO line at a time). This applies to all three SoCs on the v4.4
   baseline — they share the unified ``realtek,bee-gpio`` binding.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: the GPIO node has no
   pinctrl group at all. The same one-to-many pad-to-GPIO remapping is instead
   resolved at build time — a Kconfig option (such as
   ``CONFIG_BEE_USE_P6_2_AS_GPIOB21``) selects which physical pad a GPIO line maps
   to. Moving this hardware-dependent routing out of Kconfig and into
   pinctrl/devicetree is the main reason the v4.4 GPIO node requires a pinctrl
   group. See the :doc:`GPIO chapter <gpio>` for the full story.

So, for any peripheral, you use pinctrl to route the pads to that peripheral's
functions; the peripheral driver then applies that pinctrl configuration
automatically.

.. note::

   A few pads are reserved for debug by default and should not be routed to other
   peripheral functions unless you first free them: ``P0_3`` carries the RTK
   internal log output, and ``P1_0`` / ``P1_1`` are used as the SWD debug
   interface (SWCLK and SWDIO). The RTK internal log on ``P0_3`` is a separate
   channel from the Zephyr console described in the
   :ref:`Logging note in the Overview <driver_logging_note>`.

Operation Flow
--------------

Defining a pin configuration group
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: add child nodes under ``&pinctrl``, one per device state.
  Each state node contains one or more groups, and each group lists pin function
  selections in its ``psels`` property using the ``BEE_PSEL`` macro. Electrical
  attributes are set as standard properties alongside ``psels``:

  .. code-block:: devicetree

     #include <zephyr/dt-bindings/pinctrl/rtl87x2g-pinctrl.h>

     &pinctrl {
         uart2_default: uart2_default {
             group1 {
                 psels = <BEE_PSEL(UART2_TX, P3_2)>;
                 output-enable;
                 output-high;
                 bias-pull-up;
             };
             group2 {
                 psels = <BEE_PSEL(UART2_RX, P3_3)>;
                 output-disable;
                 bias-pull-up;
             };
         };
     };

  ``BEE_PSEL(fun, pin)`` encodes one pad: ``fun`` is a function name from the
  SoC's ``*-pinctrl.h`` header (without the ``BEE_`` prefix, for example
  ``UART2_TX``), and ``pin`` is a pad name (for example ``P3_2``). Include the
  header for your SoC (``rtl87x2g-pinctrl.h``, ``rtl8752h-pinctrl.h``, or
  ``rtl87x2j-pinctrl.h``).

- *Example*: the ``uart2_default`` node in the board's ``*-pinctrl.dtsi``.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: the same group is
   written with the 5-argument ``BEE_PSEL(fun, pin, dir, drive, pull)`` macro,
   which folds direction, drive, and pull into the macro arguments, with no
   separate ``output-*`` / ``bias-*`` properties:

   .. code-block:: devicetree

      group1 {
          psels = <BEE_PSEL(UART2_TX, P3_2, DIR_OUT, DRV_HIGH, PULL_UP)>,
                  <BEE_PSEL(UART2_RX, P3_3, DIR_IN,  DRV_HIGH, PULL_UP)>;
      };

   The direction, drive, and pull that the v4.4 baseline sets as group
   properties become macro arguments here:

   - ``output-enable`` / ``output-disable`` become ``DIR_OUT`` / ``DIR_IN``
   - ``output-high`` / ``output-low`` become ``DRV_HIGH`` / ``DRV_LOW``
   - ``bias-pull-up`` / ``bias-pull-down`` / ``bias-disable`` become ``PULL_UP`` /
     ``PULL_DOWN`` / ``PULL_NONE``

Setting extra pad properties
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: properties beyond ``psels`` are set per group and apply to
  all pads in that group:

  - ``bias-pull-strong`` — use the strong pull resistor instead of the weak one.
  - ``current-level`` — driving current, ``0``–``3`` (the actual mA is SoC- and
    ``VDDIO``-dependent; see the binding). Defaults to ``0`` if omitted.
  - ``wakeup-high`` / ``wakeup-low`` — arm the pad to wake the SoC from DLPS on a
    high or low level (see :ref:`Power Management (DLPS) <pinctrl_dlps>` below).
  - ``sleep-hardware-state`` — keep the pad configuration alive in hardware
    across sleep. This is required on a SoC with a PCK600 (such as
    rtl87x2j): on such a SoC the peripheral drivers have no DLPS enter/exit
    callback, so the ``sleep`` state cannot be applied on the way into DLPS —
    instead it is pre-configured into the pad hardware at init and held
    automatically across sleep (see the PM note).

  .. code-block:: devicetree

     &pinctrl {
         kscan_default: kscan_default {
             group1 {
                 psels = <BEE_PSEL(KEY_ROW_0, P3_2)>;
                 output-disable;
                 bias-pull-up;
                 bias-pull-strong;
                 current-level = <1>;
             };
         };
     };

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: direction, drive,
   and pull are encoded in the 5-argument ``psels`` macro instead of the separate
   ``output-*`` / ``bias-*`` properties (see the form note above), and there is
   no ``sleep-hardware-state`` property.

Linking a pin configuration to a device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: reference the state nodes from the device with ``pinctrl-N``
  properties and name them with ``pinctrl-names``. State ``0`` is conventionally
  ``default`` (applied at init); ``sleep`` is applied automatically before
  entering and after exiting DLPS:

  .. code-block:: devicetree

     &uart2 {
         pinctrl-0 = <&uart2_default>;
         pinctrl-1 = <&uart2_sleep>;
         pinctrl-names = "default", "sleep";
         status = "okay";
     };

  Application code does not call the pinctrl API; the peripheral driver applies
  the ``default`` state at init and switches to ``sleep`` before entering DLPS
  and back to ``default`` after exiting.

- *Example*: the ``&uart2`` node in the board ``.dts``.

.. _pinctrl_dlps:

Power Management (DLPS)
-----------------------
DLPS (deep low-power state) is the SoC's main sleep mode. The system enters DLPS
when idle and leaves it on a wakeup event (a software-timer wakeup, or a pad
wakeup). Most handling is transparent to the application — each driver stores and
restores its own hardware state and switches between its pinctrl configurations —
so the application just keeps using the standard Zephyr APIs.

- *Sleep-state pads*: pad routing is **not** retained automatically across DLPS,
  so pre-configure each device's ``sleep`` state in devicetree (``pinctrl-1``
  named ``"sleep"``). How that ``sleep`` state takes effect depends on whether the
  SoC has a PCK600.

  On a SoC with a PCK600 (such as rtl87x2j) the pad mode is
  switched **by hardware**: on entering DLPS the pad is parked in ``SW_MODE``
  automatically, and on exit it is restored to PINMUX mode — the peripheral
  drivers have no DLPS enter/exit callback and do no per-transition switching.
  Because this mode switch is automatic, a pad whose pull does **not** need to
  change during DLPS needs no manual ``SW_MODE`` sleep entry at all. You only add
  a ``sleep`` group entry marked ``sleep-hardware-state`` when the pad must hold a
  **different pull** while parked: that property programs the sleep pull into the
  pad hardware at init, so the hardware applies it automatically when it parks the
  pad.

  The ``sleep`` state is nevertheless written out in full, in ``SW_MODE`` form, so
  the same configuration is portable to a SoC without a PCK600 (such as rtl87x2g
  / rtl8752h), where there is no automatic hardware parking and each peripheral's
  ``PM_DEVICE`` handler switches the ``default`` / ``sleep`` pinctrl states in its
  DLPS enter/exit callbacks instead (see the note below). A common ``sleep``
  configuration parks the pad in ``SW_MODE`` with a defined pull that matches the
  level the external device holds, so no current leaks across the pad. Because the
  external device is usually still powered during DLPS, its pins typically sit
  high, so the pad is parked with ``bias-pull-up`` to match:

  .. code-block:: devicetree

     &uart2 {
         pinctrl-0 = <&uart2_default>;
         pinctrl-1 = <&uart2_sleep>;
         pinctrl-names = "default", "sleep";
         status = "okay";
     };

     &pinctrl {
         uart2_default: uart2_default {
             group1 {
                 psels = <BEE_PSEL(UART2_TX, P3_2)>;
                 output-enable;
                 output-high;
                 bias-pull-up;
             };
             group2 {
                 psels = <BEE_PSEL(UART2_RX, P3_3)>;
                 output-disable;
                 bias-pull-up;
             };
         };

         uart2_sleep: uart2_sleep {
             group1 {
                 psels = <BEE_PSEL(SW_MODE, P3_2)>;
                 output-enable;
                 output-high;
                 sleep-hardware-state;
                 bias-pull-up;
             };
             group2 {
                 psels = <BEE_PSEL(SW_MODE, P3_3)>;
                 output-disable;
                 bias-pull-up;
                 sleep-hardware-state;
                 wakeup-low;
             };
         };
     };

  With this configuration: while UART2 is in use the ``default`` state is applied
  — ``P3_2`` is muxed as ``UART2_TX`` and ``P3_3`` as ``UART2_RX``. On entering
  DLPS the hardware applies the ``sleep`` state — both pads are parked in
  ``SW_MODE`` with a pull-up, and ``P3_3`` has low-level wakeup armed. When
  ``P3_3`` sees a low level (for example a UART start bit) it wakes the SoC; on
  exit the ``default`` state is restored and UART2 resumes. See the
  :doc:`UART chapter <uart>` for the UART-side PM details.

.. note::

   On the v4.4 baseline these DT properties are decoded into the pinctrl pin
   struct. ``sleep-hardware-state`` is applied when the state is programmed, via
   ``Pad_LPConfig()``. ``wakeup-high`` / ``wakeup-low`` only record the wakeup
   polarity; the pad wakeup itself is armed later by the peripheral driver through
   the ``PINCTRL_BEE_WAKEUP_PPU`` path (``System_WakeUpPPUCmd()``) — for example
   the GPIO driver arms it when an interrupt is enabled. The resulting pad-wakeup
   events are then handled in each peripheral's own registered pad-wakeup
   interrupt handler.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so each peripheral's ``PM_DEVICE`` handler switches the pinctrl states
   in software — it applies the ``sleep`` state in its DLPS-enter callback and
   restores ``default`` in its DLPS-exit callback (there is no
   ``sleep-hardware-state`` property). Wakeup is also wired differently: the
   ``wakeup-high`` / ``wakeup-low`` properties configure pad wakeup directly, and
   direction/drive/pull come from the 5-argument ``psels`` macro instead. The
   resulting pad-wakeup events are handled in each peripheral's own DLPS-exit
   callback.

Samples and Logs
----------------
Pinctrl has no standalone sample. It is exercised indirectly by every peripheral
in this chapter, since each one routes its pads through ``pinctrl`` at init. To
see it in action, run any peripheral sample (for example the
:doc:`UART sample <uart>`) and follow that section; the board's pin
configuration is in the board's ``*-pinctrl.dtsi``.

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`GPIO <gpio>`
- :doc:`UART <uart>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr pin control introduction <https://docs.zephyrproject.org/latest/hardware/pinctrl/index.html>`_
- `Zephyr pin control API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__pinctrl__interface.html>`_
