[CmdletBinding()]
param(
  [ValidateSet("f103c8t6", "f407vet6")]
  [string]$Board = "f103c8t6",

  [string]$OpenOcdExe,

  [ValidateRange(1, 65535)]
  [int]$Port = 9090,

  [ValidateRange(0, 86400)]
  [int]$Seconds = 0,

  [string]$RttAddress,
  [string]$RttSize
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-OpenOcd {
  param([string]$ExplicitPath)

  if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
    return (Resolve-Path -LiteralPath $ExplicitPath).Path
  }

  if (-not [string]::IsNullOrWhiteSpace($env:OPENOCD_EXE)) {
    return (Resolve-Path -LiteralPath $env:OPENOCD_EXE).Path
  }

  $command = Get-Command "openocd.exe" -ErrorAction SilentlyContinue
  if ($null -ne $command) {
    return $command.Source
  }

  throw "openocd.exe not found. Pass -OpenOcdExe or set OPENOCD_EXE."
}

$boardConfig = @{
  f103c8t6 = @{
    Target = "stm32f1x"
    Address = "0x20000000"
    Size = "0x5000"
  }
  f407vet6 = @{
    Target = "stm32f4x"
    Address = "0x20000000"
    Size = "0x20000"
  }
}

$config = $boardConfig[$Board]
$address = if ([string]::IsNullOrWhiteSpace($RttAddress)) {
  $config.Address
} else {
  $RttAddress
}
$size = if ([string]::IsNullOrWhiteSpace($RttSize)) {
  $config.Size
} else {
  $RttSize
}

$openOcd = Resolve-OpenOcd -ExplicitPath $OpenOcdExe
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) (
  "layered-rtt-{0}-{1}" -f $PID, [guid]::NewGuid().ToString("N").Substring(0, 8)
)
$configPath = "$tempRoot.cfg"
$stdoutPath = "$tempRoot.stdout.log"
$stderrPath = "$tempRoot.stderr.log"

$configLines = @(
  "source [find interface/stlink.cfg]",
  "source [find target/$($config.Target).cfg]",
  "gdb_port disabled",
  "telnet_port disabled",
  "tcl_port disabled",
  "init",
  "halt",
  ('rtt setup {0} {1} "SEGGER RTT"' -f $address, $size),
  "rtt start",
  ("rtt server start {0} 0" -f $Port),
  "resume"
)
[IO.File]::WriteAllLines($configPath, $configLines)

$process = $null
$client = $null

try {
  $process = Start-Process `
    -FilePath $openOcd `
    -ArgumentList @("-f", ('"' + $configPath + '"')) `
    -WindowStyle Hidden `
    -PassThru `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath

  $connectDeadline = [DateTime]::UtcNow.AddSeconds(10)
  while ([DateTime]::UtcNow -lt $connectDeadline) {
    if ($process.HasExited) {
      $details = [IO.File]::ReadAllText($stdoutPath) +
        [Environment]::NewLine +
        [IO.File]::ReadAllText($stderrPath)
      throw "OpenOCD exited before RTT connected.`n$details"
    }

    try {
      $client = [System.Net.Sockets.TcpClient]::new()
      $client.Connect("127.0.0.1", $Port)
      $client.NoDelay = $true
      break
    }
    catch {
      if ($null -ne $client) {
        $client.Dispose()
        $client = $null
      }
      Start-Sleep -Milliseconds 100
    }
  }

  if ($null -eq $client -or -not $client.Connected) {
    throw "RTT server did not accept a connection on 127.0.0.1:$Port."
  }

  Write-Host "RTT connected: $Board, 127.0.0.1:$Port"

  $stream = $client.GetStream()
  $buffer = New-Object byte[] 1024
  $charBuffer = New-Object char[] 2048
  $decoder = [Text.UTF8Encoding]::new($false).GetDecoder()
  $deadline = if ($Seconds -gt 0) {
    [DateTime]::UtcNow.AddSeconds($Seconds)
  } else {
    [DateTime]::MaxValue
  }

  while ([DateTime]::UtcNow -lt $deadline) {
    if ($stream.DataAvailable) {
      $count = $stream.Read($buffer, 0, $buffer.Length)
      if ($count -gt 0) {
        $charCount = $decoder.GetChars($buffer, 0, $count, $charBuffer, 0)
        [Console]::Out.Write($charBuffer, 0, $charCount)
        [Console]::Out.Flush()
      }
    }

    Start-Sleep -Milliseconds 50
  }
}
finally {
  if ($null -ne $client) {
    $client.Dispose()
  }

  if ($null -ne $process -and -not $process.HasExited) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
  }

  Remove-Item -LiteralPath $configPath -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $stdoutPath -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $stderrPath -Force -ErrorAction SilentlyContinue
}
