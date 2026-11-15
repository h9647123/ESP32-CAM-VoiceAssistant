# ESP32-CAM-VoiceAssistant

[中文](README.md) | **English**

A Chinese voice and image-question assistant for ESP32-S3, with OV2640 framing, multi-image context, voice drafts, a TF photo album, and AVI recording with audio. Current public source: **v0.4.31-cam · 2026-11-14**.

v0.4.27–v0.4.31 add complete loading of long-thinking history, math/chemistry fraction layout, gray thinking-text and scroll-cache fixes, and genuine streaming CUSTOM IAT capture/upload/incremental display. During the initial connection wait, the volume meter refreshes every 100 ms.

This edition combines photography, albums, multi-image questions, AVI with audio, TF sessions, search workflows, common formula layout, and progressive streaming text. The latest photo operation uses **two different keys, A→B, to confirm deletion**; B on the first confirmation page hides the original image. See [Latest changes and validation boundaries](docs/UPDATE-v0.4.31-cam.md) for release notes and known limits.

This is the **CAM camera hardware edition**. For the original I2C/AHT20 board, see [ESP32-VoiceAssistant](https://github.com/h9647123/ESP32-VoiceAssistant). The two editions have different pins and peripherals; use the corresponding source and device configuration. This repository publishes CAM source without device keys, private settings, Flash backups, personal conversations, photos, or recordings. Releases separately provide application firmware scanned for credentials.

## Features

- 16 kHz mono PCM16 capture; iFlytek V2 uses a dedicated capture task and chunked uploads, while CUSTOM IAT uploads during recording and displays incremental text. Capture is separate from network transmission, with clipping and overflow diagnostics.
- Retains Volcengine ASR, the original iFlytek interface, iFlytek IAT V2, and independent CUSTOM IAT. Self-hosted services must support the iFlytek V2 WebSocket/HMAC protocol.
- OpenAI-compatible model APIs, streaming responses, optional reasoning/thinking display, and search. Actual capabilities depend on the server and model.
- Speech becomes a draft that can be appended, undone, and confirmed before sending; supports common math/physics/chemistry symbols, scripts, and partial LaTeX display conversion.
- OV2640 scaled/full preview modes, FPS, album, up to three candidate images per turn, image-only or image-plus-voice questions, and original-image references retained for follow-up questions.
- QVGA/VGA/UXGA AVI recording with audio, video library, and confirmation before photo/video deletion; recordings can be converted to H.264/AAC MP4 on a computer.
- Ten independent sessions: 1–5 on onboard SPIFFS and 6–10 on TF. Existing paths are preserved, with write validation, previous-version backups, and card-identity protection.
- Two-level menus, B/C value editing, Chinese/English prompts, page fades and scrolling animations, low-flicker status bar and clock; automatic sleep from 0–30 minutes, with 0 keeping the screen on.
- Automatic ASR/AI network checks after connecting and synchronizing time; ASR checks every 60 seconds while idle, with failed AI checks retried. CUSTOM IAT can retry using another DNS resolution after public TLS connection failures while retaining certificate and hostname validation.

## Hardware and pins

The target is a custom CAM board: ESP32-S3-WROOM-1 N16R8 (16 MB Flash, 8 MB OPI PSRAM), OV2640, ST7789/GMT130 240×240, INMP441, SDMMC 1-bit TF card, and three active-low buttons. It does not use the generic AI-Thinker ESP32-CAM pin layout.

`VoiceAssistant/cam_board.h` defines the software pin map and overrides the corresponding peripheral definitions in older `config.h` files.

| Peripheral | GPIO / notes |
| --- | --- |
| OV2640 SCCB | SDA 4, SCL 5 |
| OV2640 synchronization | VSYNC 6, HREF 7 |
| OV2640 D0–D7 | 11, 9, 8, 10, 12, 18, 17, 16 |
| OV2640 clocks | XCLK 15, PCLK 13 |
| OV2640 RESET / PWDN | -1; handled by external board circuitry |
| ST7789 | SCK 48, MOSI 47, DC 2, backlight 39 |
| ST7789 CS / RST | -1 / -1; display RES connects to EN on this board |
| INMP441 | BCLK 14, WS 21, DIN 1; L/R connected to GND |
| TF SDMMC 1-bit | CLK 46, CMD 40, D0 41 |
| A / B / C | 42 / 38 / 3, internal pull-ups, active-low |

GPIO0 is for BOOT only. GPIO19/20 are reserved for native USB and 43/44 for UART. GPIO26–37 and 45 are not peripheral pins in this edition. CAM does not include DHT/AHT20/BMP280 drivers; GPIO16/17/18 are already used by the camera. Display rotation is fixed at 180°. The current lens mounting uses both vertical flip and horizontal mirroring, consistently across preview, JPEG capture, and upload.

Before powering up, check the FPC contact orientation, supply, SCCB, DVP, and TF nets against your board schematic. Different OV2640 modules may have incompatible ribbons and power definitions. Firmware cannot validate soldering or voltage. The board has no audio playback output.

## Build

Validated toolchain: Arduino CLI, **Arduino ESP32 3.3.8**, ArduinoJson **7.4.3**, LovyanGFX, WebSockets, and zlib_turbo. This public build used LovyanGFX 1.2.21, WebSockets 2.7.2, and zlib_turbo 1.0.0; record your installed versions when building for the first time. `esp_camera`, SD_MMC, I2S, and the CA bundle come from the ESP32 SDK.

Run from the repository root:

```powershell
Copy-Item VoiceAssistant/config.h.example VoiceAssistant/config.h
# Review config.h defaults before building; real keys can be configured at runtime.
arduino-cli core install esp32:esp32@3.3.8
arduino-cli lib install "ArduinoJson@7.4.3" "LovyanGFX" "WebSockets" "zlib_turbo"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build-cam.ps1 -OutputPath "$PWD/artifacts/firmware"
```

First-time Arduino CLI setup also requires configuring the Espressif board index through the normal process. Third-party SDKs and libraries are not bundled.

Fixed FQBN:

```text
esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,USBMode=hwcdc,CDCOnBoot=default
```

The default build uses **UART0 logging**. Add `-UsbCdcOnBoot` if using native USB and requiring CDC logs. The script compiles only; it does not flash automatically. Having 16 MB Flash does not automatically enlarge the 3 MiB `huge_app` application partition.

Before updating an existing device, verify the serial port, chip identity, security state, and actual partition layout; save a private full-chip backup and ensure the new image fits the current application partition. The development device uses application address `0x10000`; do not assume it applies to another partition layout. A blank chip also needs matching bootloader/partition files; writing only the application is insufficient. Preserve NVS, SPIFFS, existing sessions, and TF contents.

## Initial configuration

Configure the network, model, and services over local serial or the device menu's web debug interface. Successful settings are stored in NVS; `config.h` supplies defaults only. Web debugging is for trusted local networks: close it after use and do not expose it publicly.

Angle-bracket values below are placeholders. Do not paste real credentials into public issues, screenshots, or repositories. Serial tools may also retain command history.

```text
connect <ssid> <password>
provideradd <provider_name> https://api.example.com/v1 <api_key>
model <model_id> vision
```

For a text model, use `model <model_id> text`. When adding a model, explicitly mark whether it supports vision. Models from older configurations remain “unmarked”; capability is not guessed from names. Update the same model ID in the web interface or use:

```text
modelvision <model_id> vision
modelvision <model_id> text
```

Vision capability is keyed by the **complete API URL plus model ID**. Only explicitly vision-enabled models may send images. For unsupported or unmarked models, candidate photos are retained and a prompt asks you to switch or mark the model. The mark records the user's capability declaration; it does not guarantee actual remote support or recognition quality.

### ASR

Four providers can be configured and selected independently; the original iFlytek interface remains available:

| Selection | Purpose |
| --- | --- |
| `asrprovider volc` | Volcengine ASR, retaining API-key and legacy dual-credential configuration paths |
| `asrprovider xfyun` | Original iFlytek interface |
| `asrprovider xfyun2` | iFlytek large-model speech dictation V2 |
| `asrprovider custom` | Self-hosted iFlytek V2-compatible WebSocket ASR |

iFlytek V2:

```text
xfyun2asr <appid> <apikey> <apisecret>
asrprovider xfyun2
```

CUSTOM IAT:

```text
iaturl wss://asr.example.com/v1
iatasr <appid> <apikey> <apisecret>
asrprovider custom
```

`iatasr` credentials are stored separately from official iFlytek credentials. URLs support `ws://` / `wss://` with optional ports and paths. Plain ws can be used on a LAN; public deployments should use wss with a valid certificate. HTTPS/WSS defaults to the SDK CA bundle, NTP, and hostname verification. A private CA can be supplied through `CAM_TLS_CA_PEM`. Diagnostic certificate-verification bypass is disabled by default and should not be used for normal deployment.

CUSTOM IAT defaults to a maximum **60-second** button-held recording; release A to end early, with about 1.8 seconds of trailing audio reserved. This is not a silence-VAD cutoff. A 60-second buffer needs about 2.04 MB PSRAM, and allocation failures produce an explicit error. Other providers use `MAX_RECORD_SECONDS`, which defaults to 12 seconds in the template. Independent capture reduces the risk of audio loss from blocked network calls but does not guarantee server throughput or recognition accuracy.

## Buttons and menus

Footer prompts change with the current state. Draft, candidate-image, and error actions take priority over idle shortcuts.

| Context | Controls |
| --- | --- |
| Idle main screen | Tap A to open the menu; B/C browse content |
| Long-conversation browsing | Hold B/C to scroll continuously; while holding B/C, tap A to jump by turn |
| Ordinary voice question | Hold A to record, release to recognize; tap A to send the draft, C to undo the last segment |
| Root/submenus | B/C move, A enters; hold A to return one level/to the main screen |
| Multi-value menu item | Tap A to edit, B/C decrease/increase or switch, A finishes and saves |
| Live camera preview | Tap A to capture, B to zoom for inspection, C to exit; hold A to return to the menu |
| Photo confirmation | A selects/saves, B retakes, C cancels |
| Album | B goes older, C newer, A adds to candidates; hold A to return |
| Photo deletion/hiding | Double-tap C for the first confirmation: A proceeds, B hides, C cancels; on the second page B permanently deletes, C cancels; repeated A does not delete |
| Video deletion | Double-tap C opens preview confirmation; A confirms deletion; B/C or hold A cancels. This edition has not adopted the photo workflow's two-step confirmation for videos |
| Photo-only preview | Hold B to start recording with audio, tap A to stop; hold A still returns |
| Video library | A plays/pauses video, B goes older, C newer; hold A to return |

Menu groups are Settings, UI, Chat, AI, Camera, and System. They cover Wi-Fi scanning/saved networks, ASR/language, brightness/sleep/clock/theme/UI language, sessions/prompts/clearing, providers/models/thinking/search, camera modes/album/video library, and connection checks. The Chinese clear-context prompt is “上下文已清空” (“Context cleared”).

## Photography, album, and follow-up questions

1. Select “Photo question” in the camera menu. Hold A on the main screen to open the decision page: **A adds a photo, C chooses voice only, B uses selected images**.
2. After confirming a photo, return to the decision page and add more, up to **three per turn**. Hold B on that page, or choose “Send selected images” in the menu, to send images alone with a basic image-understanding instruction supplied by firmware.
3. After selecting images, return to the main screen, hold A to record, release to recognize, and tap A after the voice draft appears. With a voice draft, tap C to undo the last segment and hold C to remove one candidate image; with images only, tap C to remove one image.
4. Album selections append to the candidate list without uploading immediately. Album-image questions can temporarily override global “Photo only” mode; camera operation resumes afterward.

| Preview mode | Preview / final capture | Behavior |
| --- | --- | --- |
| Scaled preview | VGA / UXGA | Faster framing; the shutter switches modes and waits for exposure to settle; keep the camera angle still |
| Full preview | UXGA / UXGA | No sensor readout-mode switch, but slower preview |

Both modes produce 1600×1200 JPEGs. The 240×240 screen shows the 4:3 image proportionally with black bars. Actual FPS appears at the bottom right. During capture, the latest preview remains visible with “正在拍照…” (“Taking photo…”) below it. Zoom is for inspection, not autofocus. The current OV2640 module has no driver-supported autofocus; small text at close range requires manual focus and suitable distance.

Image references are archived with the user turn. Later voice/text-only follow-ups include the original turn's photos, up to the **six most recent images** per request. Exceeding the budget explicitly indicates that older originals were omitted. Missing cards, deleted photos, or byte-count mismatches produce errors instead of silently proceeding without images. Images held only in PSRAM after failed archival can be sent in the current request but cannot be restored after memory is freed or the device restarts. Old firmware history without saved image references requires selecting the images again.

New photos use `P-YYYYMMDDHHmmss.jpg` and videos use `V-YYYYMMDDHHmmss.avi`, with compatibility for older names. Same-second captures and captures without a trusted clock use sorting metadata to preserve order. Offline captures remain ordered; unknown times are labeled explicitly rather than invented. The album indexes the newest **200 items** at most. Older saved photos are not automatically deleted and can be managed on a computer.

## Hide and restore photos

Tap B on the album's first deletion-confirmation page to hide an image. The original JPEG and `.meta` remain on TF. Album and selection lists skip images with a matching `.jpg.hidden` marker. There is no hidden-photo list or restore menu on the device. Hiding does not delete originals or rewrite existing chat image references.

```text
cam hidden list
cam unhide <actual-photo-filename.jpg>
cam photo verify <actual-photo-filename.jpg>
```

You can also restore a photo on a computer by deleting its `.jpg.hidden` marker while retaining the JPEG and `.meta`. Video saves and photo hide/restore operations invalidate the relevant media cache; idle prewarming runs automatically. The first open after a cold start, card reinsertion, or content changes can still take time.

## Search and page reading

Search providers include Tavily, DeepSeek Search, and self-hosted DeepSeek Search-compatible services. Model and search providers are configured independently. In AUTO, model planning produces a bounded number of queries, followed by search, filtering, on-demand page reading, and a bounded source-attributed summary used for the answer and subsequent context. The device displays stages and counts. Availability depends on the network, services, and model compatibility. **HTTP 200 alone does not prove a usable summary or a successful end-to-end workflow.**

In the recorded tests, ciyuanfast + H964 and DeepSeek + official Search succeeded end to end. DeepSeek + H964 and ciyuan-A + H964 returned summary HTTP 200 but zero usable evidence; ciyuan-A's final request returned HTTP 400. Tavily had no credentials configured and was not tested. These results do not establish compatibility for every model/service combination.

## Video with audio and MP4

In photo-only live preview, hold B to start and tap A to stop. Recordings are saved under TF `/video/` as **MJPEG + PCM16/16 kHz mono AVI**, not device-encoded MP4. Audio uses a dedicated I2S task and a 128 KiB PSRAM ring buffer. Each recording is limited to 10 minutes or 512 MiB; write errors are reported as failures.

Available resolutions are QVGA 320×240, VGA 640×480 (default), and UXGA 1600×1200. UXGA displays a performance warning. One short-clip test measured approximately 9.1 fps at QVGA, 3.9 fps at VGA, and 0.55 fps at UXGA; these are not sustained-performance guarantees. The device video library plays images without sound. Audio remains in the file and is audible on a computer or after conversion.

Install FFmpeg/ffprobe and add them to PATH, then run:

```powershell
python tools/convert-video-mp4.py "F:/video/example.avi"
```

The tool retains the source AVI, refuses to overwrite an existing same-name MP4, outputs H.264/AAC, and checks audio/video tracks. MP4 conversion runs on the computer.

## TF, sessions, and compatibility

| Sessions | Location |
| --- | --- |
| 1–5 | SPIFFS `/chat/session1.jsonl` … `/chat/session5.jsonl` |
| 6–10 | TF `/chat/session6.jsonl` … `/chat/session10.jsonl` |

Select a session through “Chat → Chat sessions.” RAM loading windows and history budgets remain bounded rather than doubling with more sessions. `/history/` is a separate append-only archive and is not automatically loaded in full into model context. Prompts remain onboard. Original images are usually on TF, so follow-up image questions depend on the corresponding card/files even when the text session is in Flash.

Saving writes and validates a `.tmp`, retains a `.bak`, then replaces the main file. If the main file is missing, recovery attempts the previous version rather than treating an incomplete temporary file as history. The RAM session is bound to a random TF card identifier. Missing/replaced cards or save failures retain RAM and block further requests, clearing, or switching until the original card returns, preventing writes to the wrong card. Booting into a TF session without a card falls back to Flash session 1 without overwriting TF history.

TF mounts at boot and is checked about every two seconds while idle. Reinsertion triggers a temporary-file read/write self-test. Capacity is shown in decimal MB and the filesystem is FAT32. Hot-plug logic is implemented, but physical removal and power-loss atomicity still require validation with your own card.

Use serial `sd info` / `sd mount` / `sd off` for diagnostics. `sd format` first displays a warning and a random confirmation command valid for 30 seconds. Only an exact confirmation formats the current TF as FAT32 with 32 KiB clusters; it does not proactively format SPIFFS/NVS. Formatting deletes card data. Back up first, switch to sessions 1–5, stop recording/the camera, and close web debugging. Development acceptance did not perform a real format.

Old NVS, SPIFFS, and JSONL remain readable. Older models need explicit vision-capability marks; old conversations without image references cannot reconstruct original photos. Do not directly copy pin configurations or Flash images between CAM and original boards.

## Streaming responses and scientific symbols

Common input such as `\boxed{a\in(-\infty,-1]}` displays as `a∈(-∞,-1]`. Scientific glyphs, Unicode scripts, and inline script bounds help avoid overlapping neighboring Chinese lines. Unknown LaTeX commands and incomplete groups retain their source text; code blocks remain literal. This is not a complete LaTeX layout engine and does not correct a model's mathematical conclusions.

Long thinking uses an approximately 6–8 KiB scrolling display window, with real-time layout processing roughly the last 768 bytes. Visible answers are limited to 48 KiB. Once SSE has arrived, the same long-thinking request is not automatically regenerated, preventing endless retries. Image requests have an approximately five-minute budget; C cancels during streaming. Upload and response-header waits run in a worker so the main UI can refresh NTP/elapsed wait and poll cancellation; underlying network calls still have timeout constraints. Thinking time is based on actual reasoning/think data, not waiting time. Models without reasoning fields do not get a fabricated thinking duration. Progressive text display advances at UTF-8 boundaries, automatic following stays directly on the last line, and manual B/C scrolling remains available.

## Project origins

The project branched from the original voice assistant into Route A CAM hardware support. Initial work established fixed pins, NVS isolation, streaming JPEG, and TF archival, followed by USB/RST black-screen, FPC contact, and corrupted-JPEG investigation on an older module. A separate CamProbe, CRC checks at four points, DRAM/PSRAM/10–20 MHz comparisons, strict decoding, and passive SCCB diagnostics narrowed the problem. Static continuity or matching CRCs were not treated as proof of a working camera, and uncertain module specifications were not used to guess voltage changes.

The voice path retained original iFlytek support and added V2/CUSTOM, first addressing clipping, then incorporating original v1.1.42's independent capture task, upload after complete PCM capture, anti-flicker fixes, scientific symbols, and streaming responses. Public access was compared using LAN/Cloudflare/FRP with real PCM and separate timings. After installing a working new module, iterations covered vertical/horizontal orientation, manual-focus limits, preview/shutter exposure consistency, actual FPS, independent button sampling, albums, and unified menus. Later work added video with audio, hot-plug handling, multi-image decision pages, persistent image context, long-thinking protection, TF sessions, and model capability marking. v0.4.13 rebound TLS trust after stop and passed device regression. See CHANGELOG for stage conclusions and superseded early behavior.

[Architecture and diagnostic decisions](docs/DEVELOPMENT.md) covers modules, older hardware investigation, audio/public-network methods, and CamProbe tools. It complements the current controls in this README without requiring private handover files to understand the project.

## Validation and development history

[CHANGELOG.md](CHANGELOG.md) rewrites local development logs into public version history. [docs/VALIDATION.md](docs/VALIDATION.md) distinguishes compilation, offline logic, network probing, hardware, and live human input. Raw logs, serial identifiers, SSIDs, media, and personal context are not published.

v0.4.26 was flashed to a real CAM device's application partition only. Photo hiding/remounting/unchanged CRC/serial restoration and different-key deletion confirmation passed, with deletion limited to specially created test photos. Other features rely on evidence from their respective development stages; older live speech or three-resolution video results are not claimed as a complete retest of this version. See [Validation summary](docs/VALIDATION.md) for search compatibility, cold-cache waits, history browsing, and complex-TeX limits.

See [tools/README.md](tools/README.md) for public offline tests; they do not connect to devices. Third-party glyph licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Project code is open source under the [MIT License](LICENSE), Copyright 2026 h9647123. Noto-derived glyphs remain under SIL OFL 1.1, and other dependencies retain their own licenses. MIT does not replace those third-party terms.

Draft editing: tap C to delete one UTF-8 character; double-tap C to open whole-draft-clear confirmation. A confirms, B cancels, and C cancels confirmation and continues deleting, retaining candidate photos. Changing pages, other buttons, or starting a new recording interrupts double-tap detection.

Menu settings with exactly two values toggle directly with a tap of A. Multi-value settings use A to enter B/C editing. Streaming recognition and draft editing reuse unchanged history layout to reduce repeated parsing in long conversations.
