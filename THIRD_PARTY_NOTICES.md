# Third-party notices

`VoiceAssistant/science_glyphs.h`, `common_science_glyphs.h`, `science_script11.h`, `science_script12.h` and `traditional_glyphs.h` contains raster glyphs derived from Noto Sans, Noto Sans SC, Noto Sans Math and Noto Sans Symbols 2. The source fonts are distributed under the SIL Open Font License 1.1. Their copyright and license texts are preserved in `licenses/*-OFL.txt`. Those license terms apply to the corresponding font-derived assets.

The build uses Arduino ESP32 / ESP-IDF (including esp32-camera), ArduinoJson, LovyanGFX, WebSockets and zlib_turbo as separately installed dependencies. This repository does not redistribute their SDK/library packages; consult the license included in each dependency. Their names are used to identify build requirements.

Project code is distributed under the MIT License; see `LICENSE` (Copyright (c) 2026 h9647123). Third-party components and font-derived assets retain their own copyright notices and licenses. The project MIT License does not replace or relicense those third-party materials.

The application firmware links separately installed Arduino ESP32 3.3.8, ArduinoJson 7.4.3, LovyanGFX 1.2.21, WebSockets 2.7.2 and zlib_turbo 1.0.0. Bundled dependency license texts are in licenses/. Their terms continue to apply to linked code. Matching source projects: https://github.com/espressif/arduino-esp32/tree/3.3.8 ; https://github.com/bblanchon/ArduinoJson/tree/v7.4.3 ; https://github.com/lovyan03/LovyanGFX/tree/1.2.21 ; https://github.com/Links2004/arduinoWebSockets/tree/2.7.2 ; https://github.com/bitbank2/zlib_turbo . Public application source and rebuild instructions are provided so users can modify and rebuild/relink with compatible libraries.
