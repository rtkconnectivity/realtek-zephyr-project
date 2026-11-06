.. _pinctrl:

Pinctrl
=======

The pin controller driver configures the Bee pads through the standard Zephyr
pinctrl API. A pad can be put in *pinmux mode* (the pad is driven directly by
the peripheral hardware) or in *software mode* (``SW_MODE``), which sets the
pad's electrical properties — pull, direction, drive level, and drive
current. Pin configuration is grouped per device and per device *state* (for
example ``default``), and each peripheral driver applies its pins
automatically (at init, and in the v3.7 driver also during PM actions), so
application code normally never calls the pinctrl API directly.

This chapter also explains how pads, pinmux, pinctrl, and GPIO relate, and how
pinctrl underpins the cross-peripheral DLPS (deep low-power state) behavior used
elsewhere in this chapter.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Pinmux: route a supported peripheral function to a pad (for example
  ``UART2_TX`` to ``P3_0``). The full function and pad list is in the per-SoC
  pinctrl header.
- Per-pad electrical configuration: pull up / down / none, input or output
  direction, drive level (high/low), a four-level driving current, and support
  for a strong pull resistor.
- Per-device states: each device declares one configuration node per state
  (``default``, and ``sleep`` for a driver that uses it), and the driver
  applies the matching state.
- DLPS pull: a pad can hold a separate pull while the SoC is in DLPS
  (``sleep-hardware-state``).
- DLPS wakeup: a pad can wake the SoC on a high or low level during DLPS.
  Only the UART, GPIO, and KSCAN drivers arm it, each for its own pads; there
  is no wakeup property in pinctrl.
- Special pad modes: software-controlled mode (``SW_MODE``) and per-SoC
  dedicated function muxes (see the per-SoC note below).

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``pinctrl`` (shared by all on-chip peripherals).
- Bindings file: ``dts/bindings/pinctrl/realtek,bee-pinctrl.yaml``
  (compatible ``realtek,bee-pinctrl``).
- BEE_PSEL macro: ``include/zephyr/dt-bindings/pinctrl/bee-pinctrl.h``.
- Per-SoC function-ID / pad-name headers:
  ``include/zephyr/dt-bindings/pinctrl/rtl87x2j-pinctrl.h``,
  ``rtl87x2g-pinctrl.h``, and ``rtl8752h-pinctrl.h``.
- SoC pinctrl data / decoding: ``soc/realtek/bee/common/pinctrl_soc.h``.
- Kconfig option: ``PINCTRL_BEE`` (``drivers/pinctrl/Kconfig.bee``).
- Source file: ``drivers/pinctrl/pinctrl_bee.c``.
- Example board files: the per-board ``*-pinctrl.dtsi``, for example
  ``boards/realtek/rtl87x2j_evb/rtl87x2j_evb_rtl8762jth-pinctrl.dtsi``,
  ``boards/realtek/rtl87x2g_evb_a/rtl87x2g_evb_a_gkh_gku-pinctrl.dtsi``, and
  ``boards/realtek/rtl8752h_evb/rtl8752h_evb-pinctrl.dtsi``.
- Reference:

  - `Zephyr pin control introduction <https://docs.zephyrproject.org/latest/hardware/pinctrl/index.html>`_
  - `Zephyr pin control API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__pinctrl__interface.html>`_

.. note::

   Per-SoC differences:

   - **Available pads differ:** rtl87x2j exposes pads ``P0_0``–``P7_1``,
     rtl87x2g ``P0_0``–``P10_2``, and rtl8752h ``P0_0``–``P5_2``.
   - **Available functions differ:** each SoC's ``*-pinctrl.h`` header defines
     its own function set.
   - **Drive current:** ``current-level`` (0–3) maps to different milliamp values
     on each SoC and depends on ``VDDIO``. See the binding for the exact
     per-SoC/VDDIO table.
   - **Convenience aliases:** all three SoCs provide ready-made
     ``BEE_PSEL_GPIOA_<n>_Px_y`` aliases for GPIO pad mapping. (On the v3.7
     rtl87x2g / rtl8752h driver these aliases do not exist.)
   - **sleep-hardware-state:** on a SoC with a PCK600 (such as rtl87x2j) the
     pad hardware parks the pads on its own across DLPS with the pull of the
     ``default`` state, and a ``sleep-hardware-state`` group in that state is
     needed only to park them with a different pull. A SoC without a PCK600
     (such as rtl87x2g / rtl8752h) ignores such a group.

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
  ``P3_0``). One pad can be driven by exactly one function at a time.
- **Pinmux** — the hardware multiplexer that decides *which* peripheral function
  a pad carries (for example ``UART2_TX``, or a GPIO).
- **Pinctrl** — the Zephyr layer that configures both, declaratively from
  devicetree: it picks the pinmux function for a pad and sets the pad's
  electrical properties (pull, direction, drive, current).
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

     #include <zephyr/dt-bindings/pinctrl/rtl87x2j-pinctrl.h>

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

  ``BEE_PSEL(fun, pin)`` encodes one pad: ``fun`` is a function name from the
  SoC's ``*-pinctrl.h`` header (without the ``BEE_`` prefix, for example
  ``UART2_TX``), and ``pin`` is a pad name (for example ``P3_0``). Include the
  header for your SoC (``rtl87x2j-pinctrl.h``, ``rtl87x2g-pinctrl.h``, or
  ``rtl8752h-pinctrl.h``).

  Pads that share the same properties go in one group. A pad in pinmux mode is
  driven by its peripheral, so the direction and level set here do not matter
  while the peripheral is active; they only matter when the pad is parked in
  DLPS (see :ref:`Power Management (DLPS) <pinctrl_dlps>`).

- *Example*: the ``uart2_default`` node in
  ``boards/realtek/rtl87x2j_evb/rtl87x2j_evb_rtl8762jth-pinctrl.dtsi``; on
  rtl87x2g it is in
  ``boards/realtek/rtl87x2g_evb_a/rtl87x2g_evb_a_gkh_gku-pinctrl.dtsi`` and on
  rtl8752h in ``boards/realtek/rtl8752h_evb/rtl8752h_evb-pinctrl.dtsi``, on the
  same P3_0/P3_1 pads.

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
  - ``sleep-hardware-state`` — the group does not configure the pad; it only
    sets the pull the pad hardware applies on its own while the SoC is in
    DLPS. Put such a group in the ``default`` state, after the group that
    configures the same pad. Only the pull is used; the function, direction,
    and drive of the group are ignored (see
    :ref:`Power Management (DLPS) <pinctrl_dlps>` below).

  .. code-block:: devicetree

     &pinctrl {
         kscan_default: kscan_default {
             group1 {
                 psels = <BEE_PSEL(KEY_ROW_0, P0_6)>;
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
  properties and name them with ``pinctrl-names``. State ``0`` is
  conventionally ``default`` (applied at init):

  .. code-block:: devicetree

     &uart2 {
         pinctrl-0 = <&uart2_default>;
         pinctrl-names = "default";
         status = "okay";
     };

  Application code does not call the pinctrl API; the peripheral driver
  applies the ``default`` state at init. Add a ``sleep`` state (``pinctrl-1``
  named ``"sleep"``) only for a driver that uses it; see *Sleep state* in
  :ref:`Power Management (DLPS) <pinctrl_dlps>`, which also describes how the
  pads are handled across DLPS.

- *Example*: the ``&uart2`` node in
  ``boards/realtek/rtl87x2j_evb/rtl87x2j_evb_rtl8762jth.dts``. On rtl87x2g and
  rtl8752h the node is in
  ``boards/realtek/rtl87x2g_evb_a/rtl87x2g_evb_a_common.dtsi`` and
  ``boards/realtek/rtl8752h_evb/rtl8752h_evb_common.dtsi``.

.. _pinctrl_dlps:

Power Management (DLPS)
-----------------------
DLPS (deep low-power state) is the SoC's main sleep mode. The system enters DLPS
when idle and leaves it on a wakeup event (a software-timer wakeup, or a pad
wakeup). Most handling is transparent to the application — the drivers and the
pad hardware take care of the peripheral state and the pinctrl configurations
— so the application just keeps using the standard Zephyr APIs.

- *Parking pads in DLPS*: on a SoC with a PCK600 (such as rtl87x2j) the pad
  mode is switched **by hardware**. Every pad in PINMUX mode is parked in
  ``SW_MODE`` on entering DLPS and restored to PINMUX mode on exit, so the
  peripheral drivers do not switch pinctrl states across DLPS. LPPWM and GPIO
  keep their output across DLPS.

  While parked in ``SW_MODE``, the pad takes the direction and output level
  set in its ``default`` state. In PINMUX mode the peripheral drives the pin,
  so a pad configured with ``output-enable`` and ``output-high`` /
  ``output-low`` may show a different level while active, but it is driven to
  the configured level once the hardware parks it in DLPS. To keep a pad from
  being driven high or low on entering DLPS, configure it with
  ``output-disable`` in its ``default`` state: the peripheral still drives it
  in PINMUX mode, and the hardware parks it as an input in ``SW_MODE``. The
  rtl87x2j board files configure every PINMUX pad this way.

- *Pull in DLPS*: a parked pad keeps the pull of its ``default`` state. Choose
  that pull to match the level the external device holds, so no current leaks
  across the pad and the line does not float.

  Add a group marked ``sleep-hardware-state`` to the ``default`` state only
  when a pad must hold a different pull in DLPS than while active. The group
  lists the pads in ``SW_MODE`` and sets only the pull used in DLPS. In this
  example the pads have no pull while active and are pulled up in DLPS:

  .. code-block:: devicetree

     &pinctrl {
         uart2_default: uart2_default {
             group1 {
                 psels = <BEE_PSEL(UART2_TX, P3_0)>,
                         <BEE_PSEL(UART2_RX, P3_1)>;
                 output-disable;
                 bias-disable;
             };
             group2 {
                 psels = <BEE_PSEL(SW_MODE, P3_0)>,
                         <BEE_PSEL(SW_MODE, P3_1)>;
                 sleep-hardware-state;
                 bias-pull-up;
             };
         };
     };

  The ``sleep-hardware-state`` group must come after the group that configures
  the same pad. Only these pads support a separate pull in DLPS: ``P0_0`` to
  ``P0_2``, ``P0_4`` to ``P0_6``, ``P1_0``, ``P1_1``, ``P2_0`` to ``P2_7``,
  ``P3_0`` to ``P3_3``, ``P4_0`` to ``P4_3``, and the SPIC pads. On any other
  pad the group has no effect.

- *Sleep state*: on a SoC with a PCK600 (such as rtl87x2j) the pads are
  managed by hardware across DLPS, as described above, so no ``sleep`` state
  is needed for DLPS. The ``sleep`` state is only used by a driver that
  supports deinit: it switches the pinctrl configuration to the ``sleep``
  state when the device is deinitialized (for example the UART, see the
  :doc:`UART chapter <uart>`).

- *Pad wakeup*: the peripheral driver (UART, GPIO, or KSCAN) picks the wakeup
  pad and polarity itself and arms the wakeup under its own conditions; there
  is no wakeup property in pinctrl. See each of those chapters for how to
  enable it.

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
