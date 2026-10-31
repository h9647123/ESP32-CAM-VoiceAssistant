#pragma once

// CAM/P1 + Camera_OV2640, frozen 2026-10-01. Board1 is a different board.
// Force the CAM wiring even if an old private config.h was copied here.
#define CAM_FIRMWARE 1
#undef TFT_MOSI
#undef TFT_SCLK
#undef TFT_RST
#undef TFT_DC
#undef TFT_BL
#undef TFT_CS
#undef TFT_ROTATION
#undef MIC_I2S_BCLK
#undef MIC_I2S_WS
#undef MIC_I2S_DIN
#undef BUTTON_START_PIN
#undef BUTTON_NEXT_PIN
#undef BUTTON_PREV_PIN
#undef SENSOR_I2C_AHT20_BMP280
#undef DHT_PIN
#undef DHT11_PIN
#define TFT_MOSI 47
#define TFT_SCLK 48
#define TFT_RST -1
#define TFT_DC 2
#define TFT_BL 39
#define TFT_CS -1
#define TFT_ROTATION 2
#define MIC_I2S_BCLK 14
#define MIC_I2S_WS 21
#define MIC_I2S_DIN 1
#define BUTTON_START_PIN 42
#define BUTTON_NEXT_PIN 38
#define BUTTON_PREV_PIN 3
#define SENSOR_I2C_AHT20_BMP280 0
#define DHT_PIN -1
#define DHT11_PIN -1

#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16
#define CAM_PIN_PCLK 13
#define CAM_PIN_XCLK 15
#define CAM_PIN_RESET -1
#define CAM_PIN_PWDN -1
#define CAM_SD_CLK 46
#define CAM_SD_CMD 40
#define CAM_SD_D0 41

// PEM may be supplied for a private service; public services use the SDK bundle.
#ifndef CAM_TLS_CA_PEM
#define CAM_TLS_CA_PEM nullptr
#endif
#ifndef CAM_TLS_DIAGNOSTIC_INSECURE
#define CAM_TLS_DIAGNOSTIC_INSECURE 0
#endif
#define CAM_JPEG_MAX_BYTES (1024U * 1024U)
#define CAM_PHOTO_ARCHIVE_LIMIT 200
#define CAM_HISTORY_FILE_LIMIT (512U * 1024U)
#define CAM_HISTORY_FILES_PER_SESSION 4
