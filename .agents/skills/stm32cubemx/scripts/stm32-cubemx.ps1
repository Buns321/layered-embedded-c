[CmdletBinding()]
param(
  [Parameter(Mandatory = $true, Position = 0)]
  [ValidateSet("generate", "flash")]
  [string]$Action,

  [Parameter(Mandatory = $true)]
  [string]$Ioc,

  [string]$Elf,
  [string]$CubeMxExe,
  [string]$OpenOcdExe,

  [switch]$AcceptRtosSystickWarning,
  [switch]$AllowStaleElf,
  [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step {
  param([string]$Message)
  Write-Host "==> $Message"
}

function Write-WarningLine {
  param([string]$Message)
  Write-Host "WARN: $Message" -ForegroundColor Yellow
}

function Get-RequiredPath {
  param([string]$Path, [string]$Label)

  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    throw "$Label not found: $Path"
  }

  return (Resolve-Path -LiteralPath $Path).Path
}

function Get-IocValue {
  param([string]$Path, [string]$Key)

  foreach ($line in [IO.File]::ReadAllLines($Path)) {
    $separator = $line.IndexOf("=")
    if ($separator -gt 0 -and $line.Substring(0, $separator) -eq $Key) {
      return $line.Substring($separator + 1).Trim()
    }
  }

  return $null
}

function Get-RepoRoot {
  param([string]$StartPath)

  $root = & git -C $StartPath rev-parse --show-toplevel 2>$null
  if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace(($root -join ""))) {
    throw "Unable to determine the Git repository root from: $StartPath"
  }

  return (($root | Select-Object -First 1).Trim() -replace "/", "\")
}

function Resolve-Executable {
  param(
    [string]$ExplicitPath,
    [string]$EnvironmentName,
    [string]$CommandName,
    [string[]]$Patterns
  )

  if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
    return Get-RequiredPath -Path $ExplicitPath -Label $CommandName
  }

  $environmentValue = [Environment]::GetEnvironmentVariable($EnvironmentName)
  if (-not [string]::IsNullOrWhiteSpace($environmentValue)) {
    return Get-RequiredPath -Path $environmentValue -Label "$CommandName from $EnvironmentName"
  }

  $command = Get-Command $CommandName -ErrorAction SilentlyContinue
  if ($null -ne $command) {
    return $command.Source
  }

  foreach ($pattern in $Patterns) {
    $match = Get-Item -Path $pattern -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTime -Descending |
      Select-Object -First 1
    if ($null -ne $match) {
      return $match.FullName
    }
  }

  throw "Unable to locate $CommandName. Pass its path or set $EnvironmentName."
}

function Find-CubeMx {
  param([string]$ExplicitPath)

  return Resolve-Executable `
    -ExplicitPath $ExplicitPath `
    -EnvironmentName "STM32CUBEMX_EXE" `
    -CommandName "STM32CubeMX.exe" `
    -Patterns @(
      "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeMX\STM32CubeMX.exe",
      "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeMX\STM32CubeMX.exe"
    )
}

function Find-OpenOcd {
  param([string]$ExplicitPath)

  return Resolve-Executable `
    -ExplicitPath $ExplicitPath `
    -EnvironmentName "OPENOCD_EXE" `
    -CommandName "openocd.exe" `
    -Patterns @(
      "C:\Program Files\OpenOCD*\bin\openocd.exe",
      "C:\Program Files (x86)\OpenOCD*\bin\openocd.exe"
    )
}

function New-RunDirectory {
  param([string]$RepoRoot, [string]$Name)

  $runsRoot = Join-Path $RepoRoot "build\stm32cubemx\runs"
  New-Item -ItemType Directory -Path $runsRoot -Force | Out-Null

  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  $runDirectory = Join-Path $runsRoot ("{0}-{1}-{2}" -f $stamp, $Name, [guid]::NewGuid().ToString("N").Substring(0, 8))
  New-Item -ItemType Directory -Path $runDirectory | Out-Null
  return $runDirectory
}

function Copy-GenerationInputs {
  param([string]$BoardRoot, [string]$StageRoot, [string]$IocName)

  $items = @("Core", "Drivers", "Middlewares", "cmake")
  foreach ($item in $items) {
    $source = Join-Path $BoardRoot $item
    if (Test-Path -LiteralPath $source -PathType Container) {
      Copy-Item -LiteralPath $source -Destination $StageRoot -Recurse
    }
  }

  $files = @($IocName, ".mxproject", "CMakeLists.txt", "CMakePresets.json")
  foreach ($name in $files) {
    $source = Join-Path $BoardRoot $name
    if (Test-Path -LiteralPath $source -PathType Leaf) {
      Copy-Item -LiteralPath $source -Destination $StageRoot
    }
  }

  Get-ChildItem -LiteralPath $BoardRoot -File |
    Where-Object { $_.Extension -eq ".ld" -or $_.Name -like "startup*.s" } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $StageRoot }
}

function Remove-SafeTree {
  param([string]$Path, [string]$AllowedRoot)

  $fullPath = [IO.Path]::GetFullPath($Path)
  $fullRoot = [IO.Path]::GetFullPath($AllowedRoot).TrimEnd("\") + "\"
  if (-not $fullPath.StartsWith($fullRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove a path outside $AllowedRoot`: $fullPath"
  }

  if (Test-Path -LiteralPath $fullPath) {
    Remove-Item -LiteralPath $fullPath -Recurse -Force
  }
}

function Initialize-DialogHelpers {
  if ("CodexStm32Native" -as [type]) {
    return
  }

  Add-Type -AssemblyName System.Drawing
  Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class CodexStm32Native
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr window, StringBuilder text, int maxLength);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr window, StringBuilder text, int maxLength);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr window, int command);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(
        IntPtr window,
        IntPtr insertAfter,
        int x,
        int y,
        int width,
        int height,
        uint flags);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(
        uint flags,
        uint dx,
        uint dy,
        uint data,
        UIntPtr extraInfo);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(
        IntPtr window,
        uint message,
        IntPtr wParam,
        IntPtr lParam);
}
"@
}

function Get-CubeMxWindows {
  param([int[]]$ProcessIds)

  Initialize-DialogHelpers
  $script:codexCubeMxWindows = @()
  [void][CodexStm32Native]::EnumWindows(
    {
      param($window, $parameter)

      $owner = [uint32]0
      [void][CodexStm32Native]::GetWindowThreadProcessId($window, [ref]$owner)
      if ($ProcessIds -contains [int]$owner) {
        $title = [Text.StringBuilder]::new(512)
        $class = [Text.StringBuilder]::new(128)
        [void][CodexStm32Native]::GetWindowText($window, $title, $title.Capacity)
        [void][CodexStm32Native]::GetClassName($window, $class, $class.Capacity)

        $script:codexCubeMxWindows += [pscustomobject]@{
          Handle = $window
          ProcessId = $owner
          Title = $title.ToString()
          Class = $class.ToString()
          Visible = [CodexStm32Native]::IsWindowVisible($window)
        }
      }

      return $true
    },
    [IntPtr]::Zero
  )

  return $script:codexCubeMxWindows
}

function Save-WindowScreenshot {
  param([IntPtr]$Handle, [string]$Path)

  Initialize-DialogHelpers
  $rectangle = New-Object CodexStm32Native+RECT
  [void][CodexStm32Native]::GetWindowRect($Handle, [ref]$rectangle)
  $width = $rectangle.Right - $rectangle.Left
  $height = $rectangle.Bottom - $rectangle.Top
  if ($width -le 0 -or $height -le 0) {
    return
  }

  $bitmap = New-Object Drawing.Bitmap $width, $height
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  $deviceContext = $graphics.GetHdc()
  [void][CodexStm32Native]::PrintWindow($Handle, $deviceContext, 2)
  $graphics.ReleaseHdc($deviceContext)
  $graphics.Dispose()
  $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
  $bitmap.Dispose()
}

function Approve-GenerationWarning {
  param([IntPtr]$Handle)

  Initialize-DialogHelpers
  $rectangle = New-Object CodexStm32Native+RECT
  [void][CodexStm32Native]::GetWindowRect($Handle, [ref]$rectangle)
  $width = $rectangle.Right - $rectangle.Left
  $height = $rectangle.Bottom - $rectangle.Top

  [void][CodexStm32Native]::ShowWindow($Handle, 5)
  [void][CodexStm32Native]::SetWindowPos(
    $Handle,
    [IntPtr](-1),
    $rectangle.Left,
    $rectangle.Top,
    $width,
    $height,
    0x0040)
  Start-Sleep -Milliseconds 250
  [void][CodexStm32Native]::SetForegroundWindow($Handle)
  [void][CodexStm32Native]::SetCursorPos(
    $rectangle.Left + [int]($width * 0.46),
    $rectangle.Top + [int]($height * 0.835))
  Start-Sleep -Milliseconds 200
  [CodexStm32Native]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 100
  [CodexStm32Native]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}

function Stop-StartedProcesses {
  param([int[]]$ProcessIds)

  foreach ($processId in $ProcessIds) {
    Stop-Process -Id $processId -Force -ErrorAction SilentlyContinue
  }
}

function Invoke-CubeMxGeneration {
  param(
    [string]$CubeMx,
    [string]$StageRoot,
    [string]$StagedIoc,
    [string]$RunDirectory,
    [bool]$AcceptWarning
  )

  $scriptPath = Join-Path $RunDirectory "cubemx-script.txt"
  $stdoutPath = Join-Path $RunDirectory "cubemx.stdout.log"
  $stderrPath = Join-Path $RunDirectory "cubemx.stderr.log"
  $scriptLines = @(
    ('config load "{0}"' -f $StagedIoc),
    "project generate",
    "exit"
  )
  [IO.File]::WriteAllLines($scriptPath, $scriptLines)

  $existingJavaIds = @(
    Get-Process -Name "javaw" -ErrorAction SilentlyContinue |
      Where-Object { $_.Path -like "*STM32CubeMX*" } |
      Select-Object -ExpandProperty Id
  )

  $launcher = Start-Process `
    -FilePath $CubeMx `
    -ArgumentList @("-s", ('"' + $scriptPath + '"')) `
    -PassThru `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath

  $deadline = [DateTime]::UtcNow.AddSeconds(180)
  $acceptedWarning = $false
  $seenJavaIds = @()
  $launcherExitTime = $null

  while ([DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 250
    $currentJavaIds = @(
      Get-Process -Name "javaw" -ErrorAction SilentlyContinue |
        Where-Object {
          $_.Path -like "*STM32CubeMX*" -and
          $existingJavaIds -notcontains $_.Id
        } |
        Select-Object -ExpandProperty Id
    )
    $seenJavaIds = @($seenJavaIds + $currentJavaIds | Sort-Object -Unique)

    $windows = Get-CubeMxWindows -ProcessIds $currentJavaIds
    foreach ($window in $windows) {
      if (-not $window.Visible) {
        continue
      }

      if ($window.Title -eq "Warning: Code Generation") {
        if (-not $AcceptWarning -or $acceptedWarning) {
          $screenshot = Join-Path $RunDirectory "cubemx-warning.png"
          Save-WindowScreenshot -Handle $window.Handle -Path $screenshot
          [void][CodexStm32Native]::PostMessage($window.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
          Start-Sleep -Milliseconds 500
          Stop-StartedProcesses -ProcessIds $seenJavaIds
          throw "CubeMX displayed an unapproved warning. Inspect $screenshot before retrying with -AcceptRtosSystickWarning."
        }

        Approve-GenerationWarning -Handle $window.Handle
        $acceptedWarning = $true
        Write-Step "Accepted the known FreeRTOS/SysTick generation warning."
        Start-Sleep -Milliseconds 750
      }
      elseif ($window.Class -eq "SunAwtDialog" -and -not [string]::IsNullOrWhiteSpace($window.Title)) {
        $screenshot = Join-Path $RunDirectory "cubemx-unexpected-dialog.png"
        Save-WindowScreenshot -Handle $window.Handle -Path $screenshot
        [void][CodexStm32Native]::PostMessage($window.Handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 500
        Stop-StartedProcesses -ProcessIds $seenJavaIds
        throw "CubeMX displayed an unexpected dialog '$($window.Title)'. Inspect $screenshot."
      }
    }

    $runningJavaIds = @(
      $seenJavaIds |
        Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue }
    )
    if ($launcher.HasExited -and $runningJavaIds.Count -eq 0) {
      if ($null -eq $launcherExitTime) {
        $launcherExitTime = [DateTime]::UtcNow
      }
      elseif (([DateTime]::UtcNow - $launcherExitTime).TotalSeconds -ge 2) {
        break
      }
    }
    else {
      $launcherExitTime = $null
    }
  }

  $remainingJavaIds = @(
    $seenJavaIds |
      Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue }
  )
  if (-not $launcher.HasExited -or $remainingJavaIds.Count -ne 0) {
    Stop-StartedProcesses -ProcessIds $remainingJavaIds
    throw "CubeMX generation timed out. Logs: $RunDirectory"
  }

  $stdout = [IO.File]::ReadAllText($stdoutPath)
  $stderr = [IO.File]::ReadAllText($stderrPath)
  if ($stderr -match "OutOfMemoryError") {
    throw "CubeMX ran out of memory. Logs: $RunDirectory"
  }
  if ($stdout -notmatch "Generated code:" -or $stdout -notmatch "Bye bye") {
    throw "CubeMX did not report successful code generation. Logs: $RunDirectory"
  }

  $generatedCore = @(Get-ChildItem -LiteralPath (Join-Path $StageRoot "Core") -Recurse -File -ErrorAction SilentlyContinue)
  if ($generatedCore.Count -eq 0) {
    throw "CubeMX produced no Core files. Logs: $RunDirectory"
  }

  $optionalErrors = @(
    [regex]::Matches($stdout, "OptionalMessage_ERROR[^\r\n]*") |
      ForEach-Object { $_.Value } |
      Select-Object -Unique
  )
  foreach ($message in $optionalErrors) {
    Write-WarningLine $message
  }

  return [pscustomobject]@{
    StdoutPath = $stdoutPath
    StderrPath = $stderrPath
  }
}

function Copy-FileIfChanged {
  param([string]$Source, [string]$Destination)

  if (Test-Path -LiteralPath $Destination -PathType Leaf) {
    $sourceHash = (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash
    $destinationHash = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash
    if ($sourceHash -eq $destinationHash) {
      return $false
    }
  }

  $parent = Split-Path -Parent $Destination
  New-Item -ItemType Directory -Path $parent -Force | Out-Null
  Copy-Item -LiteralPath $Source -Destination $Destination -Force
  return $true
}

function Sync-GeneratedFiles {
  param(
    [string]$StageRoot,
    [string]$BoardRoot,
    [string]$IocName
  )

  $changed = New-Object System.Collections.Generic.List[string]
  $stagePrefix = $StageRoot.TrimEnd("\") + "\"

  foreach ($file in Get-ChildItem -LiteralPath $StageRoot -Recurse -File) {
    if (-not $file.FullName.StartsWith($stagePrefix, [StringComparison]::OrdinalIgnoreCase)) {
      throw "Unexpected staged path: $($file.FullName)"
    }

    $relative = $file.FullName.Substring($stagePrefix.Length)
    $isAllowed = (
      $relative -eq $IocName -or
      $relative -eq ".mxproject" -or
      $relative.StartsWith("Core\", [StringComparison]::OrdinalIgnoreCase)
    )
    if (-not $isAllowed) {
      continue
    }

    $destination = Join-Path $BoardRoot $relative
    if ((Split-Path -Leaf $relative) -match "^stm32f[0-9]+xx_hal_conf\.h$") {
      if (Test-Path -LiteralPath $destination -PathType Leaf) {
        $stagedHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $workspaceHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        if ($stagedHash -ne $workspaceHash) {
          Write-WarningLine "Protected project file changed by CubeMX and was not synchronized: $relative"
        }
      }
      continue
    }

    if (Copy-FileIfChanged -Source $file.FullName -Destination $destination) {
      $changed.Add($relative) | Out-Null
    }
  }

  $cmakePath = Join-Path $BoardRoot "cmake\stm32cubemx\CMakeLists.txt"
  if (Test-Path -LiteralPath $cmakePath -PathType Leaf) {
    $cmakeText = [IO.File]::ReadAllText($cmakePath)
    foreach ($source in Get-ChildItem -LiteralPath (Join-Path $BoardRoot "Core\Src") -File -Filter "*.c" -ErrorAction SilentlyContinue) {
      if (-not $cmakeText.Contains($source.Name)) {
        Write-WarningLine "Core source '$($source.Name)' is not registered in cmake/stm32cubemx/CMakeLists.txt."
      }
    }
  }

  Write-Step "Skipped Drivers/Middlewares/cmake/linker/startup by design."
  return $changed
}

function Assert-ElfIsFresh {
  param(
    [string]$ElfPath,
    [string]$RepoRoot,
    [string]$BoardRoot,
    [string]$IocPath
  )

  $sourceRoots = @(
    (Join-Path $RepoRoot "App"),
    (Join-Path $RepoRoot "Controller"),
    (Join-Path $RepoRoot "DeviceDrivers"),
    (Join-Path $RepoRoot "Filter"),
    (Join-Path $RepoRoot "Policy"),
    (Join-Path $RepoRoot "RTOS"),
    (Join-Path $RepoRoot "System"),
    (Join-Path $RepoRoot "Platform"),
    (Join-Path $BoardRoot "Core"),
    (Join-Path $BoardRoot "cmake")
  )

  $extensions = @(".c", ".h", ".s", ".ld", ".txt", ".json", ".ioc", ".cmake")
  $sourceFiles = New-Object System.Collections.Generic.List[System.IO.FileInfo]

  foreach ($root in $sourceRoots) {
    if (Test-Path -LiteralPath $root -PathType Container) {
      Get-ChildItem -LiteralPath $root -Recurse -File |
        Where-Object { $extensions -contains $_.Extension.ToLowerInvariant() } |
        ForEach-Object { $sourceFiles.Add($_) | Out-Null }
    }
  }

  foreach ($file in @(
    $IocPath,
    (Join-Path $BoardRoot ".mxproject"),
    (Join-Path $BoardRoot "CMakeLists.txt"),
    (Join-Path $RepoRoot "CMakeLists.txt")
  )) {
    if (Test-Path -LiteralPath $file -PathType Leaf) {
      $sourceFiles.Add((Get-Item -LiteralPath $file)) | Out-Null
    }
  }

  $newest = $sourceFiles |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1
  $elfInfo = Get-Item -LiteralPath $ElfPath

  if ($null -ne $newest -and $newest.LastWriteTimeUtc -gt $elfInfo.LastWriteTimeUtc.AddSeconds(1)) {
    throw "ELF is older than $($newest.FullName). Build first or pass -AllowStaleElf after confirming the binary."
  }
}

function Invoke-Generate {
  $iocPath = Get-RequiredPath -Path $Ioc -Label "IOC file"
  if ([IO.Path]::GetExtension($iocPath) -ne ".ioc") {
    throw "Expected a .ioc file: $iocPath"
  }

  $boardRoot = Split-Path -Parent $iocPath
  $iocName = Split-Path -Leaf $iocPath
  $repoRoot = Get-RepoRoot -StartPath $boardRoot
  $cubeMx = Find-CubeMx -ExplicitPath $CubeMxExe
  $runDirectory = New-RunDirectory -RepoRoot $repoRoot -Name "generate"
  $stageRoot = Join-Path $runDirectory "stage"
  New-Item -ItemType Directory -Path $stageRoot | Out-Null

  Write-Step "Staging $iocName"
  Copy-GenerationInputs -BoardRoot $boardRoot -StageRoot $stageRoot -IocName $iocName
  $stagedIoc = Join-Path $stageRoot $iocName

  Write-Step "Running STM32CubeMX"
  $logs = Invoke-CubeMxGeneration `
    -CubeMx $cubeMx `
    -StageRoot $stageRoot `
    -StagedIoc $stagedIoc `
    -RunDirectory $runDirectory `
    -AcceptWarning ([bool]$AcceptRtosSystickWarning)

  Write-Step "Synchronizing generated files"
  $changed = @(Sync-GeneratedFiles -StageRoot $stageRoot -BoardRoot $boardRoot -IocName $iocName)
  if ($changed.Count -eq 0) {
    Write-Host "No generated file changes."
  }
  else {
    foreach ($file in $changed) {
      Write-Host "  $file"
    }
  }

  Remove-SafeTree -Path $stageRoot -AllowedRoot $runDirectory
  Write-Host "Generation logs: $runDirectory"
  Write-Host "CubeMX stdout: $($logs.StdoutPath)"
}

function Find-Elf {
  param(
    [string]$ExplicitElf,
    [string]$BoardRoot,
    [string]$ProjectName
  )

  if (-not [string]::IsNullOrWhiteSpace($ExplicitElf)) {
    return Get-RequiredPath -Path $ExplicitElf -Label "ELF file"
  }

  $preferred = Join-Path $BoardRoot "build\Debug\$ProjectName.elf"
  if (Test-Path -LiteralPath $preferred -PathType Leaf) {
    return (Resolve-Path -LiteralPath $preferred).Path
  }

  $buildRoot = Join-Path $BoardRoot "build"
  if (Test-Path -LiteralPath $buildRoot -PathType Container) {
    $candidate = Get-ChildItem -LiteralPath $buildRoot -Recurse -File -Filter "$ProjectName.elf" |
      Sort-Object LastWriteTimeUtc -Descending |
      Select-Object -First 1
    if ($null -ne $candidate) {
      return $candidate.FullName
    }
  }

  throw "Unable to find $ProjectName.elf. Pass -Elf explicitly."
}

function Invoke-Flash {
  $iocPath = Get-RequiredPath -Path $Ioc -Label "IOC file"
  if ([IO.Path]::GetExtension($iocPath) -ne ".ioc") {
    throw "Expected a .ioc file: $iocPath"
  }

  $boardRoot = Split-Path -Parent $iocPath
  $repoRoot = Get-RepoRoot -StartPath $boardRoot
  $projectFile = Get-IocValue -Path $iocPath -Key "ProjectManager.ProjectFileName"
  $projectName = if ([string]::IsNullOrWhiteSpace($projectFile)) {
    [IO.Path]::GetFileNameWithoutExtension($iocPath)
  }
  else {
    [IO.Path]::GetFileNameWithoutExtension($projectFile)
  }

  $family = Get-IocValue -Path $iocPath -Key "Mcu.Family"
  $target = switch ($family) {
    "STM32F1" { "stm32f1x" }
    "STM32F4" { "stm32f4x" }
    default { throw "Unsupported MCU family '$family' in $iocPath." }
  }

  $elfPath = Find-Elf -ExplicitElf $Elf -BoardRoot $boardRoot -ProjectName $projectName
  if (-not $AllowStaleElf) {
    Assert-ElfIsFresh -ElfPath $elfPath -RepoRoot $repoRoot -BoardRoot $boardRoot -IocPath $iocPath
  }

  $openOcd = Find-OpenOcd -ExplicitPath $OpenOcdExe
  $runDirectory = New-RunDirectory -RepoRoot $repoRoot -Name "flash"
  $configPath = Join-Path $runDirectory "openocd.cfg"
  $stdoutPath = Join-Path $runDirectory "openocd.stdout.log"
  $stderrPath = Join-Path $runDirectory "openocd.stderr.log"
  $openOcdElf = $elfPath -replace "\\", "/"
  $configLines = @(
    "source [find interface/stlink.cfg]",
    "source [find target/$target.cfg]",
    ('program "{0}" verify reset exit' -f $openOcdElf)
  )
  [IO.File]::WriteAllLines($configPath, $configLines)

  if ($DryRun) {
    Write-Step "Dry run only; no hardware access."
    Write-Host "OpenOCD: $openOcd"
    Write-Host "ELF: $elfPath"
    Write-Host "Config: $configPath"
    return
  }

  Write-Step "Flashing $elfPath"
  $process = Start-Process `
    -FilePath $openOcd `
    -ArgumentList @("-f", ('"' + $configPath + '"')) `
    -PassThru `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath

  if (-not $process.WaitForExit(60000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    throw "OpenOCD flash timed out. Logs: $runDirectory"
  }
  $process.WaitForExit()
  $process.Refresh()
  $exitCode = $process.ExitCode

  $stdout = [IO.File]::ReadAllText($stdoutPath)
  $stderr = [IO.File]::ReadAllText($stderrPath)
  $combined = $stdout + [Environment]::NewLine + $stderr
  if ($combined -notmatch "Verified OK") {
    throw "Flash or verification failed with OpenOCD exit code $exitCode. Logs: $runDirectory"
  }
  if ($null -ne $exitCode -and $exitCode -ne 0) {
    Write-WarningLine "OpenOCD returned exit code $exitCode after a verified flash."
  }

  Write-Host "Flash verified successfully."
  Write-Host "Flash logs: $runDirectory"
}

switch ($Action) {
  "generate" { Invoke-Generate }
  "flash" { Invoke-Flash }
}
