ADC
===

The ADC driver exposes the Bee analog-to-digital converter through the standard
Zephyr ADC API.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Single controller ``adc`` with nine channels (IDs ``0``–``8``). Channel ``8``
  is the internal VBAT channel and needs no external pin.
- External channels ``0``–``7`` sample on pads ``P2_0``–``P2_7`` respectively;
  the pad must be placed in ADC ``SW_MODE`` through pinctrl.
- Resolution is fixed at 12-bit; each selected channel returns a 12-bit sample
  per read.
- Per-channel bypass or divide input mode, selected in devicetree. In divide mode
  (default) the input range is 0–3.3 V; in bypass mode it is 0–0.9 V.
- Gain, reference, acquisition time, and resolution are fixed at their default
  values; any other value passed in the channel setup or read sequence is
  rejected with ``-ENOTSUP``.
- No hardware sampling trigger — drive periodic sampling from a software timer.

.. note::

   The channel set differs by SoC. rtl87x2g and rtl87x2j expose channels
   ``0``–``7`` on ``P2_0``–``P2_7`` plus internal VBAT on channel ``8``. On
   rtl8752h channel ``6`` is **not** available (``P2_0``–``P2_5`` and ``P2_7``
   map to channels ``0``–``5`` and ``7``), with internal VBAT on channel ``8``.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``adc``.
- Bindings file: ``dts/bindings/adc/realtek,bee-adc.yaml``
  (compatible ``realtek,bee-adc``, ``#io-channel-cells = <1>``).
- Kconfig option: ``ADC_BEE`` (``drivers/adc/Kconfig.bee``).
- Source file: ``drivers/adc/adc_bee.c``.
- Example board files (under ``tests/drivers/adc/adc_api/boards/``):
  ``rtl87x2g_evb_a_rtl8762gku.overlay``, ``rtl8752h_evb_rtl8752hjl.overlay``, and
  ``rtl87x2j_evb_rtl8762jth.overlay``.
- Reference:

  - `Zephyr ADC introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/adc.html>`_
  - `Zephyr ADC API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__adc__interface.html>`_

.. note::

   The ``bypass-mode-map`` devicetree property selects each channel's
   bypass/divide input mode: a channel bitmask where a set bit puts that channel
   in bypass mode (0–0.9 V) and a clear bit leaves it in the default divide mode
   (0–3.3 V). For example, ``0x05`` selects bypass mode for channels ``0`` and
   ``2``.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: it selects the same
   bypass/divide input mode through the ``is-bypass-mode`` devicetree property
   instead of ``bypass-mode-map``.

Operation Flow
--------------

Enabling an ADC device
~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set ``status = "okay"`` on the ``adc`` node and add a
  ``channel@N`` node for each channel you sample, using the standard
  ``zephyr,gain`` / ``zephyr,reference`` / ``zephyr,acquisition-time`` /
  ``zephyr,resolution`` properties. The channel number must match the pad it
  samples — for example ``channel@4`` (``reg = <4>``) samples on pad ``P2_4``.
  There is no ADC pinmux: each external channel's pad is simply configured as
  ``SW_MODE`` input through the ``adc`` node's ``pinctrl-0`` group (see
  :doc:`Pinctrl <pinctrl>`). Channel ``8`` (internal VBAT) needs no pad.

  - The ``zephyr,gain``, ``zephyr,reference``, ``zephyr,acquisition-time``, and
    ``zephyr,resolution`` parameters only support their default values
    (``ADC_GAIN_1``, ``ADC_REF_INTERNAL``, ``ADC_ACQ_TIME_DEFAULT``, and 12-bit);
    any other value is rejected with ``-ENOTSUP``.
  - Select bypass/divide input mode with the ``bypass-mode-map`` property
    described in the note above.

  See ``dts/bindings/adc/realtek,bee-adc.yaml`` for more details.

  .. code-block:: devicetree

     / {
         zephyr,user {
             io-channels = <&adc 4>, <&adc 8>;
         };
     };

     &adc {
         status = "okay";
         pinctrl-0 = <&adc_default>;
         pinctrl-names = "default";
         #address-cells = <1>;
         #size-cells = <0>;

         channel@4 {
             reg = <4>;
             zephyr,gain = "ADC_GAIN_1";
             zephyr,reference = "ADC_REF_INTERNAL";
             zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
             zephyr,resolution = <12>;
         };

         channel@8 {                 /* internal VBAT, no pad */
             reg = <8>;
             zephyr,gain = "ADC_GAIN_1";
             zephyr,reference = "ADC_REF_INTERNAL";
             zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
             zephyr,resolution = <12>;
         };
     };

     &pinctrl {
         adc_default: adc_default {
             group1 {
                 psels = <BEE_PSEL(SW_MODE, P2_4, DIR_IN, DRV_LOW, PULL_DOWN)>;
             };
         };
     };

  Enable ADC support in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_ADC=y

- *Example*: the ``&adc`` node and its channels in
  ``tests/drivers/adc/adc_api/boards/rtl87x2g_evb_a_rtl8762gku.overlay`` (channels
  ``4`` and ``8``); the ``rtl87x2j_evb_rtl8762jth.overlay`` maps channels ``2``
  and ``8`` instead.

Reading a channel
~~~~~~~~~~~~~~~~~~
- *How to use*: after configuring each channel with ``adc_channel_setup()``,
  describe the read with a ``struct adc_sequence`` (set ``channels`` to the
  channel bitmask, ``resolution`` to ``12``, and point ``buffer`` at an
  ``int16_t`` buffer with one entry per selected channel), then call
  ``adc_read()`` for a blocking read or ``adc_read_async()`` for an asynchronous
  one. The driver reads each channel's raw conversion, converts it through the
  channel's divide or bypass transfer function, and normalizes the result back
  onto the 12-bit full-scale range, so the value you read is a linear 12-bit
  sample regardless of the channel's input mode. Each sample is written into an
  ``int16_t`` buffer as a normalized 12-bit raw value; convert it to a voltage
  in the application from the channel's input range.

  .. code-block:: c

     int16_t sample;
     struct adc_sequence seq = {
         .channels    = BIT(4),
         .buffer      = &sample,
         .buffer_size = sizeof(sample),
         .resolution  = 12,
     };

     adc_read(adc, &seq);   /* sample now holds a 12-bit value */

- *Example*: single- and multi-channel reads are covered by
  ``adc_basic.test_adc_sample_one_channel`` and
  ``adc_basic.test_adc_sample_two_channels``; the asynchronous path by
  ``adc_basic.test_adc_asynchronous_call``.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: it returns each sample
   as a voltage in millivolts written into an ``int32_t`` buffer, instead of the
   normalized 12-bit raw value returned on v4.4.

Repeated and interval sampling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: because there is no hardware trigger, repeat ``adc_read()`` from
  a software timer for periodic sampling. ``adc_sequence_options``
  (``extra_samplings`` / ``interval_us``) drives a sequence of back-to-back
  samples.
- *Example*: ``adc_basic.test_adc_repeated_samplings`` and
  ``adc_basic.test_adc_sample_with_interval``.

Power Management (DLPS)
-----------------------
- *Behavior*: on a SoC with a PCK600 (such as rtl87x2j), the PCK600 manages
  power in hardware: the ADC blocks DLPS while a conversion is in progress and
  allows it again once sampling completes; this is transparent to the
  application.

.. note::

   On rtl87x2g / rtl8752h, the v3.7 driver differs here: these SoCs have no
   PCK600, so the driver implements a full ``PM_DEVICE`` DLPS handler instead.
   Entering and leaving DLPS is transparent to the application — it needs no
   reconfiguration and keeps using the same active-state API after wakeup. The
   handler does not, however, protect a conversion that is in progress: if the
   system enters DLPS while the ADC is sampling, the ADC loses power and the
   conversion stops. The application must keep the system out of DLPS (for
   example with a DLPS check flag) while a conversion is in progress.

Samples and Logs
----------------
Sample: ``tests/drivers/adc/adc_api``.

How to run
~~~~~~~~~~
- *Wiring*: apply a known voltage (within the channel's range) to the external
  ADC input pad; the api test validates driver behavior and does not require a
  specific level. Channel ``8`` (internal VBAT) needs no wiring. The actual
  pads are set by the board overlay and differ per SoC.
- *Extra config*: none — the board overlay enables ``adc`` and declares the
  channel(s).
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku tests/drivers/adc/adc_api

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   tests/drivers/adc/adc_api

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   tests/drivers/adc/adc_api

     west flash --port <your-flash-serial-port>

To view the log, follow the :ref:`Logging note in the Overview <driver_logging_note>`.

A successful run looks like this:

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0-174-g8855e3ecc827 ***
   Running TESTSUITE adc_basic
   ===================================================================
   START - test_adc_asynchronous_call
   Samples read: 0x01b2 0x01b2 0x01b1 0x01b2 0x01b1 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
    PASS - test_adc_asynchronous_call in 0.013 seconds
   ===================================================================
   START - test_adc_invalid_request
   E: Unsupported resolution 0, only 12-bit supported
   E: Unsupported resolution 0, only 12-bit supported
   Samples read: 0x01b1 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
    PASS - test_adc_invalid_request in 0.021 seconds
   ===================================================================
   START - test_adc_repeated_samplings
   repeated_samplings_callback: done 1
   Samples read: 0x01b2 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 2
   Samples read: 0x01b2 0x0fff 0x01b2 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 3
   Samples read: 0x01b2 0x0fff 0x01b1 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 4
   Samples read: 0x01b2 0x0fff 0x01b2 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 5
   Samples read: 0x01b2 0x0fff 0x01b1 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 6
   Samples read: 0x01b2 0x0fff 0x01b2 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 7
   Samples read: 0x01b2 0x0fff 0x01b1 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 8
   Samples read: 0x01b2 0x0fff 0x01b2 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 9
   Samples read: 0x01b2 0x0fff 0x01b2 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
   repeated_samplings_callback: done 10
   Samples read: 0x01b2 0x0fff 0x01b2 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
    PASS - test_adc_repeated_samplings in 0.141 seconds
   ===================================================================
   START - test_adc_sample_one_channel
   Samples read: 0x01b1 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
    PASS - test_adc_sample_one_channel in 0.012 seconds
   ===================================================================
   START - test_adc_sample_two_channels
   Samples read: 0x01b1 0x0fff 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
    PASS - test_adc_sample_two_channels in 0.012 seconds
   ===================================================================
   START - test_adc_sample_with_interval
   sample_with_interval_callback: sampling 0
   sample_with_interval_callback: sampling 1
   sample_with_interval_callback: sampling 2
   sample_with_interval_callback: sampling 3
   sample_with_interval_callback: sampling 4
   Samples read: 0x01b1 0x01b2 0x01b1 0x01b2 0x01b1 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000
    PASS - test_adc_sample_with_interval in 0.416 seconds
   ===================================================================
   START - test_task_different_priorities_sequences
    SKIP - test_task_different_priorities_sequences in 0.001 seconds
   ===================================================================
   TESTSUITE adc_basic succeeded

   ------ TESTSUITE SUMMARY START ------

   SUITE PASS - 100.00% [adc_basic]: pass = 6, fail = 0, skip = 1, total = 7 duration = 0.616 seconds
    - PASS - [adc_basic.test_adc_asynchronous_call] duration = 0.013 seconds
    - PASS - [adc_basic.test_adc_invalid_request] duration = 0.021 seconds
    - PASS - [adc_basic.test_adc_repeated_samplings] duration = 0.141 seconds
    - PASS - [adc_basic.test_adc_sample_one_channel] duration = 0.012 seconds
    - PASS - [adc_basic.test_adc_sample_two_channels] duration = 0.012 seconds
    - PASS - [adc_basic.test_adc_sample_with_interval] duration = 0.416 seconds
    - SKIP - [adc_basic.test_task_different_priorities_sequences] duration = 0.001 seconds

   ------ TESTSUITE SUMMARY END ------

   ===================================================================
   PROJECT EXECUTION SUCCESSFUL

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`Pinctrl <pinctrl>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr ADC introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/adc.html>`_
- `Zephyr ADC API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__adc__interface.html>`_
