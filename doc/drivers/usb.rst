USB
===

The USB driver exposes the Bee USB controller through the standard Zephyr USB
device stack.

The controller is a Synopsys DesignWare (DWC2) OTG 2.0 core and is driven by the
upstream ``udc_dwc2`` driver; the Bee integration adds only a vendor quirk that
powers up the USB PHY and advertises the core's High-Speed capability. Device
enumeration and class handling are done by the USB device stack
(``USB_DEVICE_STACK_NEXT``); the application only picks a device class and
registers it. The sections below describe the classes that are supported on Bee:
HID, CDC ACM, Mass Storage, and USB Audio Class 2 (UAC2).

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Full-Speed and High-Speed device operation.
- Six IN and six OUT endpoints in addition to the bidirectional control
  endpoint (64-byte control packets).
- Remote-wakeup support.
- HID, CDC ACM, Mass Storage (MSC), and UAC2 device classes.

Basic Information
~~~~~~~~~~~~~~~~~
- Device node: ``udc`` (``udc@40100000``); it carries the ``zephyr_udc0`` alias
  and is already enabled on the EVB boards, so a USB sample builds without an
  overlay.
- Bindings file: ``dts/bindings/usb/realtek,bee-udc.yaml``
  (compatible ``realtek,bee-udc``, which includes ``snps,dwc2``).
- Kconfig option: ``UDC_DWC2`` (``drivers/usb/udc/Kconfig.dwc2``); selected
  automatically when the node is enabled. The device stack is
  ``USB_DEVICE_STACK_NEXT``.
- Source files: ``drivers/usb/udc/udc_dwc2.c`` (the upstream DWC2 driver) and
  ``drivers/usb/udc/udc_dwc2_realtek_bee.h`` (the Bee vendor quirk).
- Reference:

  - `Zephyr USB device support introduction <https://docs.zephyrproject.org/latest/connectivity/usb/device_next/usb_device.html>`_
  - `Zephyr USB device API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__usbd__api.html>`_

Operation Flow
--------------

Enabling USB device support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: enable the device stack and the class you need in
  ``prj.conf`` (the per-class option is given under each class below); the
  ``zephyr_udc0`` node is already active on the EVB boards.

  .. code-block:: kconfig

     CONFIG_USB_DEVICE_STACK_NEXT=y

- *How to use*: the application builds a ``usbd_context``, registers one or more
  class instances against it, then attaches to the bus. The order is:

  - Build and initialize the context: create it with ``USBD_DEVICE_DEFINE()``,
    add the string descriptors (``usbd_add_descriptor()``) and a configuration
    (``usbd_add_configuration()``), register the class instances
    (``usbd_register_class()`` or ``usbd_register_all_classes()``), then call
    ``usbd_init()``. The USB samples wrap all of this in
    ``sample_usbd_init_device()`` from ``samples/subsys/usb/common``; use it as
    a template.
  - Attach to the bus with ``usbd_enable()``.

  .. code-block:: c

     struct usbd_context *ctx = sample_usbd_init_device(NULL);

     if (ctx == NULL || usbd_enable(ctx)) {
         return -ENODEV;
     }

Using a HID device
~~~~~~~~~~~~~~~~~~~
- *How to configure*: set ``CONFIG_USBD_HID_SUPPORT=y``. A HID device instance
  is instantiated from a ``zephyr,hid-device`` devicetree node.
- *How to use*: get the instance with ``DEVICE_DT_GET_ONE(zephyr_hid_device)``,
  register a report descriptor and callbacks with ``hid_device_register()``
  before ``usbd_enable()``, then push input reports with
  ``hid_device_submit_report()``. See ``samples/subsys/usb/hid-mouse``, which
  reports pointer movement and button state as a USB mouse.

  .. code-block:: c

     hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
     hid_device_register(hid_dev, report_desc, sizeof(report_desc), &ops);
     /* after usbd_enable(): */
     hid_device_submit_report(hid_dev, sizeof(report), report);

Using a CDC ACM device
~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set ``CONFIG_USBD_CDC_ACM_CLASS=y``. Each ACM instance is
  a ``zephyr,cdc-acm-uart`` devicetree node and appears to the application as a
  standard UART device.
- *How to use*: get the UART device with
  ``DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart)`` and drive it through the normal
  Zephyr UART API — ``uart_irq_callback_set()`` with ``uart_fifo_read()`` /
  ``uart_fifo_fill()`` for data, and ``uart_line_ctrl_get()`` for DTR and baud
  rate. See ``samples/subsys/usb/cdc_acm``, which echoes received bytes; on the
  host the device enumerates as a CDC ACM serial port.

Using a Mass Storage device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set ``CONFIG_USBD_MSC_CLASS=y`` and back each logical unit
  with a disk (a RAM disk, or the SD/eMMC disk from the :doc:`SDHC <sdhc>`
  driver). ``USBD_MSC_LUNS_PER_INSTANCE`` sets how many units one MSC instance
  exposes.
- *How to use*: declare each unit at build time with ``USBD_DEFINE_MSC_LUN()``,
  giving the disk name and the SCSI inquiry strings; the context init then
  registers the class like any other. See ``samples/subsys/usb/mass``. The host
  mounts the units as removable drives.

  .. code-block:: c

     USBD_DEFINE_MSC_LUN(sd, "SD", "Zephyr", "SD", "0.00");

Using a USB Audio (UAC2) device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set ``CONFIG_USBD_AUDIO2_CLASS=y`` and describe the audio
  topology (terminals and streaming interfaces) in a ``zephyr,uac2`` devicetree
  node. An audio path is usually fed from an I2S stream, so it also enables
  ``CONFIG_I2S=y``.
- *How to use*: get the instance with ``DEVICE_DT_GET()`` on the UAC2 node,
  register the terminal and data callbacks with ``usbd_uac2_set_ops()`` before
  ``usbd_enable()``, and push microphone samples to the host with
  ``usbd_uac2_send()`` (typically once per SOF). See the Bee microphone sample
  ``samples/drivers/uac-mic``, which streams an IN-only path fed from the I2S
  receive stream.

  .. code-block:: c

     const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(uac2_mic));
     usbd_uac2_set_ops(dev, &uac2_ops, &ctx);
     /* per SOF, after usbd_enable(): */
     usbd_uac2_send(dev, terminal_id, buf, size);

Power Management (DLPS)
-----------------------
- *Behavior*: the driver registers no ``PM_DEVICE`` handler. Enabling the
  controller powers up the USB PHY through the Bee vendor quirk, and disabling
  it with ``usbd_disable()`` powers the PHY back down.

Samples and Logs
----------------
Each supported class has a corresponding sample:

- HID: ``samples/subsys/usb/hid-mouse``.
- CDC ACM: ``samples/subsys/usb/cdc_acm``.
- Mass Storage: ``samples/subsys/usb/mass``.
- UAC2: the Bee microphone sample ``samples/drivers/uac-mic``.

How to run
~~~~~~~~~~
The steps below use the CDC ACM sample as the representative case.

- *Wiring*: connect the EVB USB device port to a host with a USB cable.
- *Extra config*: none beyond the sample's own ``prj.conf`` — ``zephyr_udc0`` is
  already enabled on the EVB boards.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku samples/subsys/usb/cdc_acm

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   samples/subsys/usb/cdc_acm

     west flash --port <your-flash-serial-port>

On the host the device enumerates as the class it registered — a CDC ACM serial
port for the sample above. To view the device console, follow the
:ref:`Logging note in the Overview <driver_logging_note>`.

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`SDHC <sdhc>`
- :doc:`I2S <i2s>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr USB device support introduction <https://docs.zephyrproject.org/latest/connectivity/usb/device_next/usb_device.html>`_
- `Zephyr USB device API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__usbd__api.html>`_
