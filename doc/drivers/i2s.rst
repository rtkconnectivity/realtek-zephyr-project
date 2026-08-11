I2S
===

The I2S driver exposes the Bee I2S controllers through the standard Zephyr I2S
API. Each direction streams over the general-purpose DMA controller, so an I2S
node always references a DMA channel for the direction(s) it uses.

.. note::

   The Bee I2S and its on-chip codec use the standard Zephyr I2S and audio codec
   APIs, but the Bee usage flow differs from typical Zephyr drivers: follow the
   configuration and trigger sequences described in this document rather than
   assuming stock Zephyr behavior.

Functional Overview
-------------------

Feature List
~~~~~~~~~~~~
- Transmit and receive audio streaming, each driven over a dedicated DMA
  channel. TX and RX are enabled independently at build time
  (``I2S_BEE_TX`` / ``I2S_BEE_RX``); at least one of the two must be enabled.
- Controller (master) and peripheral (slave) role support.
- I2S, PCM short, PCM long, and left-justified data formats.
- 8-, 16-, and 24-bit sample widths on all SoCs, plus 20- and 32-bit widths on
  rtl87x2g.
- Mono and stereo channel support.
- MSB-first and LSB-first bit ordering.
- Standard audio frame-clock rates from 8 kHz to 192 kHz (the frame clock must
  be a multiple of 25 Hz).
- On-chip audio codec support, with analog (AMIC) and digital (DMIC) microphone
  input, through a child ``codec`` node.
- Double-buffered (ping-pong) DMA streaming. The block / message queue depth is
  configurable (``I2S_BEE_TX_BLOCK_COUNT`` / ``I2S_BEE_RX_BLOCK_COUNT``, default
  4 each); the two ping-pong buffers are reserved statically, sized by
  ``I2S_BEE_TX_BLOCK_SIZE_MAX`` / ``I2S_BEE_RX_BLOCK_SIZE_MAX`` (default 512
  bytes each), so the driver needs no system heap.

Basic Information
~~~~~~~~~~~~~~~~~
- Device nodes:

  - rtl87x2g: ``i2s0``, ``i2s1``.
  - rtl8752h: ``i2s0``.
  - rtl87x2j: ``i2s0``.
- Bindings file: ``dts/bindings/i2s/realtek,bee-i2s.yaml``
  (compatible ``realtek,bee-i2s``).
- Kconfig options: ``I2S_BEE``, ``I2S_BEE_TX``, ``I2S_BEE_RX``,
  ``I2S_BEE_TX_BLOCK_COUNT``, ``I2S_BEE_RX_BLOCK_COUNT``,
  ``I2S_BEE_TX_BLOCK_SIZE_MAX``, ``I2S_BEE_RX_BLOCK_SIZE_MAX``
  (``drivers/i2s/Kconfig.bee``).
- Source file: ``drivers/i2s/i2s_bee.c``.
- Example board files (under ``samples/drivers/i2s_bee/boards/``):
  ``rtl87x2g_evb_a_rtl8762gku.overlay`` (rtl87x2g),
  ``rtl8752h_evb_rtl8752hjl.overlay`` (rtl8752h), and
  ``rtl87x2j_evb_rtl8762jth.overlay`` (rtl87x2j).
- Reference:

  - `Zephyr I2S introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/i2s.html>`_
  - `Zephyr I2S API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__i2s__interface.html>`_

.. note::

   Selecting ``I2S_BEE`` also selects ``DMA`` and ``PINCTRL``: the driver
   streams through DMA and routes its pads through pinctrl, so both are always
   pulled in. TX and RX are off by default — enable ``I2S_BEE_TX`` and/or
   ``I2S_BEE_RX`` for the direction(s) you need.

Operation Flow
--------------

Enabling an I2S device
~~~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: set ``status = "okay"`` on the ``i2s`` node, give it a DMA
  channel per direction through ``dmas`` / ``dma-names`` (names ``"tx"`` and
  ``"rx"``), and route its pads through a ``pinctrl-0`` group. The ``reg``,
  ``interrupts``, ``clocks``, and ``#address-cells`` / ``#size-cells`` are
  already provided by the SoC devicetree. Each ``dmas`` entry is
  ``<&dma0 <channel> <slot> <config>>``; see the :doc:`DMA <dma>` chapter for the
  slot and config encoding, and :doc:`Pinctrl <pinctrl>` for the pad syntax.

  .. code-block:: devicetree

     #include <dt-bindings/dma/rtl87x2g-dma.h>

     &dma0 {
         status = "okay";
     };

     &i2s0 {
         dmas = <&dma0 0 BEE_DMA_HANDSHAKE_I2S0_TDM0_TX
                 (BEE_DMA_M2P | BEE_DMA_SRC_INC | BEE_DMA_DST_FIXED | BEE_DMA_SRC_WIDTH_32BIT |
                  BEE_DMA_DST_WIDTH_32BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_4) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_4) | BEE_DMA_PRIORITY(0))>,
                <&dma0 1 BEE_DMA_HANDSHAKE_I2S0_TDM0_RX
                 (BEE_DMA_P2M | BEE_DMA_SRC_FIXED | BEE_DMA_DST_INC | BEE_DMA_SRC_WIDTH_32BIT |
                  BEE_DMA_DST_WIDTH_8BIT | BEE_DMA_SRC_MSIZE(BEE_DMA_MSIZE_4) |
                  BEE_DMA_DST_MSIZE(BEE_DMA_MSIZE_16) | BEE_DMA_PRIORITY(0))>;
         dma-names = "tx", "rx";
         pinctrl-0 = <&i2s0_default>;
         pinctrl-names = "default";
         status = "okay";
     };

  The pad assignments and the pinctrl group name differ per SoC; route the pads
  as in the sample overlays. The per-pin comments mark each pad's role:
  ``i2s_tx`` / ``i2s_rx`` are the functional standalone-I2S signals, needed when
  the controller drives an *external* I2S device; ``i2s_codec_tx`` /
  ``i2s_codec_rx`` are optional probe points that expose the on-chip
  I2S-to-codec signals for observation. When you use only the internal codec,
  none of these pads are required for audio to flow.
  On rtl87x2g:

  .. code-block:: devicetree

     &pinctrl {
         i2s0_default: i2s0_default {
             group1 {
                 psels = <BEE_PSEL(BCLK_SPORT0, P4_0)>,
                         <BEE_PSEL(LRC_RX_SPORT0, P4_1)>,   /* i2s_codec_rx */
                         <BEE_PSEL(LRC_SPORT0, P4_2)>,      /* i2s_tx, i2s_codec_tx */
                         <BEE_PSEL(SDI_CODEC_SLAVE, P4_3)>, /* i2s_codec_rx/tx */
                         <BEE_PSEL(SDO_CODEC_SLAVE, P4_4)>, /* i2s_codec_rx/tx */
                         <BEE_PSEL(ADCDAT_SPORT0, P4_5)>,   /* i2s_tx */
                         <BEE_PSEL(DACDAT_SPORT0, P4_6)>;   /* i2s_tx */
                 output-enable;
                 output-low;
                 bias-disable;
             };
         };
     };

  On rtl8752h:

  .. code-block:: devicetree

     &pinctrl {
         i2s_default: i2s_default {
             group1 {
                 psels = <BEE_PSEL(LRC_SPORT0, P4_0)>,
                         <BEE_PSEL(BCLK_SPORT0, P4_1)>,   /* i2s_tx, i2s_codec_rx */
                         <BEE_PSEL(DACDAT_SPORT0, P4_2)>; /* i2s_tx, i2s_codec_rx */
                 output-enable;
                 output-low;
                 bias-disable;
             };
         };
     };

  On rtl87x2j:

  .. code-block:: devicetree

     &pinctrl {
         i2s0_default: i2s0_default {
             group1 {
                 psels = <BEE_PSEL(BCLK_SPORT0, P2_0)>,
                         <BEE_PSEL(LRC_SPORT0, P2_1)>,      /* i2s_tx, i2s_codec_rx */
                         <BEE_PSEL(SDI_CODEC_SLAVE, P2_2)>, /* i2s_codec_rx */
                         <BEE_PSEL(SDO_CODEC_SLAVE, P2_3)>, /* i2s_codec_rx */
                         <BEE_PSEL(ADCDAT_SPORT0, P2_4)>,   /* i2s_tx */
                         <BEE_PSEL(DACDAT_SPORT0, P2_5)>;   /* i2s_tx */
                 output-enable;
                 output-low;
                 bias-disable;
             };
         };
     };

  Enable the driver and the direction(s) you need in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_I2S=y
     CONFIG_I2S_BEE=y
     CONFIG_I2S_BEE_TX=y
     CONFIG_I2S_BEE_RX=y

- *Example*: the ``&i2s0`` node and its ``i2s0_default`` pinctrl group in the
  overlays under ``samples/drivers/i2s_bee/boards/``.

Configuring a stream
~~~~~~~~~~~~~~~~~~~~~
- *How to configure*: configure each direction separately with a
  ``struct i2s_config``. ``I2S_DIR_BOTH`` is rejected (``-ENOSYS``); configure
  ``I2S_DIR_TX`` and ``I2S_DIR_RX`` in two calls. The driver validates the
  configuration fields:

  - ``word_size``: 8, 16, or 24 bits on all SoCs; 20 and 32 bits are also
    accepted on rtl87x2g.
  - ``channels``: 1 (mono) or 2 (stereo). On the rtl87x2g internal codec a
    value of 0 selects the codec's DAC (playback) path.
  - ``format``: I2S, PCM short (PCM mode A), PCM long (PCM mode B), or
    left-justified, optionally combined with the LSB-first data-order bit.
  - ``frame_clk_freq``: a standard rate from 8 kHz to 192 kHz. The value must be
    a multiple of 25; other rates return ``-EINVAL``.
  - role: controller (master) by default; the bit-clock target option selects
    peripheral (slave) role. The gated-bit-clock and loopback options are not
    supported and return ``-EINVAL``.

- *How to use*: pass ``I2S_DIR_TX`` or ``I2S_DIR_RX`` with the filled
  ``struct i2s_config`` (including the ``mem_slab``, ``block_size``, and
  ``timeout``) to move the stream into the ready state. Configuring with
  ``frame_clk_freq`` set to 0 releases the device: it gates the I2S clock and
  applies the pinctrl ``sleep`` state.
- *Example*: the ``i2s_configure()`` calls in
  ``samples/drivers/i2s_bee/src/main.c``.

Transmitting and receiving
~~~~~~~~~~~~~~~~~~~~~~~~~~~
- *How to use*: after configuring, queue the direction with the standard I2S
  trigger commands. For TX, submit filled blocks and issue a START trigger; the
  driver copies each block into a statically reserved ping-pong buffer and
  streams it out over DMA.
  For RX, issue a START trigger and read completed blocks from the driver's
  memory slab. STOP and DRAIN finish the current stream gracefully, DROP
  discards queued buffers, and PREPARE returns a stream from the error state to
  ready.
- *Example*: the RX capture loop and TX playback loop in
  ``samples/drivers/i2s_bee/src/main.c``.

.. note::

   The RX memory slab must hold at least two free buffers when a receive stream
   starts, otherwise the START trigger fails (``-EINVAL``): the driver needs one
   buffer in flight and one to allocate for the next DMA frame.

.. note::

   The ``block_size`` in ``struct i2s_config`` must not exceed
   ``I2S_BEE_TX_BLOCK_SIZE_MAX`` (TX) or ``I2S_BEE_RX_BLOCK_SIZE_MAX`` (RX),
   which size the statically reserved ping-pong buffers; a larger block makes
   the START trigger fail (``-EINVAL``). Raise the matching Kconfig value if
   your application needs bigger blocks.

Audio Codec
-----------
The Bee SoCs integrate an on-chip audio codec that shares the I2S data lines of
the controller it attaches to. It is a separate driver
(``drivers/audio/codec_bee.c``, compatible ``realtek,bee-codec``) exposed through
the standard Zephyr audio codec API (``include/zephyr/audio/codec.h``), and it is
described in devicetree as a child ``codec`` node of the ``i2s`` node that carries
its samples. The codec manages the analog and digital-microphone front end
(microphone bias and boost, ADC/DAC gain, channel sequence, sample rate, and the
data format), while the I2S driver streams the samples over DMA. On rtl87x2g the
internal codec supports both playback (TX) and capture (RX); on rtl8752h and
rtl87x2j it supports capture (RX) only. An I2S controller on any SoC can instead
drive an external codec over its pads.

Codec features
~~~~~~~~~~~~~~
- Analog-microphone (AMIC) and digital-microphone (DMIC) input, selected by the
  ``mic-type`` property.
- Configurable microphone bias voltage, boost mode, and boost gain.
- ADC capture gain, and DAC output gain on rtl87x2g.
- Mute and volume control through the audio codec ``set_property`` interface
  (applied to ``AUDIO_CHANNEL_ALL``).
- I2S, PCM short, PCM long, and left-justified data formats, matching the I2S
  controller it feeds.
- 8- and 16-bit sample widths on all SoCs, plus 24-bit on rtl8752h and rtl87x2j.
- Sample rates from 8 kHz to 192 kHz on rtl87x2g, 8 kHz and 16 kHz on
  rtl8752h, and from 8 kHz to 48 kHz on rtl87x2j.

Configuring the codec
~~~~~~~~~~~~~~~~~~~~~~
- *Basics*: driver ``drivers/audio/codec_bee.c``, compatible
  ``realtek,bee-codec``, binding ``dts/bindings/audio/realtek,bee-codec.yaml``.
  The Kconfig option ``AUDIO_CODEC_BEE`` (``drivers/audio/Kconfig.bee``, default
  ``y`` when a codec node is enabled) selects ``I2S``. The codec is available on
  rtl87x2g, rtl8752h, and rtl87x2j.
- *How to configure*: add a ``codec`` child node (``reg = <0>``) to the ``i2s``
  node, give it its own ``pinctrl-0`` group, and set the codec properties:

  - ``mic-type``: ``amic`` (analog) or ``dmic`` (digital); for a digital mic,
    set ``dmic`` and route the DMIC clock/data pins in the codec pinctrl group
    instead of the analog PDM pins.
  - ``channel-sequence``: ``l_r``, ``r_l``, ``l_l``, or ``r_r``.
  - ``mic-bias``: MICBIAS voltage, ``1_507`` through ``2_314``.
  - ``mic-bst-mode``: ``single`` or ``differential``.
  - ``mic-bst-gain``: ``0dB``, ``20dB``, ``30dB``, or ``40dB``.
  - ``boost-gain``: ``0dB``, ``12dB``, ``24dB``, or ``36dB``.
  - ``ad-gain``: ADC digital volume (``0x2f`` is 0 dB, 0.375 dB per step).
  - ``da-gain``: DAC output gain, ``0x0`` to ``0xAf`` (rtl87x2g).
  - ``dmic-data-latch``: ``falling_latch`` or ``rising_latch`` (DMIC only).

  .. code-block:: devicetree

     &i2s0 {
         /* dmas, dma-names, pinctrl, and status as shown above */

         codec: codec@0 {
             compatible = "realtek,bee-codec";
             reg = <0>;
             mic-type = "amic";
             channel-sequence = "l_r";
             mic-bias = "1_8";
             mic-bst-mode = "differential";
             mic-bst-gain = "30dB";
             boost-gain = "0dB";
             ad-gain = <0x2f>;
             dmic-data-latch = "rising_latch";
             pinctrl-0 = <&codec_default>;
             pinctrl-names = "default";
             status = "okay";
         };
     };

  The codec ``pinctrl-0`` group routes the microphone pads and differs per SoC
  and per microphone type. For an analog microphone (``mic-type = "amic"``) route
  the codec power pads; for a digital microphone (``mic-type = "dmic"``) route the
  DMIC clock/data pads instead. On rtl87x2g the codec also brings out its TX
  signal on the PDM pads for observation.

  On rtl87x2g, for an analog microphone (AMIC):

  .. code-block:: devicetree

     &pinctrl {
         codec_default: codec_default {
             group1 {
                 psels = <BEE_PSEL(PWR_OFF, P2_6)>,   /* amic */
                         <BEE_PSEL(PWR_OFF, P2_7)>;   /* amic */
                 output-disable;
                 bias-disable;
             };
         };
     };

  On rtl87x2g, for a digital microphone (DMIC):

  .. code-block:: devicetree

     &pinctrl {
         codec_default: codec_default {
             group1 {
                 psels = <BEE_PSEL(DMIC1_CLK, P3_2)>, /* dmic */
                         <BEE_PSEL(DMIC1_DAT, P3_3)>; /* dmic */
                 output-disable;
                 bias-disable;
             };
         };
     };

  On rtl8752h, for an analog microphone (AMIC):

  .. code-block:: devicetree

     &pinctrl {
         codec_default: codec_default {
             group1 {
                 psels = <BEE_PSEL(PWR_OFF, H_0)>,  /* amic */
                         <BEE_PSEL(PWR_OFF, P2_6)>, /* amic */
                         <BEE_PSEL(PWR_OFF, P2_7)>; /* amic */
                 output-disable;
                 bias-disable;
             };
         };
     };

  On rtl8752h, for a digital microphone (DMIC):

  .. code-block:: devicetree

     &pinctrl {
         codec_default: codec_default {
             group1 {
                 psels = <BEE_PSEL(DMIC1_CLK, P2_6)>, /* dmic */
                         <BEE_PSEL(DMIC1_DAT, P2_7)>; /* dmic */
                 output-disable;
                 bias-disable;
             };
         };
     };

  On rtl87x2j, for an analog microphone (AMIC):

  .. code-block:: devicetree

     &pinctrl {
         codec_default: codec_default {
             group1 {
                 psels = <BEE_PSEL(PWR_OFF, P2_6)>,  /* amic */
                         <BEE_PSEL(PWR_OFF, P2_7)>;  /* amic */
                 output-disable;
                 bias-disable;
             };

             group2 {
                 psels = <BEE_PSEL(SW_MODE, MICBIAS)>; /* amic */
                 output-enable;
                 output-high;
                 bias-disable;
             };
         };
     };

  On rtl87x2j, for a digital microphone (DMIC):

  .. code-block:: devicetree

     &pinctrl {
         codec_default: codec_default {
             group1 {
                 psels = <BEE_PSEL(DMIC1_CLK, P2_6)>, /* dmic */
                         <BEE_PSEL(DMIC1_DAT, P2_7)>; /* dmic */
                 output-disable;
                 bias-disable;
             };
         };
     };

  Enable the codec together with the audio subsystem in ``prj.conf``:

  .. code-block:: kconfig

     CONFIG_AUDIO=y
     CONFIG_AUDIO_CODEC=y
     CONFIG_AUDIO_CODEC_BEE=y

- *Wiring note*: with the internal codec the I2S-to-codec connection is on-chip,
  so the ``i2s`` node's pads need no pinctrl routing for audio to flow; only the
  codec's own ``pinctrl-0`` (microphone / PDM or DMIC pads) is required. Add an
  ``i2s`` ``pinctrl-0`` group only when you want to observe the I2S waveforms
  externally; the sample overlays route those pads and flag them with
  ``waveform`` comments.
- *Example*: the ``codec`` node and its ``codec_default`` group in the overlays
  under ``samples/drivers/i2s_bee/boards/``.

Driving the codec
~~~~~~~~~~~~~~~~~
- *How to use*: configure the codec with ``audio_codec_configure()``, passing an
  ``audio_codec_cfg`` whose ``dai_type`` is ``AUDIO_DAI_TYPE_I2S``, whose
  ``mclk_freq`` sets the DMIC clock, and whose ``dai_cfg.i2s`` carries the
  format, word size, sample rate, and channel count. Optionally set volume or
  mute with ``audio_codec_set_property()`` (property
  ``AUDIO_PROPERTY_OUTPUT_VOLUME`` or ``AUDIO_PROPERTY_OUTPUT_MUTE``, channel
  ``AUDIO_CHANNEL_ALL``) and commit them with ``audio_codec_apply_properties()``.
  Call ``audio_codec_start_output()`` to power the codec, apply its ``default``
  pinctrl state, and initialize it; then configure and trigger the matching I2S
  direction to stream samples. Call ``audio_codec_stop_output()`` to
  de-initialize the codec and apply its ``sleep`` pinctrl state.
- *Power management*: on a SoC with a PCK600 (such as rtl87x2j) the PCK600
  manages power in hardware; once ``audio_codec_start_output()`` has powered the
  codec the PCK600 holds it out of sleep, so the system does not enter DLPS
  while the codec is running. Call ``audio_codec_stop_output()`` to shut the
  codec down and let the system sleep.
- *Example*: the ``audio_codec_*`` calls in
  ``samples/drivers/i2s_bee/src/main.c``.

.. note::

   On rtl87x2g / rtl8752h, which have no PCK600, nothing blocks DLPS: if the
   system sleeps while the codec is active the codec loses power and
   capture / playback stops. The application must manage sleep itself: before
   entering DLPS make sure the stream has finished and stop the codec with
   ``audio_codec_stop_output()``, then re-initialize it with
   ``audio_codec_start_output()`` after wakeup.

Power Management (DLPS)
-----------------------
- *Behavior*: on a SoC with a PCK600 (such as rtl87x2j) the PCK600 manages power
  in hardware; once ``i2s_trigger()`` with ``I2S_TRIGGER_START`` has started a
  stream the PCK600 holds the I2S out of sleep, so the system does not enter
  DLPS while the stream is running. Call ``i2s_trigger()`` with
  ``I2S_TRIGGER_STOP`` or ``I2S_TRIGGER_DRAIN`` to end the stream and let the
  system sleep.

.. note::

   On rtl87x2g / rtl8752h, which have no PCK600, nothing blocks DLPS: if the
   system sleeps while a stream is active the I2S loses power and the stream
   stops. The application must manage sleep itself: before entering DLPS make
   sure the stream has finished and stop it with ``i2s_trigger()``
   (``I2S_TRIGGER_STOP`` or ``I2S_TRIGGER_DRAIN``), then restart it with
   ``I2S_TRIGGER_START`` after wakeup.

Samples and Logs
----------------
Sample: ``samples/drivers/i2s_bee``.

How to run
~~~~~~~~~~
- *Wiring*: the default path uses the on-chip audio codec for RX capture and
  I2S TX. The microphone is external: wire an analog microphone (AMIC) or a
  digital microphone (DMIC) to the codec's corresponding pins. To capture the
  recording, connect the ``dump-mic`` UART TX (``uart0``, at 2 Mbaud) to a
  serial terminal; to observe the buses, probe the I2S and codec TX / RX
  waveforms with a logic analyzer. To feed an external I2S device instead,
  remove the child ``codec`` node and wire the ``BCLK_SPORT0`` / ``LRC_SPORT0``
  / data pads from the overlay.
- *Extra config*: the sample's ``prj.conf`` enables TX and RX and the audio
  codec. The driver reserves its ping-pong buffers statically, so no system
  heap is required; the sample's 256-byte blocks fit the default
  ``I2S_BEE_TX_BLOCK_SIZE_MAX`` / ``I2S_BEE_RX_BLOCK_SIZE_MAX`` of 512 bytes.
- *Build & flash*:

  .. code-block:: console

     # rtl87x2g
     west build -p -b rtl87x2g_evb_a/rtl8762gku samples/drivers/i2s_bee

     # rtl8752h
     west build -p -b rtl8752h_evb/rtl8752hjl   samples/drivers/i2s_bee

     # rtl87x2j
     west build -p -b rtl87x2j_evb/rtl8762jth   samples/drivers/i2s_bee

     west flash --port <your-flash-serial-port>

When the sample starts it captures about five seconds of audio from the
microphone, which shows up as five seconds of I2S RX activity on the logic
analyzer. The captured samples are not printed on the console; they are dumped
as raw bytes over ``uart0`` (the ``dump-mic`` alias), which is a separate port
from the Zephyr console. Capture that stream in a serial terminal and save it
as a raw file, then import the file into Audacity as signed 16-bit PCM,
little-endian, mono to play the recording back. After the capture finishes the
sample plays the audio out over I2S, which appears as a further five seconds of
I2S TX activity on the logic analyzer.

See Also
--------
- :doc:`Drivers General Introduction <driver_general_introduction>`
- :doc:`DMA <dma>`
- :doc:`Pinctrl <pinctrl>`
- :doc:`Clock Control <clock_control>`
- :ref:`Logging note in the Overview <driver_logging_note>`
- `Zephyr I2S introduction <https://docs.zephyrproject.org/latest/hardware/peripherals/i2s.html>`_
- `Zephyr I2S API reference <https://docs.zephyrproject.org/latest/doxygen/html/group__i2s__interface.html>`_
