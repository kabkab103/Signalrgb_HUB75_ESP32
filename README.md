# Firmware for ESP32 to Drive HUB75 Panel, Syncing SignalRGB Effects

Using [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) to drive the panel
Currently tested with 32x64 panel only.

![](https://i.giphy.com/FvDnpJ3RNhiTFHGpd3.webp)

1. Change WiFi credentials in .ino file
2. Wire to HUB75 panel as per the [wiring guide](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/blob/master/src/platforms/esp32/esp32-default-pins.hpp), upload the sketch to ESP32, reset, and power on.
3. Place `Hub75-32x64.json` into the `Documents\WhirlwindFX\Components` folder under your user directory.
4. Open SignalRGB, navigate to the Network tab, select WLED, enter the IP address, click 'Discover'.
5. Link your device.
6. In the Devices tab, select the HUB75-PANEL device and add the HUB75 32x64 Panel component.
7. Profit!
