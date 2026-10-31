# 本地工具

从仓库根目录运行。所有 test-v*.py 都是主机离线测试，不会打开串口、录音或请求模型；输出到被忽略的 validation/，编译缓存位于 .build-host/。

## 编译器与依赖

需要 Python 3、Zig 0.14.1（已验证）或可用的 C++17 clang++/g++，以及 ArduinoJson 7.4.3 的 src 目录。编译器和库不打包，也不自动下载。PowerShell 示例：

```powershell
$env:ZIG_EXE = 'C:/path/to/zig.exe'
$env:ARDUINOJSON_INCLUDE = 'C:/path/to/ArduinoJson/src'
python tools/test-v047-offline.py
python tools/test-v0411-ui.py
python tools/test-v0412-storage.py
python tools/test-v0413-tls-retry.py
```

未提供 ZIG_EXE 时尝试 PATH 中的 zig，再尝试 CXX/clang++/g++。发布工具把原开发机硬编码路径改为环境变量；固件 .ino/.h 保持原样。测试依赖当前源码布局，重构函数时需同步维护提取边界。

- test-v047-offline.py：图片队列、删除、流式 Base64/上下文、长思考/think、公式、录音容量、DNS 解析等实际可移植代码。
- test-v0411-ui.py：实际动画/编辑分支和休眠范围。
- test-v0412-storage.py：实际存储事务/恢复、原卡保护、模型能力持久化/拦截。
- test-v0413-tls-retry.py：SDK stop 丢信任模拟及实际重试函数。

build-cam.ps1 / build-cam-probe.ps1 只编译，不自动烧录。capture-cam-crc.py 的实机模式会操作已指定串口，需先核对设备、备份并临时运行独立诊断固件；详见 diagnostics/CamProbe/README.md。离线模式不连设备。

相机采集依赖 pyserial、Pillow 和 FFmpeg；仅离线报告重验无需打开 serial。convert-video-mp4.py 需要 FFmpeg/ffprobe，保留源文件，拒绝覆盖目标。

历史镜像校验工具依赖开发机私有回退和原分区快照，不作为公共通用脚本发布；不要用公开仓库测试替代实际设备身份、分区和备份检查。

公开 CRC 离线测试仅使用生成样图和协议负例；若未提供原开发机历史坏图，会显式以 historical_bad_frames=0 记录，不将它说成重跑了历史 12 张。运行：`python tools/test-cam-crc-offline.py --output validation/new-empty-crc-test`。
