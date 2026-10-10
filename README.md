# ESP32-P4 NOMMU Linux OS with Wi-Fi Terminal (Telnet)

Run a real **Linux Kernel (v6.9.0-rc1+ NOMMU on RISC-V 32-bit)** with an interactive **BusyBox root shell** on the **Waveshare ESP32-P4-NANO / DevKit** with external 32MB PSRAM and wireless terminal access over Wi-Fi 6 via the on-board **ESP32-C6** co-processor!

---

## 🛠 Hardware Architecture

| Component | Specification |
| :--- | :--- |
| **Primary SoC** | ESP32-P4 (Dual-Core RISC-V HP cores @ 400 MHz, Silicon Revision v1.0) |
| **External Memory** | 32 MB High-Speed PSRAM (16-Line HEX Mode @ 200 MHz) |
| **Storage (Flash)** | 16 MB SPI Flash |
| **Wireless Co-Processor** | ESP32-C6 connected via SDIO slave (`esp-hosted`) for 802.11ax Wi-Fi 6 |
| **Operating System** | Linux 6.9.0-rc1+ (NOMMU M-mode / Sv32) with BusyBox userland |
| **Remote Terminal** | Wireless Telnet / TCP Terminal Server on Port 23 |

---

## ⚡ Quick Start: How to Build & Flash

### 1. Prerequisites
- **Ubuntu / Debian Linux x86_64**
- **ESP-IDF v5.5** installed at `~/esp/esp-idf`
- Device Tree Compiler:
  ```bash
  sudo apt-get install -y device-tree-compiler
  ```

### 2. Set Up Environment & Target
```bash
# Source ESP-IDF
source ~/esp/esp-idf/export.sh

# Target ESP32-P4
idf.py set-target esp32p4
```

### 3. Compile the Device Tree Blob (DTB)
```bash
dtc -I dts -O dtb -o dtb.bin main/uc.dts
```

### 4. Build the Firmware
```bash
idf.py build
```

### 5. Flash Everything to the ESP32-P4
```bash
python -m esptool --chip esp32p4 -p /dev/ttyACM0 -b 460800 --before default_reset --after hard_reset write_flash \
    0x2000 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0x10000 build/uc-rv32ima.bin \
    0x110000 main/Image \
    0xF10000 dtb.bin
```

---

## 🌐 Connecting to the Linux Terminal Over Wi-Fi

1. Once flashed, the ESP32-P4 powers up, communicates with the ESP32-C6 over SDIO, and connects to your Wi-Fi network.
2. Check your router (or serial log) for the assigned IP (e.g. `192.168.0.47`).
3. Connect wirelessly from any terminal:
   ```bash
   telnet 192.168.0.47 23
   # or
   nc 192.168.0.47 23
   ```
4. You will see:
   ```text
   *** Welcome to ESP32-P4 NOMMU Linux over Wi-Fi! ***
   ~ # uname -a
   Linux (none) 6.9.0-rc1+ #19 Thu Apr  4 09:56:33 CST 2024 riscv32 GNU/Linux
   ~ # 
   ```

---

## ⚠️ Known Pitfalls & Solutions Encountered During Development

### Pitfall 1: Silicon Stepping Revision Mismatch (`[v3.1 - v3.99]` vs `v1.0`)
- **Error:** `bootloader.bin requires chip revision in range [v3.1 - v3.99] (this chip is revision v1.0). Use --force to flash anyway.`
- **Cause:** ESP-IDF v5.5 defaults to ESP32-P4 Silicon Rev v3.1+. The early Waveshare chips are Silicon Rev v1.0.
- **Solution:** In `sdkconfig`:
  ```ini
  CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
  CONFIG_ESP32P4_REV_MIN_100=y
  CONFIG_ESP32P4_REV_MIN_FULL=100
  ```

### Pitfall 2: `No PSRAM available!` Panic
- **Error:** Linux emulator halts with `ERROR: No PSRAM available!`.
- **Cause:** External PSRAM disabled by default in generic configs.
- **Solution:** Enable in `sdkconfig`:
  ```ini
  CONFIG_SPIRAM=y
  CONFIG_SPIRAM_MODE_HEX=y
  CONFIG_SPIRAM_SPEED_200M=y
  ```

### Pitfall 3: Flash Size Limit Conflict with 16MB Partition Table
- **Error:** `Partitions table occupies 15.1MB of flash which does not fit in configured flash size 2MB.`
- **Solution:** Set flash size explicitly in `sdkconfig`:
  ```ini
  CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
  CONFIG_ESPTOOLPY_FLASHSIZE="16MB"
  ```

### Pitfall 4: `esp-hosted` Slave Identification Mismatch
- **Error:** `Identified slave [esp32c6] != Expected [esp32] ... Aborting..`
- **Cause:** `esp-hosted` defaults to legacy ESP32 as co-processor instead of the on-board ESP32-C6.
- **Solution:** In `sdkconfig`:
  ```ini
  CONFIG_SLAVE_IDF_TARGET_ESP32C6=y
  CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y
  CONFIG_ESP_HOSTED_IDF_SLAVE_TARGET="esp32c6"
  ```

### Pitfall 5: FreeRTOS Stack Overflows on Connection
- **Error:** `Guru Meditation Error: Core 0 panic'ed (Stack protection fault) in task "sys_evt" or "main"`.
- **Cause:** The default `CONFIG_ESP_MAIN_TASK_STACK_SIZE` (3.5KB) and `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` (2.3KB) are too small to process Wi-Fi 6 handshake and SDIO RPC buffers.
- **Solution:** Increased in `sdkconfig`:
  ```ini
  CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384
  CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=6144
  CONFIG_SYSTEM_EVENT_TASK_STACK_SIZE=6144
  ```

### Pitfall 6: SDIO DMA Out-Of-Memory (OOM)
- **Error:** `E (1460) HS_MP: mempool create failed: no mem`.
- **Cause:** The ESP32-C6 packet mempool tries to allocate DMA buffers entirely from internal SRAM, which runs out.
- **Solution:** Enable PSRAM buffering:
  ```ini
  CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y
  ```

---

## 📜 License
Apache-2.0 / GPL-2.0 (Linux kernel components).
