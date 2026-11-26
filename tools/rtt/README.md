# RTT 日志接收

先确认固件以 `SYS_LOG_USE_RTT=ON` 构建并已烧录（例如 `build/Debug/f407vet6.elf`），然后运行：

```powershell
$env:OPENOCD_EXE = "D:\path\to\openocd.exe"
.\tools\rtt\Receive-Rtt.ps1 -Board f407vet6
```

脚本会通过 ST-Link 查找 `SEGGER RTT` 控制块，在 `127.0.0.1:9090` 启动 RTT server，并把收到的字节直接输出到终端。按 `Ctrl+C` 结束接收。

常用参数：

```powershell
# 只接收 5 秒，适合快速检查
.\tools\rtt\Receive-Rtt.ps1 -Board f407vet6 -Seconds 5

# 使用其他端口
.\tools\rtt\Receive-Rtt.ps1 -Board f407vet6 -Port 9091

# 显式指定 OpenOCD
.\tools\rtt\Receive-Rtt.ps1 `
  -Board f407vet6 `
  -OpenOcdExe "D:\path\to\openocd.exe"
```

脚本仅启动 RTT，不烧录固件。脚本启动期间会短暂 halt 目标以查找 RTT 控制块，随后立即 resume。
