[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('off', 'radial', 'camera', 'vectors')]
    [string]$Mode = 'camera'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$taskExe = Join-Path $taskRoot "generated\outputs\$Configuration\CG2.exe"
if (-not (Test-Path -LiteralPath $taskExe)) { throw "Build $Configuration before launching this review." }
$taskPreviousReview = $env:AZRAID_BLUR_REVIEW
$taskPreviousMode = $env:AZRAID_FLIGHT_BLUR
try {
    $env:AZRAID_BLUR_REVIEW = '1'
    $env:AZRAID_FLIGHT_BLUR = $Mode
    Start-Process -FilePath $taskExe -WorkingDirectory (Join-Path $taskRoot 'project') -PassThru
} finally {
    $env:AZRAID_BLUR_REVIEW = $taskPreviousReview
    $env:AZRAID_FLIGHT_BLUR = $taskPreviousMode
}
Write-Host 'F6: Off / Radial / Camera / Vectors. F7: freeze the same frame for comparison.'
