# 🚗 ROZORC Firmware

![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C)
![Framework](https://img.shields.io/badge/Framework-ESP8266-0277BD)
![PlatformIO](https://img.shields.io/badge/Tools-PlatformIO-f58220)

Firmware kustom berbasis **ESP8266** untuk mobil *Remote Control* (RC) yang dikendalikan melalui jaringan Wi-Fi lokal menggunakan antarmuka Web UI interaktif berbasis layar sentuh.

---

##  Fitur Utama

-  **Embedded Web UI Controller**: Tampilan kendali berbasis HTML5/JS yang responsif (full screen, feedback vibrasi, indikator jaringan, dan menu pengaturan).
-  **4-Speed Transmission (Gigi 1–4)**: Pilihan tingkat kecepatan dengan opsi **Safe Mode** dan **Race Mode** (dilindungi password).
-  **Failsafe System**: Penghentian otomatis motor jika koneksi terputus/kehilangan sinyal dalam jangka waktu tertentu (`800ms`).
-  **Dynamic Braking System**: Sistem pengereman transisi dinamis saat berpindah arah dari maju ke mundur.
-  **LittleFS Storage**: Menyimpan konfigurasi SSID, password Wi-Fi, dan password Race Mode secara permanen di memori flash.
-  **OTA (Over-The-Air) Update**: Pembaruan firmware nirkabel melalui ArduinoOTA.
-  **Control Accessories**: Kendali servo kemudi (*steering*) dan lampu LED.

---

## Spesifikasi Hardware & Pinout

| Komponen | Pin ESP8266 (Wemos D1 Mini / NodeMCU) | Fungsi |
|---|---|---|
| **Motor Drive A** | `D1` | Kendali Motor Maju/Mundur (PWM) |
| **Motor Drive B** | `D2` | Kendali Motor Maju/Mundur (PWM) |
| **Lampu LED** | `D3` | Output Lampu utama |
| **Servo Steering** | `D6` | Kendali Kemudi Servo (Kiri / Tengah / Kanan) |

---

## Konfigurasi Awal (Default Credentials)

Saat pertama kali di-flash, firmware akan membuat Access Point Wi-Fi dengan kredensial bawaan yang diatur di dalam kode:

| Parameter | Nilai / Keterangan |
|---|---|
| **SSID Wi-Fi** | Sesuaikan pada variabel `WIFI_SSID` di `main.cpp` |
| **Password Wi-Fi** | Sesuaikan pada variabel `WIFI_PASSWORD` di `main.cpp` (Min. 8 Karakter) |
| **Password Race Mode** | Sesuaikan pada `#define RACE_PASSWORD` di `main.cpp` |
| **Password OTA** | Sesuaikan pada variabel `HARDCODED_OTA_PASSWORD` di `main.cpp` |

> ⚠️ **Catatan Penting:** Kamu juga dapat mengganti SSID Wi-Fi, Password Wi-Fi, dan Password Race Mode kapan saja langsung melalui menu **SETTINGS** di Web UI tanpa perlu melakukan kompilasi ulang (re-flash) kode.

---

##  Cara Penggunaan & Setup

### 1. Build & Flash
1. Buka project ini menggunakan **VS Code** dengan extension **PlatformIO**.
2. Hubungkan board ESP8266 (Wemos D1 Mini / NodeMCU) ke komputer via kabel data.
3. Atur SSID dan Password sesuai keinginanmu pada bagian konfigurasi awal di file `src/main.cpp`.
4. Upload firmware ke board melalui PlatformIO.

### 2. Hubungkan & Kendalikan
1. Nyalakan ESP8266.
2. Hubungkan HP atau Laptop ke Access Point Wi-Fi sesuai SSID yang kamu tentukan.
3. Buka browser dan akses alamat IP: `http://192.168.4.1` (atau akan otomatis terbuka melalui Captive Portal).
4. Tekan tombol **GAS MULAI** untuk masuk ke layar utama kendali RC.

---

##  Struktur Project

```text
├── src/
│   └── main.cpp         # Kode utama firmware & embedded Web UI
├── .gitignore           # Konfigurasi pengabaian file build (.pio, .vscode)
└── platformio.ini       # Konfigurasi board & dependencies PlatformIO
