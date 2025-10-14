Overview
========

Here is the step-by-step guide for running RTK OTA Zephyr Sample. For detailed OTA design specifications, please refer to `RTL8752H Subsystems: OTA <https://docs.realmcu.com/sdk/rtl8752h/common/en/latest/subsystems/ota/text_en/README.html>`_.

Dual Bank OTA Sample
====================

This sample is developed based on rtl8752hkf, which is equipped with a 1MB flash.

Limitation:

1. The sample just shows switching from bank0 to bank1, but not the reverse. If switch from bank1 to bank0 is needed the OTAHeader(including version info) should be changed accordingly.

Start OTA Sample
----------------

Download boot images. Pulling down P0_3 and reset to enter UART download mode, see `[RTL8752H EVB] Hardware Connection and Download Guide <https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/%5BRTL8752H-EVB%5D-Hardware-Connection-and-Download-Guide>`_ for the Download-Mode Wiring (UART).

.. code-block:: sh

   source ../zephyr/zephyr-env.sh
   west realtek-bee mpcli -c com1 --json ${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/dual_bank/mptoolconfig_bank0.json -E

Build OTA APP on bank0 and download it.

.. code-block:: sh

   # Bank0
   west build -p -b rtl8752h_evb/rtl8752hkf samples/bluetooth/peripheral_ota

Now, release the P0_3 and reset again. There should be some logs printed on UART indicating the OTA APP started.

.. code-block:: sh

   compile at Sep 25 2025 14:00:00
   *** Booting Zephyr OS build 21a9c24df93e ***
   Successfully loaded Realtek Lowerstack ROM!
   Starting Bluetooth Peripheral OTA example

Upgrading
---------

Build another APP on bank1.

.. code-block:: sh

   # Bank1
   west build -p always -b rtl8752h_evb/rtl8752hkf samples/hello_world \
   -- -DEXTRA_DTC_OVERLAY_FILE="${ZEPHYR_BASE}/../realtek-zephyr-project/samples/bluetooth/peripheral_ota/boards/rtl8752h_evb_rtl8752hkf_bank1.overlay"

Add image header and md5 to the image.

.. code-block:: sh

   source zephyr-env.sh
   west realtek-bee -d build prepend
   west realtek-bee -d build md5

Copy the image (``zephyr_MP_x.x.x.x_x-xxx.bin``) from build directory to ``${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_dual_bank/unpacked_image_bank1/``.

Pack the images. The packed output will be located at ``${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_dual_bank/packed_image_bank1``.

.. code-block:: sh

   # Copy the default flash layout file
   cp "${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/dual_bank/flash map.ini" \
   ${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_dual_bank/unpacked_image_bank1/

   # pack
   west realtek-bee packcli -n 8752H -m OTA \
   -s ${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_dual_bank/unpacked_image_bank1 \
   -d ${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_dual_bank/packed_image_bank1

Then, try OTA with `Android/iOS OTA APP <https://docs.realmcu.com/sdk/rtl8752h/common/cn/latest/tool_set/text_cn/README.html#android-ota-app>`_. For sample convenience, the "version checking" in APP can be disabled, in this way, the image with lower version also can be updated.

Reference
=========

1. `[RTL8752H EVB] Hardware Connection and Download Guide <https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/%5BRTL8752H-EVB%5D-Hardware-Connection-and-Download-Guide>`_
2. `RTL8752H Subsystems: OTA <https://docs.realmcu.com/sdk/rtl8752h/common/en/latest/subsystems/ota/text_en/README.html>`_
