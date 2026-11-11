[CmdletBinding()]
param(
    [switch]$Tutorial,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [string]$SubmissionRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$stage = Join-Path $workspace 'generated\kurihaku_20261001'
$game = Join-Path $stage '小泉羚（日本工学院専門学校_ゲームクリエイター科）\実行ファイル（SKYBREAK）'
if ($SubmissionRoot) { $game = Join-Path (Resolve-Path -LiteralPath $SubmissionRoot).Path '実行ファイル（SKYBREAK）' }
$label = if ($Tutorial) { 'tutorial' } else { 'gameplay' }
if ($Configuration -eq 'Debug') { $label += '-debug' }
$executable = if ($Configuration -eq 'Debug') {
    Join-Path (Split-Path -Parent $game) 'generated\outputs\Debug\CG2.exe'
} else { Join-Path $game 'SKYBREAK.exe' }
$log = Join-Path $stage "$label-smoke.log"
$diagnostic = Join-Path $stage "$label-d3d12.log"
$arguments = "--smoke-test 25 --smoke-timeout 120 --smoke-log `"$log`""
if ($Tutorial) { $arguments += ' --smoke-tutorial' }
$previousDiagnostic = $env:CG2_D3D12_DIAGNOSTIC_LOG
try {
    $env:CG2_D3D12_DIAGNOSTIC_LOG = $diagnostic
    $process = Start-Process -FilePath $executable `
        -WorkingDirectory $game -ArgumentList $arguments -PassThru
} finally {
    $env:CG2_D3D12_DIAGNOSTIC_LOG = $previousDiagnostic
}
Write-Output "STARTED pid=$($process.Id) mode=$label"
$deadline = (Get-Date).AddSeconds(175)
while (-not $process.HasExited) {
    if ((Get-Date) -gt $deadline) {
        Stop-Process -Id $process.Id -Force
        throw 'Packaged game smoke test timed out.'
    }
    Start-Sleep -Milliseconds 250
    $process.Refresh()
}
$process.WaitForExit()
$content = Get-Content -LiteralPath $log -Raw -Encoding UTF8
$graphics = Get-Content -LiteralPath $diagnostic -Raw -Encoding UTF8
if ($process.ExitCode -ne 0 -or $content -notmatch 'SMOKE_TEST_PASS') {
    Write-Output $content
    throw "Packaged game failed: exit=$($process.ExitCode)"
}
if ($graphics -match 'D3D12_MESSAGE severity=(CORRUPTION|ERROR|WARNING)|DEVICE_REMOVED|DEVICE_LOST') {
    Write-Output $graphics
    throw 'DirectX diagnostics reported a problem.'
}
if ($Configuration -eq 'Debug' -and ($graphics -notmatch 'D3D12_DEBUG_LAYER enabled=1' -or
    $graphics -notmatch 'D3D12_INFO_QUEUE enabled=1' -or $graphics -notmatch 'DRED_SETTINGS_ENABLED')) {
    throw 'Debug graphics validation was not fully enabled.'
}
Write-Output $content
Write-Output "PACKAGED_SMOKE_OK mode=$label seconds=25 exit=$($process.ExitCode)"
