Overview
========

Here is the step-by-step guide for running RTK OTA Zephyr Sample. For detailed OTA design specifications, please refer to `RTL8752H Subsystems: OTA <https://docs.realmcu.com/sdk/rtl8752h/common/en/latest/subsystems/ota/text_en/README.html>`_.

Single Bank OTA Sample
======================

This sample is developed based on rtl8752htv, it also can run with rtl8752hkf, which is equipped with a larger flash, but not the other way around.

Start OTA Sample
----------------

Download boot images. Pulling down P0_3 and reset to enter UART download mode, see `[RTL8752H EVB] Hardware Connection and Download Guide <https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/%5BRTL8752H-EVB%5D-Hardware-Connection-and-Download-Guide>`_ for the Download-Mode Wiring (UART).

.. code-block:: sh

   source zephyr-env.sh
   west realtek-bee mpcli -c com1 --json ${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/mptoolconfig.json -E

Build OTA APP image and download it.

.. code-block:: sh

   # Bank0
   west build -p always -b rtl8752h_evb/rtl8752htv samples/bluetooth/peripheral_ota
   west flash

Now, release the P0_3 and reset again. There should be some logs printed on UART indicating the OTA APP started.

.. code-block:: sh

   compile at Sep 25 2025 13:00:00
   *** Booting Zephyr OS build 21a9c24df93e ***
   Successfully loaded Realtek Lowerstack ROM!
   Starting Bluetooth Peripheral OTA example

Upgrading
---------

Build another APP image.

.. code-block:: sh

   # Bank0
   west build -d build_ota -p always -b rtl8752h_evb/rtl8752htv samples/bluetooth/peripheral_ota

Add image header and md5 to the image.

.. code-block:: sh

   source zephyr-env.sh
   west realtek-bee -d build_ota prepend
   west realtek-bee -d build_ota md5

Copy the image (``zephyr_MP_x.x.x.x_x-xxx.bin``) from ``build_ota`` directory to ``${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_single_bank/unpacked_image/``.

Pack the images. The packed output will be located at ``${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_single_bank/packed_image/OTA``.

.. code-block:: sh

   # Copy the default flash layout file
   cp "${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/flash map.ini" \
   ${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_single_bank/unpacked_image/
   # Pack
   west realtek-bee packcli -n 8752H -m OTA \
   -s ${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_single_bank/unpacked_image \
   -d ${ZEPHYR_BASE}/../realtek-zephyr-project/bin/RTL8752H/ota_sample/ota_image_single_bank/packed_image

Then, try OTA with `Android/iOS OTA APP <https://docs.realmcu.com/sdk/rtl8752h/common/cn/latest/tool_set/text_cn/README.html#android-ota-app>`_. For sample convenience, the "version checking" in APP can be disabled, in this way, the image with lower version also can be updated.

The compiled time of APP is involved in logs, it can be used to check if the upgraded APP has started.

Reference
=========

1. `[RTL8752H EVB] Hardware Connection and Download Guide <https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/%5BRTL8752H-EVB%5D-Hardware-Connection-and-Download-Guide>`_
2. `RTL8752H Subsystems: OTA <https://docs.realmcu.com/sdk/rtl8752h/common/en/latest/subsystems/ota/text_en/README.html>`_
