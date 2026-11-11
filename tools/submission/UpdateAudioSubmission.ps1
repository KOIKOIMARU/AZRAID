[CmdletBinding()]
param([Parameter(Mandatory)][string]$SubmissionRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$stage = Join-Path $workspace 'generated\kurihaku_20261001'
$root = (Resolve-Path -LiteralPath $SubmissionRoot).Path
$game = Join-Path $root '実行ファイル（SKYBREAK）'
$manifestPath = Join-Path $stage 'manifest.csv'
$previous = @{}
foreach ($row in Import-Csv -LiteralPath $manifestPath) { $previous[$row.File] = $row.SHA256 }
$copies = [Collections.Generic.List[object]]::new()
foreach ($relative in @('include\engine\audio\SoundManager.h', 'src\engine\audio\SoundManager.cpp',
    'src\app\GameRuntime.cpp', 'include\app\GameRuntime.h',
    'src\engine\base\DirectXCommon.cpp', 'include\engine\base\DirectXCommon.h',
    'src\engine\3d\Object3d.cpp', 'include\engine\3d\Object3d.h',
    'include\engine\3d\Camera.h', 'src\engine\scene\TitleScene.cpp',
    'tools\TestRuntimeShortcuts.ps1', 'tools\GenerateCombatSounds.py')) {
    $sourceRelative = switch ($relative) {
        'src\app\GameRuntime.cpp' { 'src\app\runtime\GameRuntime.cpp' }
        'include\app\GameRuntime.h' { 'include\app\runtime\GameRuntime.h' }
        default { $relative }
    }
    $copies.Add(@{ Source = Join-Path $workspace "project\$sourceRelative"; Relative = $relative })
}
$combat = Join-Path $workspace 'project\resources\audio\combat'
foreach ($file in Get-ChildItem -LiteralPath $combat -Recurse -File) {
    $copies.Add(@{ Source = $file.FullName; Relative = 'resources\audio\combat\' + [IO.Path]::GetRelativePath($combat, $file.FullName) })
}
$copies.Add(@{ Source = Join-Path $workspace 'generated\outputs\Release\CG2.exe'; Relative = 'SKYBREAK.exe' })
$copies.Add(@{ Source = Join-Path $PSScriptRoot 'ThirdParty.txt'; Relative = 'licenses\ThirdParty.txt' })

# 前回まとめた後にユーザーが変更したファイルがあれば上書きしない。
foreach ($item in $copies) {
    $target = Join-Path $game $item.Relative
    if (Test-Path -LiteralPath $target) {
        $key = '実行ファイル（SKYBREAK）\' + $item.Relative
        if (-not $previous.ContainsKey($key) -or (Get-FileHash -LiteralPath $target).Hash -ne $previous[$key]) {
            throw "Submission file was modified outside this task: $target"
        }
    }
}
$backup = Join-Path $workspace ('generated\audio-before-20260927\submission-' + (Get-Date -Format 'HHmmss'))
foreach ($item in $copies) {
    $target = Join-Path $game $item.Relative
    if (Test-Path -LiteralPath $target) {
        $saved = Join-Path $backup $item.Relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
        Copy-Item -LiteralPath $target -Destination $saved
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Copy-Item -LiteralPath $item.Source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $item.Source).Hash) {
        throw "Copy verification failed: $target"
    }
}
$manifest = foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File) {
    [pscustomobject]@{ File = [IO.Path]::GetRelativePath($root, $file.FullName); Bytes = $file.Length;
        SHA256 = (Get-FileHash -LiteralPath $file.FullName).Hash }
}
$manifest | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding utf8NoBOM
Write-Output "AUDIO_SUBMISSION_UPDATED files=$($copies.Count) root=$root backup=$backup"
