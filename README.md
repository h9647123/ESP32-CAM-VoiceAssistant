# ESP32-CAM-VoiceAssistant

ESP32-S3 上的中文语音与拍照问答助手，支持 OV2640 取景、多图上下文、语音草稿、TF 相册和带声音的 AVI 录像。当前发布源码 **v0.4.13-cam · 2026-10-31**。

这是 **CAM 摄像头硬件版**；带 I2C/AHT20 的原版见 [ESP32-VoiceAssistant](https://github.com/h9647123/ESP32-VoiceAssistant)。两版引脚与外设不同，请使用对应源码和设备配置。本仓库只发布 CAM 公共源码，不包含设备密钥、私有配置、Flash 备份、个人对话、照片、录音或编译好的固件。

## 功能

- 16 kHz PCM16 单声道采音；讯飞 V2/CUSTOM IAT 先完整缓冲采音再上传，采音任务与网络发送分离，提供削波和溢出诊断。
- 保留火山 ASR、旧讯飞接口、讯飞 IAT V2 和独立 CUSTOM IAT；自建服务需兼容讯飞 V2 的 WebSocket/HMAC 协议。
- OpenAI 兼容模型接口、流式回复、可选推理/思考显示和搜索；实际能力由服务端与模型决定。
- 语音先成为草稿，可追加、撤销，再确认发送；支持常见数学/物理/化学符号、上下角标和部分 LaTeX 显示转换。
- OV2640 缩放/完整两种预览、FPS、相册、每轮最多 3 张候选图、仅发送图片或图片配语音；后续问答保留原图引用。
- QVGA/VGA/UXGA 有声 AVI 录像、视频集、照片/视频删除二次确认；电脑可转 H.264/AAC MP4。
- 10 个独立会话：1—5 在板载 SPIFFS，6—10 在 TF；保留旧会话路径，增加写入校验、上一版备份和卡身份保护。
- 两级 MENU、B/C 编辑选项、中英文提示、页面渐变与滚动动画、低闪烁状态栏和时钟；自动休眠 0—30 分钟，0 为常亮。
- 联网校时后自动检测 ASR/AI 网络；空闲时每 60 秒检查 ASR，失败 AI 会重查。CUSTOM IAT 公网 TLS 连接失败后可换 DNS 重试，保留证书与域名校验。

## 硬件与引脚

目标为自定义 CAM 板：ESP32-S3-WROOM-1 N16R8（16 MB Flash、8 MB OPI PSRAM）、OV2640、ST7789/GMT130 240×240、INMP441、SDMMC 1-bit TF 卡和三个低电平按键。它并非通用 AI-Thinker ESP32-CAM 引脚布局。

`VoiceAssistant/cam_board.h` 是本版软件引脚入口，并覆盖旧 `config.h` 中的相关外设定义。

| 外设 | GPIO / 说明 |
| --- | --- |
| OV2640 SCCB | SDA 4，SCL 5 |
| OV2640 同步 | VSYNC 6，HREF 7 |
| OV2640 D0—D7 | 11、9、8、10、12、18、17、16 |
| OV2640 时钟 | XCLK 15，PCLK 13 |
| OV2640 RESET / PWDN | -1，由板上外部电路处理 |
| ST7789 | SCK 48，MOSI 47，DC 2，背光 39 |
| ST7789 CS / RST | -1 / -1；本板显示 RES 接 EN |
| INMP441 | BCLK 14，WS 21，DIN 1；L/R 接 GND |
| TF SDMMC 1-bit | CLK 46，CMD 40，D0 41 |
| A / B / C | 42 / 38 / 3，内部上拉，按下接地 |

GPIO0 仅用于 BOOT，19/20 留给原生 USB，43/44 留给 UART，26—37 和 45 不作为此版外设引脚。CAM 不包含 DHT/AHT20/BMP280 驱动，16/17/18 已用于摄像头。显示方向固定为 180°；当前镜头装配使用上下翻转和左右镜像，预览、JPEG 和上传保持一致。

上电前按自己的板卡原理图核对 FPC 触点方向、供电、SCCB、DVP 与 TF 网络；不同 OV2640 模组的排线和电源定义不能直接互换。固件不能验证焊接和电压。板卡不提供音频播放输出。

## 构建

已验证工具链：Arduino CLI、**Arduino ESP32 3.3.8**，以及 ArduinoJson **7.4.3**、LovyanGFX、WebSockets、zlib_turbo。后面三项的版本未在本项目锁定，首次构建应记录自己安装的版本。`esp_camera`、SD_MMC、I2S 和 CA bundle 来自 ESP32 SDK。

在仓库根目录执行：

```powershell
Copy-Item VoiceAssistant/config.h.example VoiceAssistant/config.h
# 先检查 config.h 的默认配置，再编译；所有真实密钥可留待运行时配置。
arduino-cli core install esp32:esp32@3.3.8
arduino-cli lib install "ArduinoJson@7.4.3" "LovyanGFX" "WebSockets" "zlib_turbo"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build-cam.ps1 -OutputPath "$PWD/artifacts/firmware"
```

首次使用 Arduino CLI 还需按其正常流程配置 Espressif 开发板索引。本仓库未打包第三方 SDK/库。

固定 FQBN：

```text
esp32:esp32:esp32s3:PartitionScheme=huge_app,PSRAM=opi,FlashSize=16M,USBMode=hwcdc,CDCOnBoot=default
```

默认构建走 **UART0 日志**；接原生 USB 并需要 CDC 日志时给脚本加 `-UsbCdcOnBoot`。脚本仅编译，不自动烧录。16 MB Flash 不会自动扩大 `huge_app` 的 3 MiB 应用分区。

更新已有设备前，确认串口、芯片身份、安全状态和实际分区，保存私有整片备份，确认新镜像能放入当前 app 分区。开发实机使用的 app 地址为 `0x10000`；其他分区布局不能照搬这个地址。首次空片安装还需要匹配的 bootloader/partition，不能只写 app。避免覆盖 NVS、SPIFFS、已有会话和 TF。

## 首次配置

通过本地串口或设备 MENU 中的 Web 调试配置网络、模型与服务；连接成功后数据保存在 NVS，`config.h` 只提供默认值。Web 调试仅用于可信本地网络，使用完关闭，不向公网暴露。

下面的尖括号均为占位符。不要将真实凭据粘贴到公开 Issue、截图或版本库；串口工具的命令历史也可能记录输入。

```text
connect <ssid> <password>
provideradd <provider_name> https://api.example.com/v1 <api_key>
model <model_id> vision
```

文本模型改用 `model <model_id> text`。添加模型时必须明确标记是否支持识图；旧配置中的模型会保持“未标注”，不会根据名字猜测能力。在 Web 中用同一模型 ID 更新，或使用：

```text
modelvision <model_id> vision
modelvision <model_id> text
```

识图能力按**完整 API URL＋模型 ID**区分。只有明确标记支持的模型能发送图像；标记不支持或未标注时保留候选照片并提示切换/标记。这个标记表示用户确认能力，不保证远端模型实际支持或识图质量。

### ASR

四个提供商可独立切换，原讯飞接口保留：

| 选择 | 用途 |
| --- | --- |
| `asrprovider volc` | 火山 ASR，保留 API key / 旧双凭据配置路径 |
| `asrprovider xfyun` | 原讯飞接口 |
| `asrprovider xfyun2` | 讯飞语音听写大模型 V2 |
| `asrprovider custom` | 兼容讯飞 V2 的自建 WebSocket ASR |

讯飞 V2：

```text
xfyun2asr <appid> <apikey> <apisecret>
asrprovider xfyun2
```

CUSTOM IAT：

```text
iaturl wss://asr.example.com/v1
iatasr <appid> <apikey> <apisecret>
asrprovider custom
```

`iatasr` 与讯飞官方凭据分开保存；URL 支持 `ws://` / `wss://`、可选端口与路径。局域网可用明文 ws，公网应配置有效证书的 wss。HTTPS/WSS 默认使用 SDK CA bundle、NTP 和域名校验；私有 CA 可配置 `CAM_TLS_CA_PEM`。诊断用不校验证书选项默认关闭，不应作为正常部署方式。

CUSTOM IAT 默认最长按键录音 **60 秒**，松开 A 可提前结束，另保留约 1.8 秒尾音；这不是静音 VAD 截止。60 秒缓冲约需 2.04 MB PSRAM，分配失败明确报错。其他提供商使用 `MAX_RECORD_SECONDS`，模板默认 12 秒。独立采音可降低网络阻塞导致丢音的风险，不保证任何服务器的吞吐和识别准确率。

## 按键与 MENU

界面底部提示随当前状态变化；有草稿、候选图和错误时，其操作优先于空闲快捷键。

| 场景 | 操作 |
| --- | --- |
| 空闲主界面 | 短 A 打开 MENU；B/C 浏览内容 |
| 连续浏览长对话 | 长 B/C 连续滚动；保持长 B/C 时短 A 按轮跳转 |
| 普通语音问题 | 长 A 录音、松开识别；草稿短 A 发送，C 撤销末段 |
| MENU 根项/子项 | B/C 移动，A 进入；长 A 返回上级/主界面 |
| 多值菜单项目 | 短 A 进入编辑，B/C 减增或切换，A 完成并保存 |
| 实时相机预览 | 短 A 拍照，短 B 放大检查，短 C 退出，长 A 返回 MENU |
| 照片确认 | A 选用/保存，B 重拍，C 取消 |
| 相册 | B 较旧，C 较新，A 选入候选区，长 A 返回 |
| 相册/视频集删除 | 双 C 打开确认，A 删除，B/C 或长 A 取消 |
| 仅拍照的预览 | 长 B 开始有声录像，松 B 停止；长 A 仍为返回 |
| 视频集 | A 播放/暂停画面，B 较旧，C 较新，长 A 返回 |

MENU 分为设置、UI、聊天、AI、相机、系统。包括 Wi-Fi 搜索/已存网络、ASR/语言、亮度/休眠/时钟/主题/界面语言、会话/提示词/清空、服务商/模型/思考/搜索、相机模式/相册/视频集和连接检测等。中文界面的清空提示为“上下文已清空”。

## 拍照、相册与多轮追问

1. 在相机菜单选“拍照提问”。主界面长 A 进入判断页：**A 添加照片，C 仅语音，B 使用已选图片**。
2. 拍照确认后返回判断页，可继续添加，每轮最多 **3 张**。判断页长 B 或菜单“发送已选图片”可仅发图片，固件附带基本看图指令。
3. 选图后返回主界面，长 A 录音、松开识别，看到语音草稿后短 A 发送。主界面短 C 撤回末张候选图。
4. 相册选图会追加候选，不会立即上传。即使全局开启“仅拍照”，也可临时使用相册图片问答，结束后恢复相机操作。

| 预览模式 | 预览 / 正式拍照 | 特性 |
| --- | --- | --- |
| 缩放预览 | VGA / UXGA | 取景较快，快门会切换并等待曝光稳定，需保持拍摄角度不动 |
| 完整预览 | UXGA / UXGA | 不切换传感器读出，预览较慢 |

两者正式照片均为 1600×1200 JPEG；240×240 屏幕等比例显示 4:3 画面，保留黑边。取景右下角显示实际 FPS；拍照期间保留最近预览，下方显示“正在拍照…”。放大是检查取景，不是自动对焦；当前 OV2640 模组没有驱动支持的自动对焦，近距离小字需手动调焦和合适距离。

图片引用与用户轮次共同存档；后续纯语音/文字追问会带回原轮的照片，每次最多携带最近 **6 张**。超过预算会明确提示旧原图未携带；缺卡、照片删除或字节数不符会报错，不默默改成无图猜测。仅存在 PSRAM、未归档成功的图可用于当次发送，但释放内存或重启后无法恢复。旧固件未保存引用的历史需重新选图。

照片名包含持久递增序号和可信时钟的时间戳；无网络校时仍能按序排列，时间未知会明确标注，不伪造拍摄时间。相册最多索引最新 **200 项**，超过数量不会自动删除已保存照片；可在电脑上管理更早文件。

## 有声录像与 MP4

仅拍照模式实时预览里，长 B 开拍、松 B 结束。录像保存 TF `/video/`，格式为 **MJPEG + PCM16/16 kHz 单声道 AVI**，不是板端 MP4 编码。音频使用独立 I2S 任务和 128 KiB PSRAM 环形缓冲。每段上限 10 分钟或 512 MiB；异常写入会显示失败。

分辨率可选 QVGA 320×240、VGA 640×480（默认）、UXGA 1600×1200。UXGA 会提示性能限制。一次短片测试约为 QVGA 9.1 fps、VGA 3.9 fps、UXGA 0.55 fps，这些数值不保证持续录像性能。板端视频集播放画面，不播放声音；声音保存在文件内，电脑播放或转换可听。

安装 FFmpeg/ffprobe 并加入 PATH 后：

```powershell
python tools/convert-video-mp4.py "F:/video/example.avi"
```

保留源 AVI，拒绝覆盖同名已有 MP4，输出 H.264/AAC 并检查音视频轨道。MP4 转换在电脑执行。

## TF、会话与兼容性

| 会话 | 位置 |
| --- | --- |
| 1—5 | SPIFFS `/chat/session1.jsonl` … `/chat/session5.jsonl` |
| 6—10 | TF `/chat/session6.jsonl` … `/chat/session10.jsonl` |

从“聊天 → 对话会话”选择。RAM 加载窗口和历史预算保持有界，不因新增会话翻倍。`/history/` 是独立追加归档，不会自动全部导入模型上下文；提示词仍在板载存储。图片原件通常在 TF，即使文字会话在 FLASH，发图追问仍依赖对应卡与文件。

保存先写 `.tmp` 并校验，再保留 `.bak` 后替换；主文件缺失时尝试恢复上一版，不把不完整临时文件当历史。当前 RAM 会话绑定 TF 随机卡标识；缺卡、换卡或保存失败时保留 RAM 并拦截继续请求/清空/切换，等待原卡恢复，避免写错卡。开机选中 TF 会话但无卡时退回 FLASH 会话 1，不覆盖 TF 历史。

TF 上电挂载，空闲约每 2 秒检查，重新插入时读写临时文件自检，容量显示十进制 MB。文件系统使用 FAT32。热插拔逻辑已实现，但物理拔卡/断电原子性仍须按自己的卡验收。

串口 `sd info` / `sd mount` / `sd off` 可诊断。`sd format` 只先显示警告和 30 秒随机确认命令；准确确认后才格式化当前 TF FAT32（32 KiB 簇），不会主动格式化 SPIFFS/NVS。格式化会删除卡内数据；先备份，切到会话 1—5，停止录音/相机并关闭 Web 调试。开发验收未执行真实格式化。

旧 NVS、旧 SPIFFS 和旧 JSONL 保持可读；旧模型须显式补识图能力，旧无图引用会话不能凭空恢复原照片。不要在 CAM 和原板间直接复制引脚配置或 Flash 镜像。

## 流式回复和科学符号的范围

常见 `\boxed{a\in(-\infty,-1]}` 会显示为 `a∈(-∞,-1]`；增加科学字形、Unicode 上下角标及上下角标行内约束，避免覆盖相邻中文行。未知 LaTeX 命令和不完整分组保留原文；代码段按字面显示。它不是完整 LaTeX 排版引擎，不会修正模型的数学结论。

长思考使用约 6—8 KiB 滚动显示窗口，实时排版处理尾部约 768 字节；可见回答上限 48 KiB。已收到 SSE 后不自动重新生成同一长思考，避免无限重试；图片请求约 5 分钟预算，流式接收时 C 可取消。同步上传/等待响应头期间取消需等网络调用返回，响应头最多 60 秒。长网络调用仍可能影响按键响应。

## 项目从何而来

项目从原语音助手分出路线 A CAM 硬件适配，先完成固定引脚、NVS 隔离、流式 JPEG 和 TF 归档，再经历 USB/RST 黑屏、FPC 接触、旧模组 JPEG 花屏的 bring-up。独立 CamProbe、四处 CRC、DRAM/PSRAM/10—20 MHz 对照、严格解码和 SCCB 被动诊断用于缩小问题；没有将静态通断或 CRC 相同当作相机正常，也没有在模组规格矛盾时猜电压改板。

语音路线保留原讯飞并加入 V2/CUSTOM，先处理采音削波，再合入原版 v1.1.42 的独立任务采音、完整 PCM 后上传、防闪烁、科学符号和流式回复；公网按 LAN/Cloudflare/FRP、真实 PCM/耗时分别对照。换上可用新模组后，迭代上下/左右方向、手动调焦边界、预览/快门曝光一致性、真实 FPS、独立按键采样、相册和统一 MENU。随后扩展有声视频、热插拔、多图判断页、持久图片上下文、长思考保护、TF 会话和模型能力标记。v0.4.13 修复 stop 后 TLS 信任重绑并完成上板回归。各阶段结论及被替换的早期行为见 CHANGELOG。

[架构与诊断决策](docs/DEVELOPMENT.md) 汇总各模块、旧硬件调查、音频/公网方法和 CamProbe 工具；它与 README 的当前操作相互补充，不需要私人交接文件即可了解全项目。

## 验证记录与开发沿革

[CHANGELOG.md](CHANGELOG.md) 将本地开发日志重写为公开版本沿革；[docs/VALIDATION.md](docs/VALIDATION.md) 区分编译、离线逻辑、网络探测、实机和真人输入。未公开原始日志、串口识别信息、SSID、媒体或个人上下文。

v0.4.13 已在 CAM 实机仅更新 app，并验证 TF 会话测试内容跨重启恢复、菜单编辑/30 分钟上限、布局/语言检查、ASR 101 和 AI 网络检测。用户随后确认实体操作手感正常。最后一轮不包含真人 ASR、多图模型推理、物理 TF 拔插验收，不能将网络正常等同于识别/回答质量通过。

公开离线测试运行说明见 [tools/README.md](tools/README.md)。它们不连接设备。源码的第三方字形许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。项目自身当前未额外授予开源许可证；公开可见不等于授予任意再分发许可。
