# Wake-on-LAN for ESP32 (Zephyr RTOS)

<p align="center">
  <img src="img/working2.gif" alt="System Working" width="400">
</p>

<p align="center">
  <i>A Zephyr RTOS port and enhancement of the original <a href="https://github.com/sergio-isidoro/Wake-on-LAN_ESP32">Wake-on-LAN_ESP32</a></i>
</p>

---

## 📝 Description
This project provides a complete, robust, and asynchronous **Wake-on-LAN (WoL)** solution for the **ESP32 DevKitC**. It features a captive portal for easy configuration, persistent storage (NVS), and a UI for SSD1306 OLED displays.

---

## ✨ Key Features

* **Captive Portal Configuration:** No need to hardcode credentials. If no config is found, the device starts an Access Point (`WOL_ESP`) with a DNS redirector and HTTP server.
* **Persistent Storage (NVS):** Wi-Fi credentials, Target MAC, and Target IP are saved securely in the ESP32 internal Flash.
* **Factory Reset:** A hardware-based reset to wipe all saved settings and return to Portal Mode.
* **System Reliability:**
    * **Hardware Watchdog (WDT):** 5-second timeout to automatically recover from hangs.
    * **Asynchronous Workqueues:** WoL packet dispatch and ICMP Ping checks are offloaded from ISRs for maximum stability.
    * **Auto-reconnect:** On Wi-Fi disconnection, the device automatically retries the connection after 3 seconds.
* **OLED UI (SSD1306):** Supports both **0.42" (72x40)** and **1.30" (128x64)** screens.
    * **Animated Heartbeat:** A scrolling `>>>>>>>>` indicator shows the system is active.
    * **Live Status:** Displays the ESP32's own IP and the target PC's IP with online/offline indicator.

---

## 🏗️ Architecture (ESP32 DevKitC — Dual-Core)

The ESP32 DevKitC build uses a **dual-core AMP (Asymmetric Multi-Processing)** architecture managed by **sysbuild**:

| Core | Image | Role |
| :--- | :--- | :--- |
| **PRO CPU** (`procpu`) | `Wake_On_Lan` | Wi-Fi, NVS storage, captive portal, button handling, WoL dispatch, ICMP ping |
| **APP CPU** (`appcpu`) | `wol_appcpu` | SSD1306 OLED rendering via I2C |

Communication between cores is done entirely through a **shared memory region** at a fixed address (`0x3FFD8000`), mapped inside the PRO CPU DRAM and accessible by both cores. The `shared_display_state_t` struct carries display state, IP addresses, and synchronisation flags.

```
ESP32 DRAM layout (relevant regions):
  0x3FFC0000 – 0x3FFFFFFF  procpu DRAM
  0x3FFD8000               shared_display_state_t  ← shared memory
  0x3FFE8000 – 0x3FFED4A4  appcpu .data/.bss (loaded by AMP loader)
  0x3FFB0000 – 0x3FFBFFFF  AMP heap for appcpu
```

> **Why `CONFIG_IPM=y`?** Even though IPM (Inter-Processor Mailbox) is not used directly for messaging, this Kconfig option is required to trigger the Zephyr AMP loader that boots the APP CPU image. Without it the second core never starts.

**Startup sequence:**
1. PRO CPU boots, zeroes shared memory, initialises storage/notify/button.
2. PRO CPU checks NVS — starts Wi-Fi station or captive portal accordingly.
3. Once ready, PRO CPU sets `display_start_flag = 1` in shared memory.
4. APP CPU polls `display_start_flag` and starts the OLED render loop.

---

## 🖥️ User Interface & States

### 1. Portal Mode (Initial Setup)
Connect to Wi-Fi `WOL_ESP` and access `192.168.4.1`.

<p align="center">
  <img src="img/portal1.png" width="400"> <img src="img/portal2.png" width="400"> <img src="img/portal3.png" width="400">
</p>

| Line | Content | Description |
| :---: | :---: | :--- |
| **Top** | `WOL_ESP` | Static header for Configuration Mode |
| **Middle** | `192.168` | Portal IP (Part 1) |
| **Bottom** | `.4.1` | Portal IP (Part 2) |

### 2. Connecting Mode
<p align="center">
  <img src="img/connecting_wifi.png" width="250">
</p>

| Line | Content | Description |
| :---: | :---: | :--- |
| **Top** | `>>>>>>>` | Activity heartbeat |
| **Middle** | `Waiting` | Status message |
| **Bottom** | `WIFI` | **Ideal time to trigger Factory Reset** |

### 3. Operation Mode (Station)
<p align="center">
  <img src="img/wol_magic_packet.png" width="250">
</p>

| Line | Content | Description |
| :---: | :---: | :--- |
| **Top** | `>>>>>>>` | Real-time refresh indicator |
| **Middle** | `192.168.1.111` | ESP32's own IP address |
| **Bottom** | `* 192.168.1.222` | `*` = Online / `x` = Offline + Target PC IP |

---

## 🔄 Reset Procedures (Factory Reset)

<p align="center">
  <img src="img/factory_reset.png" width="250">
</p>

If you need to change the network or target details, you can force the device back into **Portal Mode**:

1.  **When to press:** Power on or reboot the device.
2.  **The Trigger:** As soon as the display shows **`Waiting WIFI`**, press and hold the **BOOT button**.
3.  **Duration:** Keep pressed for **1 second**.
4.  **Confirmation:** Settings are erased, LED blinks twice, and the device reboots into Portal Mode (`WOL_ESP`).

---

## 🛠️ Hardware Requirements

### ESP32 DevKitC
| Component | Detail |
| :--- | :--- |
| **Display** | 1.30" OLED SSD1306 via I2C (**SDA: GPIO 21, SCL: GPIO 22**) |
| **LED** | Blue LED on **GPIO 2** |
| **Button** | BOOT button on **GPIO 0** (Trigger WoL / Factory Reset) |

> **Target PC:** Must support WoL and allow ICMP Echo Requests (Ping).

---

## 📂 Project Structure

```
.
├── CMakeLists.txt              # Root build file (procpu image + overlay selection)
├── prj.conf                    # Shared Kconfig (networking, Wi-Fi, NVS, WDT)
├── sysbuild.cmake              # Sysbuild: enables MCUboot + adds appcpu image
│
├── procpu/                     # PRO CPU image (Wake_On_Lan)
│   ├── CMakeLists.txt
│   ├── prj.conf
│   ├── esp32_devkitc_procpu.overlay   # DTS: LED (GPIO2), Button (GPIO0), WDT
│   ├── src/
│   │   ├── main.c              # Boot logic, WDT feed loop, factory reset
│   │   ├── wifi.c              # Wi-Fi connect/reconnect, DHCP, ICMP ping, WoL dispatch
│   │   ├── portal.c            # Captive portal: AP mode, HTTP/DHCP/DNS servers
│   │   ├── storage.c           # NVS read/write/clear for SSID, password, MAC, IP
│   │   ├── button.c            # GPIO interrupt for BOOT button
│   │   └── notify.c            # Blue LED blink thread + display refresh signal
│   └── inc/
│       ├── wifi.h
│       ├── portal.h
│       ├── portal_html.h       # HTML for captive portal (split into chunks)
│       ├── storage.h
│       ├── button.h
│       ├── display.h           # Shared-memory helpers (DevKitC) / semaphores (C3)
│       └── notify.h
│
├── appcpu/                     # APP CPU image (wol_appcpu)
│   ├── CMakeLists.txt
│   ├── prj.conf                # I2C, SSD1306, CFB, minimal stack
│   ├── esp32_devkitc_appcpu.overlay   # DTS: I2C0 on GPIO21/22, SSD1306@0x3C
│   ├── src/
│   │   └── main.c              # CFB render loop: AP static screen / station animation
│   └── inc/
│       └── custom_font.h       # Embedded bitmap font for CFB
│
├── shared/
│   └── shared_mem.h            # shared_display_state_t struct at 0x3FFD8000
│
├── Tools/
│   ├── WakeOnLanMonitor.exe    # Windows tool to verify WoL magic packets on LAN
│   └── readme.md
│
└── img/                        # Screenshots and GIFs for documentation
```

---

## 🚀 Quick Start

### ESP32 DevKitC (dual-core, sysbuild)
```bash
west build -p always -b esp32_devkitc/esp32/procpu --sysbuild .
west flash --build-dir build
```

> The first flash builds **MCUboot** + **procpu** + **appcpu** in a single sysbuild invocation. Subsequent builds with `west flash` update all images automatically.

---

## 🔧 Build Environment

| Tool | Version |
| :--- | :--- |
| **Zephyr RTOS** | 4.3.0 |
| **Zephyr SDK** | 0.17.3 |
| **West** | latest |

---

## ⚙️ Key Configuration Details

### `prj.conf` (root / procpu)
Notable options beyond the standard networking and Wi-Fi stack:

| Config | Value | Purpose |
| :--- | :--- | :--- |
| `CONFIG_IPM` | `y` | **Required** to boot the APP CPU via the AMP loader |
| `CONFIG_ESP_APPCPU_IRAM_SIZE` | `0x10000` | APP CPU instruction RAM allocation |
| `CONFIG_ESP_APPCPU_DRAM_SIZE` | `0x10000` | APP CPU data RAM allocation |
| `CONFIG_WATCHDOG` | `y` | Hardware WDT, 5 s timeout, fed every 500 ms in main loop |
| `CONFIG_HEAP_MEM_POOL_SIZE` | `65536` | Heap for TCP/IP and Wi-Fi buffers |
| `CONFIG_THREAD_ANALYZER_AUTO` | `y` | Periodic thread stats every 30 s (debug) |

### `appcpu/prj.conf`
Minimal image — only what the display driver needs:

| Config | Value | Purpose |
| :--- | :--- | :--- |
| `CONFIG_I2C` | `y` | I2C bus for SSD1306 |
| `CONFIG_SSD1306` | `y` | SSD1306/SH1106 display driver |
| `CONFIG_CHARACTER_FRAMEBUFFER` | `y` | CFB text rendering engine |
| `CONFIG_CHARACTER_FRAMEBUFFER_USE_DEFAULT_FONTS` | `n` | Use `custom_font.h` instead |

### `sysbuild.cmake`
```cmake
set(SB_CONFIG_BOOTLOADER_MCUBOOT y ...)   # Enables MCUboot as bootloader

ExternalZephyrProject_Add(
    APPLICATION wol_appcpu
    SOURCE_DIR  ${APP_DIR}/appcpu
    BOARD       esp32_devkitc/esp32/appcpu
)
sysbuild_add_dependencies(CONFIGURE wol_appcpu Wake_On_Lan)
```

---

## 🔁 Ping & WoL Logic

- **Ping interval:** every **1 minute** when the target is considered stable.
- **Failure detection:** 3 consecutive unanswered pings within 1-second intervals mark the target as offline.
- **WoL trigger:** pressing the **BOOT button** while the device is in station mode submits a UDP broadcast magic packet (port 9) via the system workqueue.
- **LED feedback:**
  - **2 blinks** — WoL magic packet sent (or factory reset triggered).
  - **1 blink** — Ping status changed (online ↔ offline).

---

## 🛠️ Tools

### `Tools/WakeOnLanMonitor.exe`
A standalone Windows utility that listens on UDP port 9 and displays each received WoL magic packet with sender IP, target MAC, and timestamp. Useful for verifying that the ESP32 is transmitting correctly without needing to power-cycle the target PC.

No installation required — just run the `.exe`.
