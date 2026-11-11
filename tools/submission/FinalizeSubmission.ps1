[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SubmissionRoot,
    [Parameter(Mandatory)][string]$FixedPrPath,
    [Parameter(Mandatory)][string]$ZipPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$root = (Resolve-Path -LiteralPath $SubmissionRoot).Path
$fixed = (Resolve-Path -LiteralPath $FixedPrPath).Path
$zip = [IO.Path]::GetFullPath($ZipPath)
if (Test-Path -LiteralPath $zip) { throw "Existing ZIP will not be overwritten: $zip" }
$videos = @(Get-ChildItem -LiteralPath $root -File -Filter '*.mp4')
$sheets = @(Get-ChildItem -LiteralPath $root -File -Filter '*.xlsx')
if ($videos.Count -ne 1 -or $sheets.Count -ne 1) { throw 'Expected exactly one video and one self-PR workbook.' }
$videoName = '作品実演動画（SKYBREAK）.mp4'
$sheetName = '自己PRシート（小泉羚）.xlsx'
foreach ($name in @($videoName, $sheetName)) {
    if (Test-Path -LiteralPath (Join-Path $root $name)) { throw "Destination already exists: $name" }
}
$backup = Join-Path $workspace ('generated\verification\submission-backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $sheets[0].FullName -Destination (Join-Path $backup $sheets[0].Name)
$videoHash = (Get-FileHash -LiteralPath $videos[0].FullName).Hash
Rename-Item -LiteralPath $videos[0].FullName -NewName $videoName
Rename-Item -LiteralPath $sheets[0].FullName -NewName $sheetName
Copy-Item -LiteralPath $fixed -Destination (Join-Path $root $sheetName) -Force
if ((Get-FileHash -LiteralPath (Join-Path $root $videoName)).Hash -ne $videoHash) { throw 'Video was unexpectedly modified.' }
if ((Get-FileHash -LiteralPath (Join-Path $root $sheetName)).Hash -ne (Get-FileHash -LiteralPath $fixed).Hash) { throw 'Workbook copy mismatch.' }

$required = @('実行ファイル（SKYBREAK）', 'プログラム説明資料（SKYBREAK）.pdf', $videoName, $sheetName)
$children = @(Get-ChildItem -LiteralPath $root -Force)
if ($children.Count -ne 4 -or @($children | Where-Object { $_.Name -notin $required }).Count) {
    throw 'Submission root must contain exactly the four required items.'
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($root, $zip, [IO.Compression.CompressionLevel]::Optimal, $false, [Text.Encoding]::UTF8)

# 圧縮後の全ファイルを読み戻して、元データとの一致を確認する。
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
$checked = 0
$expected = @(Get-ChildItem -LiteralPath $root -Recurse -File)
try {
    $entries = @{}
    foreach ($entry in $archive.Entries) { $entries[$entry.FullName] = $entry }
    foreach ($file in $expected) {
        $relative = [IO.Path]::GetRelativePath($root, $file.FullName).Replace('\', '/')
        if (-not $entries.ContainsKey($relative)) { throw "ZIP entry missing: $relative" }
        $entry = $entries[$relative]
        if ($entry.Length -ne $file.Length) { throw "ZIP length mismatch: $relative" }
        $stream = $entry.Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [Convert]::ToHexString($sha.ComputeHash($stream)) }
        finally { $sha.Dispose(); $stream.Dispose() }
        if ($hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) { throw "ZIP data mismatch: $relative" }
        ++$checked
    }
    $extra = @($archive.Entries | Where-Object { $_.Name -ne '' }).Count - $expected.Count
    if ($extra -ne 0) { throw 'Unexpected extra files in ZIP.' }
} finally { $archive.Dispose() }

Write-Output "FINAL_ZIP_VERIFIED files=$checked bytes=$((Get-Item -LiteralPath $zip).Length)"
Write-Output "ZIP=$zip"
Write-Output "ORIGINAL_PR_BACKUP=$backup"
Get-FileHash -LiteralPath $zip
