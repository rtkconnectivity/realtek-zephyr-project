Clock Control
=============

The clock control driver manages the Bee peripheral clocks (RCC) through the
standard Zephyr clock control API. There is a single controller node, ``cctl``,
that gates each peripheral's APB clock on or off. Clock handling is transparent
to the application: every peripheral declares its clock in devicetree and its
driver enables that clock automatically at init, so application code normally
never calls the clock controller directly.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Enable or disable a peripheral's APB clock.
- Predefined clock IDs for every peripheral as ``APB_CLK(<peri>)`` macros.
- Clock-status query is available on **rtl87x2g only**; it is not implemented
  for rtl8752h or rtl87x2j.
- Rate query and set are **not** supported; the SoC clock tree is fixed.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``cctl`` (shared by all on-chip peripherals).
- Bindings file: ``dts/bindings/clock/realtek,bee-cctl.yaml``
  (compatible ``realtek,bee-cctl``).
- Clock-ID headers: ``include/zephyr/dt-bindings/clock/rtl87x2g-clocks.h``,
  ``rtl8752h-clocks.h``, and ``rtl87x2j-clocks.h``.
- Controller-reference header:
  ``include/zephyr/drivers/clock_control/bee_clock_control.h``
  (provides ``BEE_CLOCK_CONTROLLER``).
- Kconfig option: ``CLOCK_CONTROL_BEE``
  (``drivers/clock_control/Kconfig.bee``).
- Source file: ``drivers/clock_control/clock_control_bee.c``.
- Reference:

  - `Zephyr clock control introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/clock_control.html>`_
  - `Zephyr clock control API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__clock__control__interface.html>`_

The ``cctl`` node is a standalone ``clock-controller`` node with its own
``reg``, carrying the ``#clock-cells = <1>`` cell.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: ``cctl`` is instead a
   child of an ``rcu`` (reset-clock-controller) node and has no ``reg`` of its
   own — the driver reads the register base from the parent.

Operation Flow
--------------

Assigning a clock to a peripheral
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: a peripheral node names its clock with the standard
  ``clocks`` property, ``clocks = <&cctl APB_CLK(<peri>)>``, where ``<peri>`` is
  one of the IDs in the SoC's ``*-clocks.h`` header (for example ``UART0``,
  ``GPIOA``). This is already set for every on-chip peripheral in the SoC
  devicetree, so applications normally do not change it.

  .. code-block:: devicetree

     uart0: serial@40011000 {
         compatible = "realtek,bee-uart";
         clocks = <&cctl APB_CLK(UART0)>;
         ...
     };

- *Example*: the ``clocks`` property on the peripheral nodes in the SoC
  devicetree — ``dts/arm/realtek/bee/rtl87x2g.dtsi``, ``rtl8752h.dtsi``, and
  ``rtl87x2j.dtsi``.

Enabling a peripheral clock
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: applications do not call the clock controller directly. Each
  peripheral driver enables its own clock during init with
  ``clock_control_on()``, passing the controller handle from the
  ``BEE_CLOCK_CONTROLLER`` macro and the clock cell read back from devicetree
  with ``DT_INST_CLOCKS_CELL()``. Use the same pattern only if you write a
  custom driver:

  .. code-block:: c

     #include <zephyr/drivers/clock_control/bee_clock_control.h>

     uint16_t clkid = DT_INST_CLOCKS_CELL(0, id);

     clock_control_on(BEE_CLOCK_CONTROLLER, (clock_control_subsys_t)&clkid);

- *Example*: the ``clock_control_on()`` call in ``drivers/gpio/gpio_bee.c`` (and
  in every other Bee peripheral driver at init).

Power Management (DLPS)
-----------------------
- *Behavior*: the clock controller holds no state of its own, and it contains
  no PM/DLPS code. DLPS handling is transparent to the application: each
  peripheral driver re-enables its clock as part of its own DLPS restore (on the
  SoCs where that peripheral supports PM), so no clock-related action is
  required after DLPS exit.

Samples and Logs
----------------
The clock controller has no standalone sample. It is exercised indirectly by
every peripheral test in this chapter, since each peripheral enables its clock
through ``cctl`` at init. To see it in action, run any peripheral sample (for
example the :doc:`GPIO <gpio>` sample) and follow that section.

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`GPIO <gpio>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr clock control introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/clock_control.html>`_
- `Zephyr clock control API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__clock__control__interface.html>`_
