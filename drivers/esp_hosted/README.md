# ESP-Hosted host shim for ESP32-P4

This directory does not contain the full Espressif `esp-hosted` stack.
It provides the minimal host-side attachment points needed to add it cleanly
to this bare-metal SDK:

1. A transport API for the ESP32-P4 SDIO host driver
2. A small RX queue for complete hosted packets
3. A raw Ethernet callback path for `lwIP`
4. A raw HCI event/ACL callback path for a BLE host stack

## Intended data flow

1. The SDIO host driver receives one or more complete packets from the ESP32-C6
2. The SDIO driver decodes the hosted header and fills `esp_hosted_frame_info_t`
3. The SDIO driver calls `esp_hosted_rx_submit()`
4. The application either:
   - calls `esp_hosted_rx_pop()` and handles packets manually, or
   - registers callbacks and calls `esp_hosted_dispatch_rx()`

## Main APIs

- `esp_hosted_init()`
- `esp_hosted_set_transport()`
- `esp_hosted_poll()`
- `esp_hosted_rx_submit()`
- `esp_hosted_lwip_tx()`
- `esp_hosted_ctrl_tx()`
- `esp_hosted_ble_send_hci_cmd()`
- `esp_hosted_ble_send_acl()`
- `esp_hosted_sdio_get_default_config()`
- `esp_hosted_sdio_attach()`
- `esp_hosted_sdio_detach()`

## SDIO transport notes

- Default ESP32-P4 Function EV board mapping matches Espressif's hosted wiring:
  - `CLK=18 CMD=19 D0=14 D1=15 D2=16 D3=17 RESET=54`
- The current driver is polling-based to fit the bare-metal SDK.
- It supports Slot 1 GPIO-matrix routing and Slot 0 native IOMUX pins.

## What still needs to be added

- Wi-Fi control/RPC codec compatible with Espressif's hosted firmware
- Actual `lwIP` stack sources and netif glue
- Actual BLE host stack sources and HCI glue
