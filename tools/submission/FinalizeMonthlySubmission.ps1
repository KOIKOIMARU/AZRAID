[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

# ASCII source also works in Windows PowerShell 5.1 with UTF-8 data files.
$root = $PSScriptRoot
$config = [IO.File]::ReadAllText((Join-Path $root 'submission.json'), [Text.Encoding]::UTF8) | ConvertFrom-Json
$directories = @(Get-ChildItem -LiteralPath $root -Directory)
$contents = @($directories | Where-Object { $_.Name -notmatch '^ZIP' })
if ($contents.Count -ne 1) { throw 'Expected exactly one submission contents folder.' }
$contentsPath = $contents[0].FullName
$personal = @(Get-ChildItem -LiteralPath $contentsPath -Directory | Where-Object { $_.Name -match '^01_' })
if ($personal.Count -ne 1) { throw 'Individual project folder was not found.' }
$videoFolder = @(Get-ChildItem -LiteralPath $personal[0].FullName -Directory | Where-Object { $_.Name -match '^04_' })
if ($videoFolder.Count -ne 1) { throw 'Video folder was not found.' }
$videos = @(Get-ChildItem -LiteralPath $videoFolder[0].FullName -File -Filter '*.mp4')
if ($videos.Count -ne 1 -or $videos[0].Length -eq 0) {
    throw $config.messages.videoMissing
}

$parts = @(Get-ChildItem -LiteralPath $personal[0].FullName -Directory)
$sourcePart = @($parts | Where-Object { $_.Name -match '^01_' })
$runtimePart = @($parts | Where-Object { $_.Name -match '^02_' })
$documentPart = @($parts | Where-Object { $_.Name -match '^03_' })
$portfolioPart = @(Get-ChildItem -LiteralPath $contentsPath -Directory | Where-Object { $_.Name -match '^00_' })
if ($sourcePart.Count -ne 1 -or $runtimePart.Count -ne 1 -or $documentPart.Count -ne 1 -or $portfolioPart.Count -ne 1) {
    throw 'Required submission folders are missing.'
}
$solutions = @(Get-ChildItem -LiteralPath $sourcePart[0].FullName -Recurse -File -Filter 'CG2.sln')
$documentPdfs = @(Get-ChildItem -LiteralPath $documentPart[0].FullName -File -Filter '*.pdf')
$portfolioPdfs = @(Get-ChildItem -LiteralPath $portfolioPart[0].FullName -File -Filter '*.pdf')
if ($solutions.Count -ne 1 -or $documentPdfs.Count -ne 1 -or $portfolioPdfs.Count -ne 1) {
    throw 'The solution, program document, or portfolio PDF is missing.'
}
foreach ($name in @('AZRAID.exe', 'dxcompiler.dll', 'dxil.dll', 'resources', 'shaders')) {
    if (-not (Test-Path -LiteralPath (Join-Path $runtimePart[0].FullName $name))) {
        throw "Required runtime file is missing: $name"
    }
}

# Keep Japanese names in UTF-8 JSON, not in a legacy-encoded script literal.
$videoLabel = -join ([char[]](0x4F5C,0x54C1,0x7D39,0x4ECB,0x52D5,0x753B))
$videoName = $config.account + '_' + $videoLabel + '.mp4'
$namedVideo = Join-Path $videoFolder[0].FullName $videoName
if ($videos[0].FullName -ne $namedVideo) {
    if (Test-Path -LiteralPath $namedVideo) { throw 'Named video already exists.' }
    Rename-Item -LiteralPath $videos[0].FullName -NewName $videoName
}
$external = @($directories | Where-Object { $_.Name -match '^ZIP' })
if ($external.Count -ne 1) { throw 'External submission folder was not found.' }
Copy-Item -LiteralPath $namedVideo -Destination (Join-Path $external[0].FullName $videoName) -Force

$zip = Join-Path $root ($config.account + '.zip')
$temporary = Join-Path $root ($config.account + '.building.zip')
if (Test-Path -LiteralPath $temporary) { throw 'An unfinished ZIP already exists.' }
[IO.Compression.ZipFile]::CreateFromDirectory($contentsPath, $temporary,
    [IO.Compression.CompressionLevel]::Optimal, $false, [Text.Encoding]::UTF8)
$archive = [IO.Compression.ZipFile]::OpenRead($temporary)
try {
    $entries = @($archive.Entries | Where-Object { $_.Name -ne '' })
    $files = @(Get-ChildItem -LiteralPath $contentsPath -Recurse -File)
    if ($entries.Count -ne $files.Count) { throw 'ZIP file count mismatch.' }
    $lookup = @{}
    foreach ($entry in $entries) { $lookup[$entry.FullName.Replace('\', '/')] = $entry }
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($contentsPath.Length + 1).Replace('\', '/')
        if (-not $lookup.ContainsKey($relative) -or $lookup[$relative].Length -ne $file.Length) {
            throw "Missing or incomplete ZIP entry: $relative"
        }
        $stream = $lookup[$relative].Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
        finally { $sha.Dispose(); $stream.Dispose() }
        if ($hash -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {
            throw "ZIP checksum mismatch: $relative"
        }
    }
} finally { $archive.Dispose() }
if (Test-Path -LiteralPath $zip) {
    Move-Item -LiteralPath $zip -Destination ($zip + '.backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
Move-Item -LiteralPath $temporary -Destination $zip
Write-Output ($config.messages.complete + $zip)
Write-Output $config.messages.upload
