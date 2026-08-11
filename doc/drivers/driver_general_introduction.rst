.. _driver_general_introduction:

Drivers General Introduction
============================

This document describes the downstream Zephyr driver support for the Realtek
**Bee** SoC family — **rtl87x2g**, **rtl8752h**, and **rtl87x2j**. It is intended
for users who develop applications and maintain drivers on these SoCs. Each
peripheral is exposed through the standard Zephyr driver API; the per-peripheral
sections in this chapter describe the features of each driver and walk through
example usage based on the in-tree driver tests and samples.

All three SoCs share the same unified ``bee`` drivers and ``realtek,bee-*``
compatibles. Genuine differences between the SoCs are called out in **Note**
blocks throughout.

To locate specific information for a peripheral, see the indicated part of that
peripheral's section:

- How many nodes each SoC supports for the peripheral — the *Device nodes* entry
  under *Basic Information*.
- The detailed bindings and Kconfig options — the *Bindings file* and *Kconfig
  options* entries under *Basic Information*.
- The peripheral's driver / API introduction — the *Reference* links under
  *Basic Information*.
- Configuration examples — the *Example board files* entry under *Basic
  Information*.
- Usage examples of those configurations — the *Samples and Logs* section.

For the usage of
`DTS <https://docs.zephyrproject.org/latest/build/dts/index.html>`_ and
`Kconfig <https://docs.zephyrproject.org/latest/build/kconfig/index.html>`_,
and for the API of each peripheral, refer to the official Zephyr documentation
linked in each section. For hardware details (register descriptions, timing,
electrical characteristics, etc.), refer to the official Realtek documentation:
`Realtek RealMCU <https://www.realmcu.com/en/Home>`_.

Requirements
------------
- Hardware: a Realtek Bee EVB — one of ``rtl87x2g_evb``, ``rtl8752h_evb``, or
  ``rtl87x2j_evb``. EVBs can be purchased from the
  `Realtek RealMCU Shop <https://www.realmcu.com/en/Home/Shop>`_.
- Software: a Zephyr development environment with ``west`` and the Zephyr SDK
  toolchain installed. See the
  `Zephyr Getting Started Guide
  <https://docs.zephyrproject.org/latest/develop/getting_started/index.html>`_.

How Peripherals Are Configured
------------------------------
Zephyr describes the hardware declaratively in devicetree and enables drivers
through Kconfig. The driver framework brings each peripheral up automatically
before the application starts, so the application does not perform any manual
clock, pin, or interrupt setup.

- *Pin mux and pad*: pin assignment and pad attributes (direction, pull, drive
  strength, wakeup) are configured through ``pinctrl`` groups in the devicetree,
  not in C. See the :doc:`Pinctrl section <pinctrl>`.
- *Enabling a peripheral*: set ``status = "okay"`` on the peripheral node in a
  board overlay and enable the matching ``CONFIG_*`` option (for example
  ``CONFIG_SERIAL`` for UART). Each peripheral section lists the options it
  needs.
- *Clocks and interrupts*: peripheral clock (RCC) and interrupt wiring are
  handled inside the drivers via the :doc:`Clock Control <clock_control>` driver
  and ``IRQ_CONNECT``; no manual clock-enable or NVIC setup is required in the
  application.
- *Power management (DLPS)*: DLPS (Deep Low Power State) is the SoC's main sleep
  mode — the system enters it when idle and leaves it on a wakeup event to save
  power. Where it is supported, each driver stores and restores its own hardware
  state across DLPS, so the application mostly uses the standard Zephyr APIs.
  DLPS support varies by driver and SoC (see below), and each peripheral's
  *Power Management (DLPS)* note describes any action the application still has
  to take.

.. note::

   DLPS availability: power management is **not uniform** across the family, and
   how it works depends on whether the SoC has a PCK600 power sequencer.

   On a SoC without a PCK600 (such as rtl87x2g / rtl8752h), enabling ``CONFIG_PM``
   lets the platform manage the overall DLPS behavior — the idle thread decides
   whether entering DLPS is currently allowed. Enabling ``CONFIG_PM_DEVICE`` then
   lets each peripheral driver handle its own DLPS behavior in software (for
   example maintaining its software state, switching between the ``default`` and
   ``sleep`` pinctrl states, and storing/restoring registers).

   On a SoC with a PCK600 (such as rtl87x2j), enabling ``CONFIG_PM`` lets the hardware
   handle each module's DLPS enter/exit flow and behavior automatically and
   independently (for example automatically storing/restoring registers and
   switching between the pre-configured ``default`` and ``sleep`` pinctrl states),
   with no software intervention required.

   Two things are worth separating in either case. The DLPS enter/exit
   transition itself is transparent: the driver (without a PCK600) or the
   hardware (with a PCK600) stores and restores state, so the application needs
   no reconfiguration and keeps using the same active-state APIs after wakeup. An
   operation that is *in progress* is a separate matter. On a SoC with a PCK600
   the hardware holds the system awake while a peripheral is busy, so an active
   transfer is never cut off. On a SoC without a PCK600 nothing blocks DLPS
   automatically: if the system enters DLPS while a peripheral is working the
   peripheral loses power and the operation stops, so the application must keep
   the system out of DLPS (for example with a DLPS check flag) while the
   peripheral is actively in use.

   Each section's *Power Management (DLPS)* note states exactly what its driver
   does.

.. note::

   PM availability by release: the rtl87x2g and rtl8752h PM (DLPS) functionality
   is currently supported only on Zephyr **v3.7** — on **v4.4** these two SoCs
   provide only their active (non-PM) functionality. The rtl87x2j PM
   functionality is currently supported only on Zephyr **v4.4**.

Building and Running
--------------------
The samples in this chapter are in-tree examples — mostly the driver tests under
``tests/drivers/<peripheral>/``, with some under ``samples/drivers/``. Each
peripheral section names the exact path of its sample(s). Build and flash with
``west``, choosing the board that matches your EVB:

.. code-block:: console

   # rtl87x2g
   west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/gpio/gpio_basic_api

   # rtl8752h
   west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/gpio/gpio_basic_api

   # rtl87x2j
   west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/gpio/gpio_basic_api

   west flash --port <your-flash-serial-port>

Each peripheral section names its sample(s) and lists any extra wiring,
devicetree overlay, or Kconfig needed to run them.

.. note::

   Examples in this chapter use the three Bee EVB board targets above. In an
   actual product the board name will differ; refer to the board definition
   provided by your application and replace the board target accordingly. Not
   every sample supports every SoC — a section notes when a sample is
   board-specific.

.. _driver_logging_note:

Logging
-------
The default, always-available way to view output on a Bee EVB is the **native
Zephyr console on uart2**. All three EVBs choose ``uart2`` as the console and
shell UART and enable it in their board files, so the interactive sample tests
(which read the characters you type) work out of the box:

.. code-block:: kconfig

   CONFIG_CONSOLE=y
   CONFIG_UART_CONSOLE=y
   CONFIG_SERIAL=y
   CONFIG_UART_INTERRUPT_DRIVEN=y

.. code-block:: devicetree

   / {
       chosen {
           zephyr,console = &uart2;
           zephyr,shell-uart = &uart2;
       };
   };

   &uart2 {
       status = "okay";
       current-speed = <115200>;
   };

To read the native console, connect a serial tool to ``uart2`` and set it to
match the board: **115200 baud, no parity, 1 stop bit, 8 data bits**.

.. note::

   Internal-log UART: each SoC reserves one UART for the Realtek internal log
   system (per the DTSI comments): ``uart1`` on **rtl87x2g** and **rtl8752h**,
   and ``uart3`` on **rtl87x2j**. On every board this log is routed out through
   ``P0_3``, and it must be viewed with the Realtek dedicated log tool
   **DebugAnalyzer** rather than a plain serial terminal. This internal-log route
   is not covered further here — use the native uart2 console above unless your
   board explicitly documents it.

When a section says "follow the Logging note in the Overview", it refers to this
note: view the test output on ``uart2`` with a serial terminal at 115200 baud,
no parity, 1 stop bit, 8 data bits.

.. _driver_how_to_read:

How to Read Each Peripheral Section
-----------------------------------
The peripheral sections share a common layout:

- *Functional Overview*: a Feature List and Basic Information (device nodes,
  bindings file, Kconfig file, source file, example overlay, and reference
  links).
- *Operation Flow*: how to configure and use each transfer mode or feature, with
  a *How to configure* / *How to use* / *Example* breakdown that points at the
  test that exercises it.
- *Power Management (DLPS)*: behavior across DLPS and any application-side
  recovery steps, including which SoCs support it.
- *Samples and Logs*: how to wire and run the sample, plus a captured test log.

Where a peripheral behaves differently across the Bee SoCs — including where
rtl87x2g/rtl8752h's v3.7 driver differs from the v4.4 baseline — that difference
is called out in a **Note** block within the relevant part.

See Also
--------
- :doc:`Drivers chapter index <index>`
- :doc:`Pinctrl <pinctrl>`
- `Zephyr introduction <https://docs.zephyrproject.org/latest/introduction/index.html>`_
- `Zephyr devicetree guide <https://docs.zephyrproject.org/latest/build/dts/index.html>`_
- `Zephyr Kconfig guide <https://docs.zephyrproject.org/latest/build/kconfig/index.html>`_
- `Realtek RealMCU <https://www.realmcu.com/en/Home>`_
