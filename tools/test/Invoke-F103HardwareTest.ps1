[CmdletBinding()]
param(
  [string]$PortName,

  [ValidateRange(1, 60)]
  [int]$TimeoutSeconds = 8,

  [switch]$SkipResetTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Find-Ch343Port {
  if (-not [string]::IsNullOrWhiteSpace($PortName)) {
    return $PortName
  }

  $device = Get-CimInstance Win32_PnPEntity |
    Where-Object {
      $_.Name -match "CH343.*\((COM\d+)\)"
    } |
    Select-Object -First 1
  if ($null -eq $device) {
    throw "CH343 serial port not found. Pass -PortName explicitly."
  }

  $null = $device.Name -match "\((COM\d+)\)"
  return $Matches[1]
}

function Read-Until {
  param(
    [System.IO.Ports.SerialPort]$Port,
    [string]$Pattern,
    [int]$TimeoutMs
  )

  $text = ""
  $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
  while ([DateTime]::UtcNow -lt $deadline) {
    $text += $Port.ReadExisting()
    if ($text -match $Pattern) {
      return $text
    }
    Start-Sleep -Milliseconds 25
  }
  return $text
}

function Send-Command {
  param(
    [System.IO.Ports.SerialPort]$Port,
    [string]$Command,
    [string]$ExpectedPattern
  )

  $Port.DiscardInBuffer()
  $Port.Write("$Command`r`n")
  return Read-Until `
    -Port $Port `
    -Pattern $ExpectedPattern `
    -TimeoutMs ($TimeoutSeconds * 1000)
}

$resolvedPort = Find-Ch343Port
$serial = [System.IO.Ports.SerialPort]::new(
  $resolvedPort,
  115200,
  [System.IO.Ports.Parity]::None,
  8,
  [System.IO.Ports.StopBits]::One
)
$serial.ReadTimeout = 250

$failures = New-Object System.Collections.Generic.List[string]

try {
  $serial.Open()
  Start-Sleep -Milliseconds 250
  $serial.DiscardInBuffer()

  Write-Host "Port: $resolvedPort"

  $ping = Send-Command -Port $serial -Command "PING" -ExpectedPattern "PONG"
  if ($ping -match "PONG") {
    Write-Host "USART2 PING: PASS"
  }
  else {
    $failures.Add("USART2 PING did not return PONG.") | Out-Null
    Write-Host "USART2 PING: FAIL"
  }

  $serial.DiscardInBuffer()
  $serial.Write("RX_DMA`r`n")
  $dma_ready = Read-Until `
    -Port $serial `
    -Pattern "DMA READY" `
    -TimeoutMs ($TimeoutSeconds * 1000)
  if ($dma_ready -match "DMA READY") {
    $serial.Write("12345678")
    $dma_result = Read-Until `
      -Port $serial `
      -Pattern "RX_DMA (PASS|FAIL)" `
      -TimeoutMs ($TimeoutSeconds * 1000)
    if ($dma_result -match "RX_DMA PASS") {
      Write-Host "USART2 RX DMA: PASS"
    }
    else {
      $failures.Add("USART2 RX DMA did not return the expected payload.") |
        Out-Null
      Write-Host "USART2 RX DMA: FAIL"
    }
  }
  else {
    $failures.Add("USART2 RX DMA did not return DMA READY.") | Out-Null
    Write-Host "USART2 RX DMA: FAIL"
  }

  $test = Send-Command `
    -Port $serial `
    -Command "TEST" `
    -ExpectedPattern "TEST pass=\d+ fail=\d+ skip=\d+"
  if ($test -match "TEST pass=(\d+) fail=(\d+) skip=(\d+)") {
    $passed = [int]$Matches[1]
    $failed = [int]$Matches[2]
    $skipped = [int]$Matches[3]
    if ($failed -eq 0) {
      Write-Host "Hardware suite: PASS pass=$passed fail=$failed skip=$skipped"
    }
    else {
      $failures.Add("Hardware suite reported fail=$failed.") | Out-Null
      Write-Host "Hardware suite: FAIL fail=$failed"
    }
  }
  else {
    $failures.Add("Hardware suite summary was not received.") | Out-Null
    Write-Host "Hardware suite: FAIL (no summary)"
  }

  if (-not $SkipResetTests) {
    $serial.DiscardInBuffer()
    $serial.Write("IWDG`r`n")
    $iwdg = Read-Until `
      -Port $serial `
      -Pattern "F103 READY" `
      -TimeoutMs ($TimeoutSeconds * 1000)
    if ($iwdg -match "F103 READY iwdg=1 wwdg=0") {
      Write-Host "IWDG reset: PASS"
    }
    else {
      $failures.Add("IWDG reset flag was not observed.") | Out-Null
      Write-Host "IWDG reset: FAIL"
    }

    Start-Sleep -Milliseconds 250
    $serial.DiscardInBuffer()
    $serial.Write("WWDG`r`n")
    $wwdg = Read-Until `
      -Port $serial `
      -Pattern "F103 READY" `
      -TimeoutMs ($TimeoutSeconds * 1000)
    if ($wwdg -match "F103 READY iwdg=0 wwdg=1") {
      Write-Host "WWDG reset: PASS"
    }
    else {
      $failures.Add("WWDG reset flag was not observed.") | Out-Null
      Write-Host "WWDG reset: FAIL"
    }
  }
}
finally {
  if ($serial.IsOpen) {
    $serial.Close()
  }
  $serial.Dispose()
}

if ($failures.Count -ne 0) {
  foreach ($failure in $failures) {
    Write-Host "FAIL: $failure"
  }
  exit 1
}

Write-Host "RESULT: PASS"
