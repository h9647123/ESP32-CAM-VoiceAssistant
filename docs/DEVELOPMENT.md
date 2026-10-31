# 架构、硬件 bring-up 与工具

## 模块分工

| 模块 | 作用 |
| --- | --- |
| VoiceAssistant.ino | 状态机、主 UI/MENU、串口/Web 配置、ASR/LLM、会话和流式文本 |
| cam_board.h / cam_tls.h | 固定引脚覆盖、显示方向、CA bundle/时钟 |
| buffered_mic.h / asr_capture_limits.h | 独立 I2S、完整 PCM 缓冲、录音和上传容量策略 |
| xfyun_iat_v2.h / iat_endpoint.h | 讯飞 V2 协议、动态修正、CUSTOM URL 校验 |
| iat_dns_packet.h / iat_dns_fallback.h | 有界 DNS A/CNAME 解析、传输失败后备用解析、重绑 TLS 信任 |
| cam_capture.h / cam_jpeg_stream.h | JPEG 所有权、PSRAM/TF 回退、流式 Base64 编码 |
| cam_photo_ui.h / cam_question_ui.h | 两种预览、确认/判断页、候选图与语音流程 |
| button_sampler.h | 独立 GPIO 采样，慢取景/解码时仍记录事件 |
| cam_album.h / cam_photo_order.h | 相册索引、无时钟序号、跨重启排序 |
| cam_video.h / cam_avi_format.h / cam_video_mic.h | AVI 写入/索引恢复、PCM 环形缓冲、视频集画面播放 |
| cam_media_delete*.h | 锁定删除对象、双击和二次确认 |
| cam_vision.h / cam_vision_policy.h | 多模态请求、原轮图片上下文、受限重试和取消 |
| cam_chat_storage.h / cam_sd_hotplug.h / cam_sd_diagnostics.h | TF 会话事务、原卡保护、挂载自检、确认式格式化 |
| cam_model_capabilities.h | 按服务/模型持久化识图三态 |
| science_glyphs.h / science_renderer.h / latex_display.h | 科学字形、上下角标、常见 LaTeX 的轻量显示 |
| service_monitor.h / service_probe_policy.h | 空闲后台 ASR/AI 检测、超时/周期/恢复 |
| debug_page.h / ui_health_audit.h / cam_v046_audit.h | 本地网页与可重复的板端审查入口 |

## 旧硬件故障留下的诊断原则

早期旧模组曾 JPEG 花屏，独立 CamProbe 去掉主业务的 Wi-Fi/I2S/TFT/NVS/TF，比较 10/20 MHz、PSRAM/内部 DRAM 和场景/彩条。对驱动返回、独立副本、归还后的副本和 USB 收到原图计算相同 CRC。CRC 一致只能证明该观察点之后字节没变；驱动前的 DVP/DMA/拼帧仍未被排除。

12 张正式对照图 CRC 一致但 FFmpeg 严格解码失败；内部 RAM 仍失败，故不能把问题简单归到 PSRAM。JPEG 有头尾、PID 正常、Pillow 可 load 都不足以证明图像质量。主机工具后来明确要求完整协议、数量、长度、四处 CRC、JPEG 尺寸、Pillow 和 FFmpeg 严格解码全部通过；仍需实际场景目视。

具体旧模组资料的 DVDD 标签与参数范围存在矛盾，不能直接套裸芯片或另一开发板电压。设计网表、断电通断、静态电压、动态波形是不同证据。没有记录示波器波形或精确相机库 commit 时保持未知，不把新版源码当作已安装预编译库的事实。

对本机预编译 SCCB 驱动的反汇编确认过初始化请求开启内部上拉；这不能给出实物阻值/上升沿。SCCB_Read 的底层失败可被压成零，读改写 setter 返回成功不足以证明中间读取有效。独立诊断增加被动链接 wrapper，观察已有事务而不额外写传感器寄存器。

后来换上新模组后能严格解码和正常预览，软件继续迭代取景/拍照/曝光。换件和重插接触同时改变，不能反推旧模组一定损坏。当前手动调焦、自动曝光和放大取景也须分别判断。

## 公网与音频

先检查源 PCM 长度、哈希、削波、I2S/软件溢出和读错误，再比较上传与 final 耗时。真实重复/改口应向说话者核对；不能按文本重复直接去重。LAN 成功不代表公网吞吐相同，Cloudflare/FRP 的一次对照不等于普遍性能保证。

CUSTOM DNS 重试只在系统路径传输失败后启用，对 IP、本地域名或明文 LAN 不启用。每次 stop 后重新绑定 CA 信任；连接目标 IP 可变，SNI/证书主机名/签名仍使用原域名。未写 Windows DNS、设备全局 DNS 或固定 FRP 节点。

## 工具与发布范围

- tools/build-cam.ps1：主固件只编译；tools/build-cam-probe.ps1：独立诊断只编译。
- diagnostics/CamProbe：独立彩条/场景/CRC/被动 SCCB 诊断，临时替代 app，需备份及恢复主固件。
- tools/capture-cam-crc.py：设备采集或离线报告重验，输出原始证据；tools/test-cam-crc-offline.py 为其离线负例测试。
- tools/convert-video-mp4.py：电脑 AVI→MP4，不覆盖原件。
- tools/test-v047-offline.py、test-v0411-ui.py、test-v0412-storage.py、test-v0413-tls-retry.py：当前源码函数/头文件的主机测试。

本地还有特定开发机的刷写计划、历史固件校验、凭据导入、实时设备控制与截图回归工具。它们依赖原机目录、私有回退和设备状态，未作为通用安装工具发布；其测试结论在验证摘要中区分。完整 Flash、原始日志、音视频、个人对话以及原始交接全文均不发布。
