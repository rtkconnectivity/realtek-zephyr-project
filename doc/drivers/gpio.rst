GPIO
====

The GPIO driver exposes the Bee general-purpose I/O through the standard Zephyr
GPIO API.

On the Bee family a logical GPIO (for example ``GPIOA_0``) is a peripheral
control line that is distinct from a physical pad (for example ``P0_0``). The
logical GPIO is routed to a pad through **pinctrl**, so every GPIO port node
needs a ``pinctrl-0`` group. See the :doc:`Pinctrl <pinctrl>` chapter for the
pad/pinmux/GPIO relationship.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- GPIO ports of 32 logical pins each (for the pins actually bonded on a given
  package, refer to that package's datasheet on
  `Realtek RealMCU <https://www.realmcu.com/>`_).
- Standard set/get operations, output high/low level, input level read.
- Push-pull and (on rtl87x2g / rtl87x2j) open-drain outputs.
- Edge-, level-, and both-edge-triggered interrupts with per-pin callbacks.
- Hardware debounce (0–255 ms).
- Wakeup from DLPS: armed automatically when an interrupt is enabled on a pin.
  See *Power Management (DLPS)* below.

Basic Information
~~~~~~~~~~~~~~~~~
- Device nodes:

  - rtl87x2g: ``gpioa``, ``gpiob``.
  - rtl8752h: ``gpioa``.
  - rtl87x2j: ``gpioa``, ``gpiob``.
- Bindings file: ``dts/bindings/gpio/realtek,bee-gpio.yaml``
  (compatible ``realtek,bee-gpio``).
- Extension-flags header:
  ``include/zephyr/dt-bindings/gpio/realtek-bee-gpio.h``
  (provides ``BEE_GPIO_INPUT_DEBOUNCE_MS()``).
- Kconfig option: ``GPIO_BEE`` (``drivers/gpio/Kconfig.bee``).
- Source file: ``drivers/gpio/gpio_bee.c``.
- Example board files (under ``tests/drivers/gpio/gpio_basic_api/boards/``):
  ``rtl87x2g_evb_a_rtl8762gku.overlay`` (rtl87x2g),
  ``rtl8752h_evb_rtl8752hjl.overlay`` (rtl8752h), and
  ``rtl87x2j_evb_rtl8762jth.overlay`` (rtl87x2j).
- Reference:

  - `Zephyr GPIO introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html>`_
  - `Zephyr GPIO API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html>`_

The ``#gpio-cells`` is ``2`` — ``<pin flags>`` — so a consumer references a pin
as, for example, ``gpios = <&gpioa 0 GPIO_ACTIVE_LOW>``.

Operation Flow
--------------

Enabling a GPIO port and mapping pins to pads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Using a GPIO on Bee takes **two coordinated pieces of devicetree, and they must
correspond**:

1. The usual GPIO **consumer reference** — ``<&gpio<port> <pin> <flags>>`` — that
   your node (an LED, a button, the sample's ``out-gpios`` / ``in-gpios``, ...)
   points at, naming the logical pin you want to use.
2. A ``pinctrl-0`` group on that ``gpio<port>`` node that contains a
   ``BEE_PSEL_GPIO<port>_<pin>_P<x>_<y>`` entry mapping **that same logical pin**
   to the physical pad you wired.

Set ``status = "okay"`` on the ``gpioa`` / ``gpiob`` node and give it the
``pinctrl-0`` group (named through ``pinctrl-names``). The ``pinctrl-0`` /
``pinctrl-names`` properties are **required** by the binding.

.. code-block:: devicetree

   &gpioa {
       pinctrl-0 = <&gpioa_default>;
       pinctrl-names = "default";
       status = "okay";
   };

   &pinctrl {
       gpioa_default: gpioa_default {
           group1 {
               /* map GPIOA_0 -> pad P0_0, GPIOA_2 -> pad P0_2 */
               psels = <BEE_PSEL_GPIOA_0_P0_0>,
                       <BEE_PSEL_GPIOA_2_P0_2>;
           };
       };
   };

**How it works.** The ``pinctrl-0`` group is how the driver learns which pad each
logical pin uses. At init the driver reads the ``default`` pinctrl state and
builds an internal *logical-pin-to-pad* table, **registering** only the pins
listed in the group; the runtime GPIO calls then look a pin up in that table. So
every logical pin your code references with ``<&gpio<port> <pin> ...>`` must have
a matching ``BEE_PSEL_GPIO<port>_<pin>_...`` entry in the group — in the example
above, referencing ``&gpioa 0`` and ``&gpioa 2`` works because both are
registered, but referencing ``&gpioa 5`` (not in the group) would not.

**Why it is required — the main reason is pad remapping.** A single logical GPIO
can be reached from more than one pad — a *one-to-many* pad-to-GPIO mapping — and
only one mapping can be active at a time, so the board must select which pad backs
a given GPIO line. That choice is a hardware configuration, so it belongs in
devicetree: you make it by configuring the ``BEE_PSEL_GPIO<port>_<pin>_P<x>_<y>``
alias for the pad you wired, which keeps the routing alongside the rest of the
board's pin configuration.

**A secondary benefit is reviewability.** A bare ``<&gpio<port> <pin>>`` reference
names only a logical GPIO number, which does not make it obvious *which physical
pad* the pin drives — so a devicetree that configured GPIOs by number alone was
easy to get wrong and hard to review. Spelling out both the logical pin and the
pad in one ``BEE_PSEL_GPIO<port>_<pin>_P<x>_<y>`` macro makes the pin-to-pad
choice visible in the devicetree and reduces mistakes.

Only this pad-mapping step is done in devicetree; direction and level are then
switched at runtime with ``gpio_pin_configure()`` and the standard GPIO calls.

.. note::

   A few pads are reserved for debug by default, so avoid choosing them as the
   pad that backs a GPIO line unless you first free them: ``P0_3`` carries the
   RTK internal log output, and ``P1_0`` / ``P1_1`` are used as the SWD debug
   interface (SWCLK and SWDIO). See the :doc:`Pinctrl <pinctrl>` chapter for the
   full list.

.. note::

   The consumer reference and the pinctrl group must line up. If a pin is
   referenced through the GPIO API but has no matching ``BEE_PSEL_GPIO*`` entry
   in the port's ``pinctrl-0`` group, it is never registered at init, and a
   later ``gpio_pin_configure()`` (or other call) on that pin trips a debug
   assertion (``__ASSERT``, message ``gpio port or pin error``): it panics when
   ``CONFIG_ASSERT`` is enabled and is compiled out otherwise, so build with
   asserts enabled to catch this during development. If the ``gpio<port>`` node
   has no ``pinctrl-0`` at all, the driver fails to initialize (``-EIO``) with
   the log line ``GPIO relate pins should be configured on dts pinctrl node``.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: there is **no pinctrl
   step at all** — you do not configure a ``pinctrl-*`` group, and you just call
   the standard GPIO API directly to use a pin. The node carries a ``port``
   property (``0`` = GPIOA, ``1`` = GPIOB — rtl8752h has only ``port = <0>`` on
   its single ``gpio`` node), and the logical-pin-to-pad mapping for multiplexed
   pins is chosen at build time with Kconfig options such as
   ``CONFIG_BEE_USE_P6_2_AS_GPIOB21``. The node is enabled with just
   ``status = "okay"``. Because this remapping is hardware-dependent rather than a
   build-time choice, moving it into pinctrl/devicetree is the main reason the
   GPIO node now requires a pinctrl group.

- *Example*: the ``&gpioa`` node and its ``gpioa_default`` pinctrl group in the
  ``rtl87x2g_evb_a_rtl8762gku.overlay`` / ``rtl8752h_evb_rtl8752hjl.overlay`` /
  ``rtl87x2j_evb_rtl8762jth.overlay`` files under
  ``tests/drivers/gpio/gpio_basic_api/boards/``.

Output and input
~~~~~~~~~~~~~~~~~
- *How to use*: configure the direction with ``GPIO_OUTPUT`` / ``GPIO_INPUT`` in
  ``gpio_pin_configure()``, then drive or read the pin with the standard
  ``gpio_pin_set()`` / ``gpio_pin_get()`` calls. Requesting a pin as both input
  and output at once returns ``-ENOTSUP``.
- *Open-drain*: OR ``GPIO_OPEN_DRAIN`` into the flags for an open-drain output.
  ``GPIO_OPEN_SOURCE`` is not supported (returns ``-ENOTSUP``).
- *Example*: basic set/get and raw/logical level behavior is covered by
  ``gpio_port.test_gpio_port``.

.. note::

   Open-drain output is SoC-specific: it is supported on **rtl87x2g** and
   **rtl87x2j** only. On **rtl8752h** the output mode is fixed push-pull, so
   ``GPIO_OPEN_DRAIN`` has no effect there and an open-drain test is skipped.

Interrupts and callbacks
~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: register a callback with ``gpio_init_callback()`` +
  ``gpio_add_callback()``, then arm the trigger with
  ``gpio_pin_interrupt_configure()``. Edge, level, and **both-edge**
  (``GPIO_INT_EDGE_BOTH``) triggers are all supported, with active-high or
  active-low polarity.
- *Example*: callback management is covered by the ``gpio_port_cb_mgmt.*``
  tests, and the edge/level/both-edge trigger combinations by
  ``gpio_port_cb_vari.test_gpio_callback_variants``.

.. note::

   Both-edge triggers are supported on Bee. The Bee driver emulates them with a
   level interrupt whose polarity is flipped in the ISR.

Hardware debounce
~~~~~~~~~~~~~~~~~~
- *How to configure*: OR the ``BEE_GPIO_INPUT_DEBOUNCE_MS(ms)`` flag (from
  ``realtek-bee-gpio.h``) into the GPIO flags, in devicetree or at runtime in
  ``gpio_pin_configure()``. The range is **0–255 ms**; the hardware debounces the
  input for that time before the interrupt fires. This is a Bee-specific
  extension that combines with the generic ``GPIO_*`` flags and is available on
  all three SoCs.

  In devicetree:

  .. code-block:: devicetree

     #include <zephyr/dt-bindings/gpio/realtek-bee-gpio.h>

     in-gpios = <&gpioa 2 (GPIO_ACTIVE_HIGH | BEE_GPIO_INPUT_DEBOUNCE_MS(8))>;

  The flags set in devicetree can be read back with ``DT_GPIO_FLAGS()`` and
  passed to ``gpio_pin_configure()``, or the debounce flag can be OR-ed in
  directly at runtime:

  .. code-block:: c

     #define PIN_IN_FLAGS DT_GPIO_FLAGS(DT_INST(0, test_gpio_basic_api), in_gpios)

     gpio_pin_configure(dev_in, PIN_IN,
                        GPIO_INPUT | PIN_IN_FLAGS | BEE_GPIO_INPUT_DEBOUNCE_MS(8));

Power Management (DLPS)
-----------------------
A SoC with a PCK600 and a SoC without one use the same logic here. A GPIO
configured as an output holds its output level across DLPS, and a GPIO
configured as an input automatically arms wakeup across DLPS when you enable an
interrupt on the pin, so entering and leaving DLPS is transparent to the GPIO
user. There is no separate wakeup flag. The only power-management feature is
**pad wakeup**, armed through pinctrl when an interrupt is enabled. When
``gpio_pin_interrupt_configure()`` enables an interrupt, the driver calls the
pinctrl wakeup path (``pinctrl_bee_wakeup_config()``,
``PINCTRL_BEE_WAKEUP_PPU``) for that pad; disabling the interrupt disarms it.
The driver rejects (``-ENOTSUP``) arming an edge trigger whose configured level
already matches the current pad level, to avoid an immediate self-trigger.

.. code-block:: c

   gpio_pin_configure(dev_in, PIN_IN, GPIO_INPUT);
   gpio_init_callback(&cb, wake_handler, BIT(PIN_IN));
   gpio_add_callback(dev_in, &cb);
   /* enabling the interrupt also arms DLPS wakeup */
   gpio_pin_interrupt_configure(dev_in, PIN_IN, GPIO_INT_EDGE_TO_ACTIVE);

DLPS handling is otherwise transparent: keep using the pin through its normal
active-state API, and the callback fires after DLPS exit. See the
:doc:`Pinctrl <pinctrl>` chapter for the underlying pad wakeup mechanism.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so the driver implements a full ``PM_DEVICE`` DLPS handler instead. It
   stores and restores the port state across DLPS, plus a dedicated
   ``BEE_GPIO_INPUT_PM_WAKEUP`` flag to arm a pin as a wakeup source. OR that flag
   into the pin's GPIO flags to arm it as a DLPS wakeup source; the driver's
   ``PM_DEVICE`` action saves and restores the port registers across sleep, so
   the pin resumes with its configuration intact.

Samples and Logs
----------------
Sample: ``tests/drivers/gpio/gpio_basic_api``.

How to run
~~~~~~~~~~
- *Wiring*: short the output pad to the input pad (the ``out-gpios`` /
  ``in-gpios`` pins mapped by the board overlay's ``gpioa_default`` group).
  The actual pads are set by the board overlay and differ per SoC.
- *Extra config*: none — each board overlay already enables its GPIO port(s) and
  maps the pads.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/gpio/gpio_basic_api

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/gpio/gpio_basic_api

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/gpio/gpio_basic_api

     west flash --port <your-flash-serial-port>

  The matching overlay under ``boards/`` selects that board's pins.

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

.. note::

   The ``gpio_basic_api`` suite includes an open-drain configuration check. On
   rtl87x2g / rtl87x2j it runs; on **rtl8752h** it is skipped because the SoC has
   no open-drain output (see the open-drain note above). Both-edge trigger checks
   run on all three SoCs.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-170-g633ddfc0ad13 ***
   Running TESTSUITE after_flash_gpio_config_trigger
   ===================================================================
   START - test_gpio_config_trigger
    PASS - test_gpio_config_trigger in 0.011 seconds
   ===================================================================
   START - test_gpio_config_twice_trigger
    PASS - test_gpio_config_twice_trigger in 0.011 seconds
   ===================================================================
   TESTSUITE after_flash_gpio_config_trigger succeeded
   Running TESTSUITE gpio_port
   ===================================================================
   START - test_gpio_port
   Validate device gpio@4001a000 and gpio@4001a000
   Check gpio@4001a000 output 0 connected to gpio@4001a000 input 1
   OUT 0 to IN 1 linkage works
   - bits_physical
   - pin_physical
   - check_raw_output_levels
   - check_logic_output_levels
   - check_input_levels
   - bits_logical
   - check_pulls
    PASS - test_gpio_port in 0.026 seconds
   ===================================================================
   TESTSUITE gpio_port succeeded
   Running TESTSUITE gpio_port_cb_mgmt
   ===================================================================
   START - test_gpio_callback_add_remove
   callback_2 triggered: 1
   callback_1 triggered: 1
   callback_2 triggered: 1
    PASS - test_gpio_callback_add_remove in 3.610 seconds
   ===================================================================
   START - test_gpio_callback_enable_disable
   callback_2 triggered: 1
   callback_1 triggered: 1
   callback_2 triggered: 1
   callback_1 triggered: 1
    PASS - test_gpio_callback_enable_disable in 3.612 seconds
   ===================================================================
   START - test_gpio_callback_self_remove
   callback_remove_self triggered: 1
   callback_1 triggered: 1
   callback_1 triggered: 1
    PASS - test_gpio_callback_self_remove in 2.510 seconds
   ===================================================================
   TESTSUITE gpio_port_cb_mgmt succeeded
   Running TESTSUITE gpio_port_cb_vari
   ===================================================================
   START - test_gpio_callback_variants
   callback triggered: 1
   OUT init a0001, IN cfg 3400000, cnt 1
   callback triggered: 1
   OUT init 60000, IN cfg 5400000, cnt 1
   callback triggered: 1
   OUT init 60000, IN cfg 5c00000, cnt 1
   callback triggered: 1
   OUT init a0001, IN cfg 3c00000, cnt 1
   callback triggered: 1
   callback triggered: 2
   callback triggered: 3
   OUT init 60000, IN cfg 4400000, cnt 3
   callback triggered: 1
   callback triggered: 2
   callback triggered: 3
   OUT init a0001, IN cfg 2400000, cnt 3
   callback triggered: 1
   callback triggered: 2
   callback triggered: 3
   OUT init 60000, IN cfg 4c00000, cnt 3
   callback triggered: 1
   callback triggered: 2
   callback triggered: 3
   OUT init a0001, IN cfg 2c00000, cnt 3
   callback triggered: 1
   callback triggered: 2
   OUT init a0001, IN cfg 7400000, cnt 2
    PASS - test_gpio_callback_variants in 9.973 seconds
   ===================================================================
   TESTSUITE gpio_port_cb_vari succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [after_flash_gpio_config_trigger]: pass = 2, fail = 0, skip = 0, total = 2 duration = 0.022 seconds
    - PASS - [after_flash_gpio_config_trigger.test_gpio_config_trigger] duration = 0.011 seconds
    - PASS - [after_flash_gpio_config_trigger.test_gpio_config_twice_trigger] duration = 0.011 seconds

   SUITE PASS - 100.00% [gpio_port]: pass = 1, fail = 0, skip = 0, total = 1 duration = 0.026 seconds
    - PASS - [gpio_port.test_gpio_port] duration = 0.026 seconds

   SUITE PASS - 100.00% [gpio_port_cb_mgmt]: pass = 3, fail = 0, skip = 0, total = 3 duration = 9.732 seconds
    - PASS - [gpio_port_cb_mgmt.test_gpio_callback_add_remove] duration = 3.610 seconds
    - PASS - [gpio_port_cb_mgmt.test_gpio_callback_enable_disable] duration = 3.612 seconds
    - PASS - [gpio_port_cb_mgmt.test_gpio_callback_self_remove] duration = 2.510 seconds

   SUITE PASS - 100.00% [gpio_port_cb_vari]: pass = 1, fail = 0, skip = 0, total = 1 duration = 9.973 seconds
    - PASS - [gpio_port_cb_vari.test_gpio_callback_variants] duration = 9.973 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :doc:`Clock Control <clock_control>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr GPIO introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html>`_
- `Zephyr GPIO API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html>`_
- `Realtek RealMCU <https://www.realmcu.com/>`_
