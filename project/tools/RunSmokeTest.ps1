[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateRange(1.0, 600.0)]
    [double]$GameplaySeconds = 15.0,

    [ValidateRange(10.0, 600.0)]
    [double]$StartupTimeoutSeconds = 120.0,

    [switch]$SkipBuild,

    [switch]$Tutorial,
    [switch]$TutorialPreview,
    [switch]$Playthrough,
    [switch]$Phantom,
    [switch]$PhantomPreview,
    [switch]$ChargeShot,
    [switch]$ChargePreview,
    [switch]$Rift,
    [switch]$RiftPreview,
    [switch]$Boss,
    [switch]$BossPreview,

    [switch]$CaptureFrames,

    [switch]$CaptureFromGameplay,

    [ValidateRange(1, 120)]
    [int]$CaptureFrameCount = 12,

    [ValidateRange(0.0, 600.0)]
    [double]$CaptureStartSeconds = 6.0,

    [ValidateRange(0.1, 10.0)]
    [double]$CaptureIntervalSeconds = 0.35,

    [ValidateSet('Opening', 'Wave2', 'Wave3', 'Boss', 'SniperWing')]
    [string]$StartPhase = 'Opening'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-LogIfPresent {
    param([string]$Path)

    if (Test-Path -LiteralPath $Path) {
        Write-Output (Get-Content -LiteralPath $Path -Raw -Encoding UTF8)
    }
}

$workspaceRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '..\..'))
$projectPath = Join-Path $workspaceRoot 'project\CG2.vcxproj'
$projectDirectory = Join-Path $workspaceRoot 'project'
$executablePath = Join-Path $workspaceRoot (
    'generated\outputs\{0}\CG2.exe' -f $Configuration)
$resultDirectory = Join-Path $workspaceRoot 'generated\smoke-tests'
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$smokeLogPath = Join-Path $resultDirectory (
    '{0}-{1}.log' -f $Configuration.ToLowerInvariant(), $timestamp)
$diagnosticLogPath = Join-Path $resultDirectory (
    '{0}-{1}-d3d12.log' -f $Configuration.ToLowerInvariant(), $timestamp)
$buildLogPath = Join-Path $resultDirectory (
    '{0}-{1}-build.log' -f $Configuration.ToLowerInvariant(), $timestamp)
$captureDirectory = Join-Path $resultDirectory (
    '{0}-{1}-frames' -f $Configuration.ToLowerInvariant(), $timestamp)

New-Item -ItemType Directory -Path $resultDirectory -Force | Out-Null

if ($CaptureFrames) {
    New-Item -ItemType Directory -Path $captureDirectory -Force | Out-Null
    Add-Type -AssemblyName System.Drawing
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CG2SmokeCaptureNative {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr window, ref POINT point);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    public static IntPtr FindMainWindow(int processId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            uint windowProcessId;
            GetWindowThreadProcessId(window, out windowProcessId);
            if (windowProcessId == processId && IsWindowVisible(window)) {
                result = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@
}

if (-not $SkipBuild) {
    $msbuildPath =
        'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'
    if (-not (Test-Path -LiteralPath $msbuildPath)) {
        throw "MSBuild was not found: $msbuildPath"
    }

    $buildOutput = & $msbuildPath $projectPath `
        "/p:Configuration=$Configuration" `
        '/p:Platform=x64' `
        '/m' `
        '/nologo' `
        '/v:minimal' 2>&1
    $buildExitCode = $LASTEXITCODE
    $buildOutput | Set-Content -LiteralPath $buildLogPath -Encoding UTF8
    if ($buildExitCode -ne 0) {
        $buildOutput
        throw "Build failed with exit code $buildExitCode. Log: $buildLogPath"
    }
}

if (-not (Test-Path -LiteralPath $executablePath)) {
    throw "Game executable was not found: $executablePath"
}

$gameplayText = $GameplaySeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture)
$startupTimeoutText = $StartupTimeoutSeconds.ToString(
    [System.Globalization.CultureInfo]::InvariantCulture)
$arguments =
    "--smoke-test $gameplayText " +
    "--smoke-timeout $startupTimeoutText " +
    "--smoke-log `"$smokeLogPath`""
if ($TutorialPreview) {
    if ($Configuration -ne 'Debug' -or $StartPhase -ne 'Opening') {
        throw 'TutorialPreview requires Debug and Opening.'
    }
    $Tutorial = $true
    $Playthrough = $true
    $arguments += ' --smoke-tutorial-preview'
}
if ($Tutorial) {
    if ($StartPhase -ne 'Opening') {
        throw 'Tutorial cannot be combined with a main-game StartPhase.'
    }
    $arguments += ' --smoke-tutorial'
}
if ($Playthrough) {
    if ($Configuration -ne 'Debug' -or $StartPhase -ne 'Opening') {
        throw 'Playthrough requires Debug and Opening.'
    }
    $arguments += ' --smoke-playthrough'
}
if ($Phantom -or $PhantomPreview) {
    if ($Configuration -ne 'Debug' -or $Tutorial -or $StartPhase -ne 'Opening' -or $Playthrough) {
        throw 'Phantom requires Debug, main game, Opening, and cannot combine with Playthrough.'
    }
    $arguments += $(if ($PhantomPreview) { ' --smoke-phantom-preview' } else { ' --smoke-phantom' })
}

$diagnosticEnvironmentName = 'CG2_D3D12_DIAGNOSTIC_LOG'
if ($ChargeShot -or $ChargePreview) {
    if ($Configuration -ne 'Debug' -or $Tutorial -or $StartPhase -ne 'Opening' -or $Playthrough -or $Phantom -or $PhantomPreview) {
        throw 'ChargeShot requires Debug, main game, Opening, and cannot combine with other probes.'
    }
    $arguments += $(if ($ChargePreview) { ' --smoke-charge-preview' } else { ' --smoke-charge' })
}

if ($Boss -or $BossPreview) {
    if ($Configuration -ne 'Debug' -or $Tutorial -or $StartPhase -ne 'Opening' -or $Playthrough -or $Phantom -or $PhantomPreview -or $ChargeShot -or $ChargePreview) {
        throw 'Boss requires Debug, main game, Opening, and cannot combine with other probes.'
    }
    $arguments += $(if ($BossPreview) { ' --smoke-boss-preview' } else { ' --smoke-boss' })
}

if ($Rift -or $RiftPreview) {
    if ($Configuration -ne 'Debug' -or $Tutorial -or $StartPhase -ne 'Opening' -or $Playthrough -or $Phantom -or $PhantomPreview -or $ChargeShot -or $ChargePreview -or $Boss -or $BossPreview) {
        throw 'Rift requires Debug, main game, Opening, and cannot combine with other probes.'
    }
    $arguments += $(if ($RiftPreview) { ' --smoke-rift-preview' } else { ' --smoke-rift' })
}

$startPhaseEnvironmentName = 'CG2_DEBUG_START_PHASE'
$previousDiagnosticLog = [Environment]::GetEnvironmentVariable(
    $diagnosticEnvironmentName,
    [EnvironmentVariableTarget]::Process)
$previousStartPhase = [Environment]::GetEnvironmentVariable(
    $startPhaseEnvironmentName,
    [EnvironmentVariableTarget]::Process)
try {
    [Environment]::SetEnvironmentVariable(
        $diagnosticEnvironmentName,
        $diagnosticLogPath,
        [EnvironmentVariableTarget]::Process)
    [Environment]::SetEnvironmentVariable(
        $startPhaseEnvironmentName,
        $StartPhase,
        [EnvironmentVariableTarget]::Process)
    $process = Start-Process `
        -FilePath $executablePath `
        -WorkingDirectory $projectDirectory `
        -ArgumentList $arguments `
        -PassThru
} finally {
    [Environment]::SetEnvironmentVariable(
        $diagnosticEnvironmentName,
        $previousDiagnosticLog,
        [EnvironmentVariableTarget]::Process)
    [Environment]::SetEnvironmentVariable(
        $startPhaseEnvironmentName,
        $previousStartPhase,
        [EnvironmentVariableTarget]::Process)
}

$hardTimeoutSeconds =
    [Math]::Ceiling($StartupTimeoutSeconds + $GameplaySeconds + 45.0)
$deadline = (Get-Date).AddSeconds($hardTimeoutSeconds)
$processStartTime = Get-Date
$gameplayCaptureStartTime = $null
$nextCaptureSeconds = $CaptureStartSeconds
$capturedFrameCount = 0

while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 250
    $process.Refresh()
    if ($CaptureFrames -and
        $CaptureFromGameplay -and
        $null -eq $gameplayCaptureStartTime -and
        (Test-Path -LiteralPath $smokeLogPath)) {
        $currentSmokeLog = Get-Content -LiteralPath $smokeLogPath -Raw
        if ($currentSmokeLog -match
            '\[(?<seconds>[0-9.]+)s\] SMOKE_TEST_GAMEPLAY_ENTERED') {
            $gameplayEnteredSeconds = [double]::Parse(
                $Matches.seconds,
                [System.Globalization.CultureInfo]::InvariantCulture)
            $gameplayCaptureStartTime =
                $processStartTime.AddSeconds($gameplayEnteredSeconds)
        }
    }
    $captureElapsedSeconds =
        if ($CaptureFromGameplay) {
            if ($null -eq $gameplayCaptureStartTime) {
                -1.0
            } else {
                ((Get-Date) - $gameplayCaptureStartTime).TotalSeconds
            }
        } else {
            ((Get-Date) - $processStartTime).TotalSeconds
        }
    $windowHandle = if ($CaptureFrames) {
        [CG2SmokeCaptureNative]::FindMainWindow($process.Id)
    } else {
        [IntPtr]::Zero
    }
    if ($CaptureFrames -and
        $capturedFrameCount -lt $CaptureFrameCount -and
        $captureElapsedSeconds -ge $nextCaptureSeconds -and
        $windowHandle -ne [IntPtr]::Zero) {
        [CG2SmokeCaptureNative]::SetForegroundWindow(
            $windowHandle) | Out-Null
        $rect = New-Object CG2SmokeCaptureNative+RECT
        if ([CG2SmokeCaptureNative]::GetClientRect(
                $windowHandle,
                [ref]$rect)) {
            $origin = New-Object CG2SmokeCaptureNative+POINT
            $origin.X = 0
            $origin.Y = 0
            [CG2SmokeCaptureNative]::ClientToScreen(
                $windowHandle,
                [ref]$origin) | Out-Null
            $width = $rect.Right - $rect.Left
            $height = $rect.Bottom - $rect.Top
            if ($width -gt 0 -and $height -gt 0) {
                $bitmap = New-Object System.Drawing.Bitmap $width, $height
                $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
                try {
                    $graphics.CopyFromScreen(
                        $origin.X,
                        $origin.Y,
                        0,
                        0,
                        $bitmap.Size)
                } finally {
                    $graphics.Dispose()
                }
                $capturePath = Join-Path $captureDirectory (
                    'frame-{0:D3}.png' -f $capturedFrameCount)
                $bitmap.Save(
                    $capturePath,
                    [System.Drawing.Imaging.ImageFormat]::Png)
                $bitmap.Dispose()
                ++$capturedFrameCount
            }
        }
        $nextCaptureSeconds += $CaptureIntervalSeconds
    }
}

if (-not $process.HasExited) {
    Stop-Process -Id $process.Id -Force
    Write-LogIfPresent -Path $smokeLogPath
    Write-LogIfPresent -Path $diagnosticLogPath
    throw "Smoke test exceeded the hard timeout of $hardTimeoutSeconds seconds."
}

$process.Refresh()
if (-not (Test-Path -LiteralPath $smokeLogPath)) {
    Write-LogIfPresent -Path $diagnosticLogPath
    throw "Smoke test produced no application log. Exit code: $($process.ExitCode)"
}

$smokeLog = Get-Content -LiteralPath $smokeLogPath -Raw -Encoding UTF8
$diagnosticLog = if (Test-Path -LiteralPath $diagnosticLogPath) {
    Get-Content -LiteralPath $diagnosticLogPath -Raw -Encoding UTF8
} else {
    ''
}
if ($process.ExitCode -ne 0) {
    $smokeLog
    if ($diagnosticLog) {
        $diagnosticLog
    }
    throw "Smoke test failed with exit code $($process.ExitCode)."
}

if ($smokeLog -notmatch 'SMOKE_TEST_PASS') {
    $smokeLog
    throw 'Smoke test exited without a pass marker.'
}

if (-not $diagnosticLog) {
    throw 'Smoke test produced no DirectX 12 diagnostic log.'
}

if ($Configuration -eq 'Debug' -and
    ($diagnosticLog -notmatch 'DRED_SETTINGS_ENABLED' -or
     $diagnosticLog -notmatch 'D3D12_DEBUG_LAYER enabled=1' -or
     $diagnosticLog -notmatch 'D3D12_INFO_QUEUE enabled=1')) {
    $diagnosticLog
    throw 'Debug DirectX 12 diagnostics were not fully enabled.'
}

if ($diagnosticLog -match
    'D3D12_MESSAGE severity=(CORRUPTION|ERROR|WARNING)') {
    $diagnosticLog
    throw 'DirectX 12 validation reported a warning or error.'
}

Write-Output (
    'SMOKE_TEST_OK configuration={0} gameplay_seconds={1} log={2} d3d12_log={3}' -f
    $Configuration,
    $gameplayText,
    $smokeLogPath,
    $diagnosticLogPath)
if ($CaptureFrames) {
    Write-Output (
        'SMOKE_TEST_FRAMES count={0} directory={1}' -f
        $capturedFrameCount,
        $captureDirectory)
}
Write-Output $smokeLog.TrimEnd()
Write-Output $diagnosticLog.TrimEnd()
