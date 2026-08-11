I2C
===

The I2C driver exposes the Bee I2C controllers through the standard
Zephyr I2C API.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Controller (master) and target (slave) roles.
- Standard (100 kHz), Fast (400 kHz) and Fast-plus (1 MHz) bus speeds, set with
  the ``clock-frequency`` devicetree property and adjustable at runtime.
- 7-bit and 10-bit addressing.
- Synchronous read, write and combined write-read transfers, including
  multi-message ``struct i2c_msg`` arrays.
- Read-back of the last-applied bus configuration.
- Asynchronous (callback) transfers (``CONFIG_I2C_CALLBACK``), one at a time,
  bounded by a per-node transfer timeout.
- Target (slave) mode (``CONFIG_I2C_TARGET``).
- Bus recovery (``CONFIG_I2C_BEE_BUS_RECOVERY``) that bit-bangs the SCL/SDA
  lines through GPIO (``scl-gpios`` / ``sda-gpios``).
- RTIO backend (``CONFIG_I2C_RTIO``) via the generic submit fallback.

Basic Information
~~~~~~~~~~~~~~~~~
- Device nodes:

  - rtl87x2g: ``i2c0``, ``i2c1``, ``i2c2``, ``i2c3``.
  - rtl8752h: ``i2c0``, ``i2c1``.
  - rtl87x2j: ``i2c0``, ``i2c1``.
- Bindings file: ``dts/bindings/i2c/realtek,bee-i2c.yaml``
  (compatible ``realtek,bee-i2c``).
- Kconfig option: ``I2C_BEE`` (``drivers/i2c/Kconfig.bee``).
- Source file: ``drivers/i2c/i2c_bee.c``.
- Example board files: per-SoC overlays under
  ``tests/drivers/i2c/i2c_target_api/boards/`` —
  ``rtl87x2g_evb_a_rtl8762gku.overlay``, ``rtl8752h_evb_rtl8752hjl.overlay`` and
  ``rtl87x2j_evb_rtl8762jth.overlay``.
- Reference:

  - `Zephyr I2C introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/i2c.html>`_
  - `Zephyr I2C API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__i2c__interface.html>`_

Operation Flow
--------------

Enabling an I2C device
~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable an ``i2c`` node and route its SCL/SDA pads through
  pinctrl (pinctrl is required by the binding):

  .. code-block:: devicetree

     &i2c0 {
         status = "okay";
         pinctrl-0 = <&i2c0_default>;
         pinctrl-names = "default";
     };

     &pinctrl {
         i2c0_default: i2c0_default {
             group1 {
                 psels = <BEE_PSEL(I2C0_CLK, P2_2)>,
                         <BEE_PSEL(I2C0_DAT, P2_3)>;
                 output-enable;
                 output-high;
                 bias-pull-up;
             };
         };
     };

  Enable I2C support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_I2C=y

- *Example*: the ``&i2c0`` node in the per-SoC overlays under
  ``tests/drivers/i2c/i2c_target_api/boards/``.

Setting the bus speed
~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set the bus speed in devicetree with ``clock-frequency``
  (in Hz). The ``I2C_BITRATE_STANDARD`` (100 kHz), ``I2C_BITRATE_FAST``
  (400 kHz) and ``I2C_BITRATE_FAST_PLUS`` (1 MHz) macros from
  ``<zephyr/dt-bindings/i2c/i2c.h>`` are convenient values. The driver reads this
  property at init and applies it once as the power-on default; if the property is
  omitted it defaults to ``I2C_BITRATE_STANDARD``.

  .. code-block:: devicetree

     &i2c0 {
         clock-frequency = <I2C_BITRATE_FAST>;   /* 400 kHz */
     };

- *How to use*: to change the speed (or addressing mode) at runtime, call
  ``i2c_configure()`` with the desired configuration bits; this is the single
  entry point that programs the controller, so an application can re-tune the bus
  — for example when the same bus serves devices with different maximum speeds.
  ``i2c_configure()`` accepts controller mode (``I2C_MODE_CONTROLLER``) only;
  to enter target mode use ``i2c_target_register()`` instead.

  .. code-block:: c

     i2c_configure(i2c, I2C_MODE_CONTROLLER | I2C_SPEED_SET(I2C_SPEED_FAST));

Reading and writing
~~~~~~~~~~~~~~~~~~~
- *How to use*: configure the controller once with ``i2c_configure()`` before the
  first transfer (the devicetree ``clock-frequency`` is only the power-on
  default; ``i2c_configure()`` is the call that actually programs the
  controller). Then transfer with ``i2c_write()``, ``i2c_read()`` and
  ``i2c_write_read()`` (or assemble ``struct i2c_msg`` arrays for
  ``i2c_transfer()``), passing the target address. Pass ``I2C_ADDR_10_BITS`` in
  the configuration word when addressing a 10-bit target.

  .. code-block:: c

     uint8_t reg = 0x00, val;

     /* Configure the bus before transferring. */
     uint32_t i2c_config = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

     i2c_configure(i2c_dev, i2c_config);

     i2c_write_read(i2c, 0x68, &reg, 1, &val, 1);

- *Example*: the ``i2c_write_read()`` calls in
  ``tests/drivers/i2c/i2c_target_api/src/main.c``.

Asynchronous transfers
~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: with ``CONFIG_I2C_CALLBACK`` enabled, ``i2c_transfer_cb()`` (and
  the ``i2c_write_cb()`` / ``i2c_read_cb()`` helpers) start a transfer that
  returns immediately and reports completion through the supplied callback. Only
  one asynchronous transfer runs at a time; the call returns ``-EWOULDBLOCK`` if
  the bus is busy. A per-node transfer timeout bounds both synchronous and
  asynchronous transfers and fails them with ``-ETIMEDOUT`` on expiry; it is
  taken from the ``zephyr,transfer-timeout-ms`` devicetree property, falling back
  to ``CONFIG_I2C_TRANSFER_TIMEOUT_MS`` and, when that is ``0``, to an infinite
  wait.

Target (slave) mode
~~~~~~~~~~~~~~~~~~~
- *How to use*: with ``CONFIG_I2C_TARGET`` enabled, register a target with
  ``i2c_target_register()``, passing an ``i2c_target_config`` whose callbacks
  handle the ``write_requested`` / ``write_received`` / ``read_requested`` /
  ``read_processed`` / ``stop`` events; ``i2c_target_unregister()`` returns the
  peripheral to controller mode with its previous configuration. The same
  peripheral cannot be controller and target at once — while a target is
  registered, ``i2c_transfer()`` and ``i2c_configure()`` return ``-EBUSY``.
- *Example*: ``tests/drivers/i2c/i2c_target_api`` (see *Samples and Logs*).

Bus recovery
~~~~~~~~~~~~
- *How to configure*: enable ``CONFIG_I2C_BEE_BUS_RECOVERY`` (on by default when
  ``CONFIG_I2C_BUS_RECOVERY`` is set) and add ``scl-gpios`` and ``sda-gpios`` to
  the I2C node, pointing at the same physical SCL/SDA pins. Recovery selects the
  bit-bang helper (``I2C_BITBANG``) and needs GPIO enabled.
- *How to use*: call ``i2c_recover_bus()`` when a stuck target wedges the bus.
  The driver temporarily muxes SCL/SDA to GPIO, clocks the lines to free the
  target, then hands the pins back to the controller and restores its
  configuration.

Power Management (DLPS)
-----------------------
- *Behavior*: on a SoC with a PCK600 (such as rtl87x2j), the PCK600 manages
  power in hardware. As a master the controller sleeps automatically once a
  transfer completes, so DLPS is transparent to the application. As a target
  (slave), however, once ``i2c_target_register()`` has registered a target the
  system cannot enter sleep while the target is in use; call
  ``i2c_target_unregister()`` to release the target and let the system sleep.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so the driver implements a full ``PM_DEVICE`` DLPS handler that saves
   and restores the controller configuration across the low-power (DLPS)
   transition. Entering and leaving DLPS is transparent to the application — it
   needs no reconfiguration and keeps using the same active-state API after
   wakeup. The handler does not, however, protect a transfer that is in
   progress: if the system enters DLPS mid-transfer the controller loses power
   and the transfer stops. The application must keep the system out of DLPS (for
   example with a DLPS check flag) while a transfer is in progress.

Samples and Logs
----------------
``tests/drivers/i2c/i2c_target_api`` exercises both the controller (master) and
target (slave) roles in a single-role loopback configuration: because one Bee
peripheral cannot be controller and target at the same time, the test loops two
independent on-chip buses (``i2c0`` and ``i2c1``) back to each other on the board
— SCL0 shorted to SCL1 and SDA0 to SDA1 (the ``i2c_bus_short`` fixture). One bus
drives transfers as the controller — full reads, partial reads and
program/read-back through ``i2c_write()`` / ``i2c_write_read()`` — while the
other hosts an EEPROM registered as a target. Board overlays are provided for
all three SoCs.

``tests/drivers/i2c/i2c_target_api``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- *Wiring* (loopback): loop the two on-chip buses back to each other — short
  ``i2c0`` SCL to ``i2c1`` SCL and ``i2c0`` SDA to ``i2c1`` SDA (``i2c_bus_short``
  fixture). The actual pads are set by the board overlay and differ per SoC.
- *Extra config*: none — the per-SoC board overlay enables the ``i2c0`` and
  ``i2c1`` nodes and routes their SCL/SDA pads through pinctrl.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/i2c/i2c_target_api

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/i2c/i2c_target_api

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/i2c/i2c_target_api

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

``test_deinit`` is skipped when ``scl-gpios`` / ``sda-gpios`` are absent from
the I2C node (bus recovery not configured). A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-10360-ge8f1c94ff62b ***
   Running TESTSUITE i2c_eeprom_target
   ===================================================================
   START - test_deinit
   bus gpios not specified in zephyr,path
   SKIP - test_deinit in 0.00
   ===================================================================
   START - test_eeprom_target
   Found EEPROM 0 on I2C bus device i2c@40015000 at addr 54
   Found EEPROM 1 on I2C bus device i2c@40015400 at addr 56
   Testing single-role
   Testing full read: Master: i2c@40015400, address: 0x54
   Testing partial read. Master: i2c@40015400, address: 0x54, off=0
   Testing partial read. Master: i2c@40015400, address: 0x54, off=1
   Testing partial read. Master: i2c@40015400, address: 0x54, off=2
   Testing partial read. Master: i2c@40015400, address: 0x54, off=3
   Testing partial read. Master: i2c@40015400, address: 0x54, off=4
   Testing partial read. Master: i2c@40015400, address: 0x54, off=5
   Testing partial read. Master: i2c@40015400, address: 0x54, off=6
   Testing partial read. Master: i2c@40015400, address: 0x54, off=7
   Testing partial read. Master: i2c@40015400, address: 0x54, off=8
   Testing partial read. Master: i2c@40015400, address: 0x54, off=9
   Testing partial read. Master: i2c@40015400, address: 0x54, off=10
   Testing partial read. Master: i2c@40015400, address: 0x54, off=11
   Testing partial read. Master: i2c@40015400, address: 0x54, off=12
   Testing partial read. Master: i2c@40015400, address: 0x54, off=13
   Testing partial read. Master: i2c@40015400, address: 0x54, off=14
   Testing partial read. Master: i2c@40015400, address: 0x54, off=15
   Testing partial read. Master: i2c@40015400, address: 0x54, off=16
   Testing partial read. Master: i2c@40015400, address: 0x54, off=17
   Testing partial read. Master: i2c@40015400, address: 0x54, off=18
   Testing program. Master: i2c@40015400, address: 0x54, off=0
   Testing program. Master: i2c@40015400, address: 0x54, off=1
   Testing program. Master: i2c@40015400, address: 0x54, off=2
   Testing program. Master: i2c@40015400, address: 0x54, off=3
   Testing program. Master: i2c@40015400, address: 0x54, off=4
   Testing program. Master: i2c@40015400, address: 0x54, off=5
   Testing program. Master: i2c@40015400, address: 0x54, off=6
   Testing program. Master: i2c@40015400, address: 0x54, off=7
   Testing program. Master: i2c@40015400, address: 0x54, off=8
   Testing program. Master: i2c@40015400, address: 0x54, off=9
   Testing program. Master: i2c@40015400, address: 0x54, off=10
   Testing program. Master: i2c@40015400, address: 0x54, off=11
   Testing program. Master: i2c@40015400, address: 0x54, off=12
   Testing program. Master: i2c@40015400, address: 0x54, off=13
   Testing program. Master: i2c@40015400, address: 0x54, off=14
   Testing program. Master: i2c@40015400, address: 0x54, off=15
   Testing program. Master: i2c@40015400, address: 0x54, off=16
   Testing program. Master: i2c@40015400, address: 0x54, off=17
   Testing program. Master: i2c@40015400, address: 0x54, off=18
    PASS - test_eeprom_target in 0.277 seconds
   ===================================================================
   TESTSUITE i2c_eeprom_target succeeded
   ------ TESTSUITE SUMMARY START ------
   SUITE PASS - 100.00% [i2c_eeprom_target]: pass = 1, fail = 0, skip = 1, total = 2 duration = 0.282 seconds
    - SKIP - [i2c_eeprom_target.test_deinit] duration = 0.005 seconds
    - PASS - [i2c_eeprom_target.test_eeprom_target] duration = 0.277 seconds
   ------ TESTSUITE SUMMARY END ------
   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr I2C introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/i2c.html>`_
- `Zephyr I2C API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__i2c__interface.html>`_
