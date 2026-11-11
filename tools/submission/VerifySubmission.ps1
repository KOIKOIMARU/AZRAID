[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$project = Join-Path $workspace 'project'
$stage = Join-Path $workspace 'generated\kurihaku_20261001'
$root = Join-Path $stage '小泉羚（日本工学院専門学校_ゲームクリエイター科）'
$game = Join-Path $root '実行ファイル（SKYBREAK）'

# この提出用コピーを検証する間に生成されたファイルだけを削除する。
foreach ($temporary in @(
    (Join-Path $root 'generated'),
    (Join-Path $game 'externals\DirectXTex\Shaders\Compiled'),
    (Join-Path $game 'imgui.ini')
)) {
    $resolved = [IO.Path]::GetFullPath($temporary)
    if (-not $resolved.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Cleanup target escaped the new submission folder: $resolved"
    }
    if (Test-Path -LiteralPath $resolved) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}

$checked = 0
foreach ($directory in @('src', 'include', 'shaders', 'resources')) {
    $sourceDirectory = Join-Path $project $directory
    foreach ($source in Get-ChildItem -LiteralPath $sourceDirectory -Recurse -File) {
        $relative = [IO.Path]::GetRelativePath($project, $source.FullName)
        $destination = Join-Path $game $relative
        if (-not (Test-Path -LiteralPath $destination)) { throw "Missing: $relative" }
        if ((Get-FileHash -LiteralPath $source.FullName).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) {
            throw "Copy differs from current source: $relative"
        }
        ++$checked
    }
}
$pdf = 'プログラム説明資料（SKYBREAK）.pdf'
if ((Get-FileHash -LiteralPath (Join-Path $workspace "output\pdf\$pdf")).Hash -ne
    (Get-FileHash -LiteralPath (Join-Path $root $pdf)).Hash) { throw 'PDF copy differs.' }

# glTFの外部画像・頂点データがフォルダ内で解決できることを確認する。
foreach ($model in Get-ChildItem -LiteralPath (Join-Path $game 'resources') -Recurse -File -Filter '*.gltf') {
    $gltf = Get-Content -LiteralPath $model.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($property in @('buffers', 'images')) {
        if (-not $gltf.PSObject.Properties[$property]) { continue }
        foreach ($entry in $gltf.$property) {
            if (-not $entry.PSObject.Properties['uri']) { continue }
            if ($entry.uri.StartsWith('data:')) { continue }
            $dependency = Join-Path $model.DirectoryName ([Uri]::UnescapeDataString($entry.uri))
            if (-not (Test-Path -LiteralPath $dependency)) { throw "Missing glTF dependency: $dependency" }
        }
    }
}

$required = @('SKYBREAK.exe', 'dxcompiler.dll', 'dxil.dll', 'ReadMe.txt', 'CG2.sln',
    'licenses\assimp.txt', 'licenses\DirectXTex.txt', 'licenses\DirectXShaderCompiler.txt')
foreach ($relative in $required) {
    $path = Join-Path $game $relative
    if (-not (Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Missing or empty: $relative"
    }
}
$files = Get-ChildItem -LiteralPath $root -Recurse -File
$bytes = ($files | Measure-Object Length -Sum).Sum
$manifest = foreach ($file in $files) {
    [pscustomobject]@{
        File = [IO.Path]::GetRelativePath($root, $file.FullName)
        Bytes = $file.Length
        SHA256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
}
$manifest | Export-Csv -LiteralPath (Join-Path $stage 'manifest.csv') -NoTypeInformation -Encoding utf8NoBOM
Write-Output "VERIFIED source_and_resource_files=$checked total_files=$($files.Count) total_mib=$([math]::Round($bytes / 1MB, 1))"
Write-Output "ROOT=$root"
