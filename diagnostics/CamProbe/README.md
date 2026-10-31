# Independent camera diagnostic

Temporary replacement app for the custom CAM board. Does not use NVS, TF, Wi-Fi, I2S microphone or display. Review its dedicated cam_board.h against the actual board before building; this diagnostic copy is historical and not a drop-in hardware detector.

From repository root, `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/build-cam-probe.ps1` only compiles the diagnostic with passive SCCB linker wrappers. It does not flash. Default console is native USB CDC; `-UseUartSerial` selects UART. Before any flash, identify device, inspect actual partition/security state and back up the current app/config privately. Restore the matching main app after diagnosis. No generic automatic flash command is supplied.

At 115200 baud, crc-p20 / crc-p10 / crc-d20 / crc-d10 compare PSRAM or DRAM at 20 or 10 MHz. crc-scene-p20 and crc-bars-p20 form a scene/color-bar pair. See source for legacy probe/bar commands; those legacy paths may alter GPIO weak-pull setup and must not be mixed with a single-variable experiment.

Capture via `python tools/capture-cam-crc.py --port <current_port> --command crc-scene-p20 --output validation/new-empty-directory`. Requires pyserial, Pillow and FFmpeg on PATH. The output directory must be new. CRC/strict decode success is not a visual quality acceptance; inspect scene and sensor transaction errors separately. `--offline-report` re-evaluates an existing report without opening a serial port.

Historical CRC results and their limits are summarized in ../../docs/DEVELOPMENT.md; no old real-world captures are included.
