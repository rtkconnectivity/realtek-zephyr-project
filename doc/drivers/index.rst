.. _drivers:

=======
Drivers
=======

This chapter describes the downstream Zephyr driver support for the Realtek
**Bee** SoC family — **rtl87x2g**, **rtl8752h**, and **rtl87x2j**. Each
peripheral is exposed through the standard Zephyr driver API and is configured
declaratively in devicetree and Kconfig.

Start with the :doc:`General Introduction <driver_general_introduction>` for the
conventions shared by every peripheral section (how peripherals are configured,
how to build and run the samples, and how to view the log). The
:doc:`Pinctrl <pinctrl>` section also explains how pads, pinmux, pinctrl, and
GPIO relate, and covers the cross-peripheral wakeup/DLPS concepts that the
per-peripheral sections build on.

Each peripheral section is divided into the following parts:

+ Functional Overview (Feature List and Basic Information)
+ Operation Flow
+ Power Management (DLPS)
+ Samples and Logs

For what each part covers, see
:ref:`How to Read Each Peripheral Section <driver_how_to_read>` in the General
Introduction.

.. _driver_supported_socs:

Supported SoCs and versions
---------------------------

These docs describe the Bee family — **rtl87x2g**, **rtl8752h**, and
**rtl87x2j** — on the **v4.4** downstream release. All three SoCs use the same
unified ``bee`` drivers and ``realtek,bee-*`` devicetree compatibles, and unless a
section says otherwise, everything below applies to all three.

**rtl87x2g** and **rtl8752h** additionally have an earlier **v3.7** release, in
which some peripherals' usage or configuration differs from v4.4. In particular,
on v3.7 the rtl87x2g and rtl8752h drivers implement ``PM_DEVICE`` to handle the
DLPS flow for a number of peripherals, whereas on v4.4 only rtl87x2j provides
PM-related functionality. Wherever v3.7 and v4.4 differ in peripheral usage or
configuration, the relevant section calls it out in a note; anything a section
does not explicitly flag is identical between v3.7 and v4.4.

.. rubric:: Sections

.. toctree::
   :maxdepth: 1

   General Introduction <driver_general_introduction>
   Clock Control <clock_control>
   Pinctrl <pinctrl>
   GPIO <gpio>
   UART <uart>
   DMA <dma>
   ADC <adc>
   Counter <counter>
   PWM <pwm>
   RTC <rtc>
   SPI <spi>
   I2C <i2c>
   CAN <can>
   I2S <i2s>
   SDHC <sdhc>
   IR <ir>
   KSCAN <kscan>
   QDEC <qdec>
   USB <usb>
