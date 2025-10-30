Overview
========

This document provides a step-by-step guide to running the Realtek Peripheral OTA sample (both single bank and dual bank plan). For detailed OTA design specifications, please refer to the `RTL8752H Subsystems: OTA <https://docs.realmcu.com/sdk/rtl8752h/common/en/latest/subsystems/ota/text_en/README.html>`_.

Single Bank OTA Sample
======================

This sample is developed based on rtl8752htv with a 512KB flash, it also can run with rtl8752hkf, which is equipped with a 1024KB flash.

Start OTA Sample
----------------

**1. Download boot images.**
   
Pulling down P0_3 and reset to enter UART download mode. For details on UART wiring, please refer to the `[RTL8752H EVB] Hardware Connection and Download Guide <https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/%5BRTL8752H-EVB%5D-Hardware-Connection-and-Download-Guide>`_ for the Download-Mode Wiring (UART). Open GIT Bash and execute:

.. code-block:: sh

   cd realtek-zephyr-project
   # Linux/macOS
   source ../zephyr/zephyr-env.sh
   west realtek-bee mpcli -c comX --json bin/rtl8752h/single_bank/mptoolconfig.json -E
   # Windows
   ..\zephyr\zephyr-env.cmd
   west realtek-bee mpcli -c comX --json bin/rtl8752h/single_bank/mptoolconfig.json -E

.. note::
   
   - Replace comX with your actual serial port, e.g., com7.

**2. Build and flash OTA sample.**

.. code-block:: sh

   west build -p always -b rtl8752h_evb/rtl8752htv samples/bluetooth/peripheral_ota
   west flash

**3. Release the P0_3 and reset again.**

The console will display logs similar to below, indicating OTA APP is running:

.. code-block:: sh

   Compile at Sep 25 2025 13:00:00
   *** Booting Zephyr OS build 21a9c24df93e ***
   Successfully loaded Realtek Lowerstack ROM!
   Starting Bluetooth Peripheral OTA example

Upgrading Procedure
-------------------

**1. Build a new APP image.**

.. code-block:: sh

   west build -d build_ota -p always -b rtl8752h_evb/rtl8752htv samples/bluetooth/peripheral_ota

**2. Add realtek's image header and md5 to the image.**

.. code-block:: sh

   west realtek-bee -d build_ota prepend
   west realtek-bee -d build_ota md5

**3. Pack the images.**

Copy the newly generated image (such as ``zephyr_MP_x.x.x.x_x-xxx.bin``) in  from the ``build_ota`` directory to ``realtek-zephyr-project/samples/bluetooth/peripheral_ota/bin/single_bank/unpacked_image/``. And run the commands below to pack the images:

.. code-block:: sh

   west realtek-bee packcli -n 8752H -m OTA \
   -s samples/bluetooth/peripheral_ota/bin/single_bank/unpacked_image \
   -d samples/bluetooth/peripheral_ota/bin/single_bank/packed_image

The packaged file will be generated in  ``realtek-zephyr-project/samples/bluetooth/peripheral_ota/bin/single_bank/packed_image/OTA/``.

**4. Test OTA Upgrading.**

Try OTA with `Android/iOS OTA APP <https://docs.realmcu.com/sdk/rtl8752h/common/cn/latest/tool_set/text_cn/README.html#android-ota-app>`_. For sample convenience, the "version checking" in APP can be disabled, in this way, the image with lower version also can be updated. 

.. note::

   - The device name for the peripheral app is  ``Zephyr OTA``.
   - The APP's compile timestamp will be printed in the console and can be used to verify whether the upgraded APP has started successfully.


Dual Bank OTA Sample
======================

This sample is developed based on developed based on **rtl8752hkf**, which is equipped with a 1024KB flash.

Start OTA Sample
----------------

**1. Download boot images.**
   
Pulling down P0_3 and reset to enter UART download mode. For details on UART wiring, please refer to the `[RTL8752H EVB] Hardware Connection and Download Guide <https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/%5BRTL8752H-EVB%5D-Hardware-Connection-and-Download-Guide>`_ for the Download-Mode Wiring (UART). Open CMD Line and execute:

.. code-block:: sh

   cd realtek-zephyr-project
   # Linux/macOS
   source ../zephyr/zephyr-env.sh
   west realtek-bee mpcli -c comX --json bin/rtl8752h/dual_bank/bank0/mptoolconfig_bank0.json -E
   # Windows
   ..\zephyr\zephyr-env.cmd
   west realtek-bee mpcli -c comX --json bin/rtl8752h/dual_bank/bank0/mptoolconfig_bank0.json -E

.. note::

   - Replace comX with your actual serial port, e.g., com7.

**2. Build and flash OTA sample.**

Please ensure that you add the qualifier rtl8752hkf after rtl8752h_evb.

.. code-block:: sh

   west build -p -b rtl8752h_evb/rtl8752hkf samples/bluetooth/peripheral_ota
   west flash

**3. Release the P0_3 and reset again.**

The console will display logs similar to below, indicating OTA APP is running:

.. code-block:: sh

   Compile at Sep 25 2025 14:00:00
   *** Booting Zephyr OS build 21a9c24df93e ***
   Successfully loaded Realtek Lowerstack ROM!
   Starting Bluetooth Peripheral OTA example

Upgrading Procedure
-------------------

**1. Build a new APP image on BANK1.**

.. code-block:: sh

   west build -d build_ota_bank1 -p always -b rtl8752h_evb/rtl8752hkf samples/bluetooth/peripheral_ota \
   -- -DEXTRA_DTC_OVERLAY_FILE="%ZEPHYR_BASE%/../realtek-zephyr-project/samples/bluetooth/peripheral_ota/boards/rtl8752h_evb_rtl8752hkf_bank1.overlay"


**2. Add realtek's image header and md5 to the image.**

.. code-block:: sh

   west realtek-bee -d build_ota_bank1 prepend
   west realtek-bee -d build_ota_bank1 md5

**3. Pack the images.**

Copy the newly generated image (such as ``zephyr_MP_x.x.x.x_x-xxx.bin``) in  from the ``build_ota_bank1`` directory to ``realtek-zephyr-project/samples/bluetooth/peripheral_ota/bin/dual_bank/unpacked_image_bank1/``. And run the commands below to pack the images:

.. code-block:: sh

   west realtek-bee packcli -n 8752H -m OTA \
   -s samples/bluetooth/peripheral_ota/bin/dual_bank/unpacked_image_bank1 \
   -d samples/bluetooth/peripheral_ota/bin/dual_bank/packed_image_bank1

The packaged file will be generated in  ``realtek-zephyr-project/samples/bluetooth/peripheral_ota/bin/dual_bank/packed_image_bank1/OTA/``.

**4. Test OTA Upgrading.**

Try OTA with `Android/iOS OTA APP <https://docs.realmcu.com/sdk/rtl8752h/common/cn/latest/tool_set/text_cn/README.html#android-ota-app>`_. For sample convenience, the "version checking" in APP can be disabled, in this way, the image with lower version also can be updated. 

.. note::

   - The device name for the peripheral app is  ``Zephyr OTA``.
   - The APP's compile timestamp will be printed in the console and can be used to verify whether the upgraded APP has started successfully.

References
==========

1. `[RTL8752H EVB] Hardware Connection and Download Guide <https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/%5BRTL8752H-EVB%5D-Hardware-Connection-and-Download-Guide>`_
2. `RTL8752H Subsystems: OTA <https://docs.realmcu.com/sdk/rtl8752h/common/en/latest/subsystems/ota/text_en/README.html>`_
