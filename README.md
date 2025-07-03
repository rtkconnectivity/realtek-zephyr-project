# Realtek Support For Zephyr

<p align="center">
  <img src="doc/images/realtek-zephyr-logo.png"/>
</p>

The  [realtek-zephyr-project](https://github.com/rtkconnectivity/realtek-zephyr-project) repository is the entry point for Zephyr development on Realtek Bee Series SoCs. Realtek Zephyr SDK is built on top of the Zephyr Project, reusing the same tools, workflows, and dependencies. 

## What is Zephyr?

The Zephyr Project is a Linux Foundation hosted Collaboration Project. It’s an open source collaborative effort uniting developers and users in building a best-in-class small, scalable, real-time operating system (RTOS) optimized for resource-constrained devices, across multiple architectures.

## What is Realtek Zephyr SDK?

Realtek Zephyr SDK is the Realtek's Zephyr downstream solution, which mainly contains three Github repositories:

- [realtek-zephyr-project](https://github.com/rtkconnectivity/realtek-zephyr-project): Default manifest repo containing Realtek's applications developed based on Zephyr.

- [zephyr](https://github.com/rtkconnectivity/zephyr): Realtek downstream Zephyr containing Realtek Bee Series SoCs support. It is developed on top of tagged release from the Zephyr Upstream (currently is Zephyr v3.7 LTS).

- [hal_realtek](https://github.com/rtkconnectivity/hal_realtek): Hardware Abstraction Layer for Realtek Bee Series SoCs containing driver sources and platform files.

### 📚 Wiki

See the [Realtek Zephyr SDK Wiki](https://github.com/rtkconnectivity/realtek-zephyr-project/wiki) for more information about Realtek Zephyr SDK.

### Supported SoCs & Boards

#### SoCs

- [RTL87X2G](https://www.realmcu.com/en/Home/Product/RTL8762G-RTL877xG-Series)
- [RTL8752H](https://www.realmcu.com/en/Home/Products/RTL8752H-Series)

See the [Realtek Zephyr SDK Wiki](https://github.com/rtkconnectivity/realtek-zephyr-project/wiki#supported-realtek-bee-socs-and-drivers) for detailed drivers and connectivity support status.

#### Boards

- rtl8762gn_evb
- rtl8752htv_evb
- rtl8752h_dongle

### 🚀 Getting Started

For getting started, please refer to the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/3.7.0/develop/getting_started/index.html) for the Zephyr project and follow the same getting-started guide for setting up the environment and building your first application.

> **_NOTE:_** When running `west init` in the getting-started guide it's important to instead run
> `west init -m https://github.com/rtkconnectivity/realtek-zephyr-project zephyrproject`
> in order to use the Realtek Zephyr enablement.

Refer to [Getting started guide for Windows environment](https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/Getting-started-guide-for-Windows-environment) to get the step-by-step guide for Windows platform.

### Versioning

Please refer to [Versioning](https://github.com/rtkconnectivity/realtek-zephyr-project/wiki/Versioning) page.

## Need help?

- For technical support with Realtek Zephyr, including bugs and feature requests -
  submit a subject to [RealMCU's DevZone](https://www.realmcu.com/community/cimd).

Additionally, we welcome any feedback that you can give to improve the
documentation!