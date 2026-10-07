[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$Name = ('AZRAID_' + (Get-Date -Format 'yyyyMMdd'))
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$project = Join-Path $workspace 'project'
$release = Join-Path $workspace 'generated\outputs\Release'
$package = Join-Path $workspace ('generated\submissions\' + $Name)
$verification = Join-Path $workspace ('generated\verification\' + $Name + '-package')
$utf8 = [Text.UTF8Encoding]::new($false)

# 新しい実行用コピーを作る。既存の提出物・元データには上書きしない。
if (Test-Path -LiteralPath $package) { throw "Package already exists: $package" }
foreach ($runtimeFile in @('CG2.exe', 'dxcompiler.dll', 'dxil.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $release $runtimeFile) -PathType Leaf)) {
        throw "Missing Release runtime: $runtimeFile"
    }
}
New-Item -ItemType Directory -Path $package | Out-Null
New-Item -ItemType Directory -Path $verification -Force | Out-Null

$copies = [Collections.Generic.List[object]]::new()
function Copy-RuntimeFile([string]$source, [string]$relative) {
    $destination = Join-Path $package $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $sourceHash) {
        throw "Runtime copy mismatch: $relative"
    }
    $copies.Add([pscustomobject]@{File=$relative;SHA256=$sourceHash})
}

Copy-RuntimeFile (Join-Path $release 'CG2.exe') 'AZRAID.exe'
foreach ($dll in @('dxcompiler.dll', 'dxil.dll')) {
    Copy-RuntimeFile (Join-Path $release $dll) $dll
}

# 起動時に読むリソースと描画シェーダーだけを走査する。
# resourcesの.objは3Dモデルなので残す。
foreach ($directory in @('resources', 'shaders')) {
    $sourceDirectory = Join-Path $project $directory
    $rgArguments = @('--files', '--hidden', '--no-ignore', $sourceDirectory,
        '-g', '!**/__pycache__/**', '-g', '!**/.git/**',
        '-g', '!*.py', '-g', '!*.ps1', '-g', '!*.bat', '-g', '!*.cpp', '-g', '!*.pdb',
        '-g', '!README.md', '-g', '!HUD_FONTS.md', '-g', '!source_manifest.json')
    $files = @(& rg @rgArguments)
    if ($LASTEXITCODE -ne 0) { throw "Cannot enumerate runtime directory: $directory" }
    foreach ($file in $files) {
        Copy-RuntimeFile $file ([IO.Path]::GetRelativePath($project, $file))
    }
}

# glTFから参照される頂点データや画像が配布フォルダ内にあることを確認する。
foreach ($model in Get-ChildItem -LiteralPath (Join-Path $package 'resources') -Recurse -File -Filter '*.gltf') {
    $gltf = Get-Content -LiteralPath $model.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($property in @('buffers', 'images')) {
        if (-not $gltf.PSObject.Properties[$property]) { continue }
        foreach ($entry in $gltf.$property) {
            if (-not $entry.PSObject.Properties['uri'] -or $entry.uri.StartsWith('data:')) { continue }
            $dependency = [IO.Path]::GetFullPath((Join-Path $model.DirectoryName ([Uri]::UnescapeDataString($entry.uri))))
            if (-not $dependency.StartsWith($package + '\', [StringComparison]::OrdinalIgnoreCase) -or
                -not (Test-Path -LiteralPath $dependency -PathType Leaf)) {
                throw "Missing or external model dependency: $dependency"
            }
        }
    }
}

$licenseSources = [ordered]@{
    'licenses\DearImGui.txt' = 'externals\imgui\LICENSE.txt'
    'licenses\ImGuizmo.txt' = 'externals\ImGuizmo\LICENSE'
    'licenses\ImGuiFileDialog.txt' = 'externals\ImGuiFileDialog\LICENSE'
    'licenses\ImPlot.txt' = 'externals\implot\LICENSE'
    'licenses\imgui-node-editor.txt' = 'externals\imgui-node-editor\LICENSE'
    'licenses\ImGuiColorTextEdit.txt' = 'externals\ImGuiColorTextEdit\LICENSE'
    'licenses\IconFontCppHeaders.txt' = 'externals\IconFontCppHeaders\licence.txt'
}
foreach ($entry in $licenseSources.GetEnumerator()) {
    Copy-RuntimeFile (Join-Path $project $entry.Value) $entry.Key
}
foreach ($license in @('DirectXTex.txt', 'DirectXShaderCompiler.txt')) {
    Copy-RuntimeFile (Join-Path $PSScriptRoot ('licenses\' + $license)) ('licenses\' + $license)
}
$assimpHeader = Get-Content -LiteralPath (Join-Path $project 'externals\assimp\include\assimp\version.h') -Raw -Encoding UTF8
$assimpLicense = [regex]::Match($assimpHeader, '(?s)\A/\*(.*?)\*/').Groups[1].Value.Trim()
if (-not $assimpLicense) { throw 'Cannot find Assimp license notice.' }
[IO.File]::WriteAllText((Join-Path $package 'licenses\Assimp.txt'), $assimpLicense + "`r`n", $utf8)

$readme = @'
アズレイド / AZRAID

AZRAID.exeをダブルクリックするとタイトル画面が開きます。
resources・shaders・DLLは実行に必要です。フォルダ全体を一緒に移動してください。
Windows 10/11 64bit・DirectX 12対応の環境で実行してください。

操作
移動: W / A / S / D　照準: マウス　射撃: SPACE（長押しで連射）
回避: A または D + SHIFT　残像連撃: Q　ポーズ: ESC
全画面切替: F11 / Alt + Enter　終了: ウィンドウ右上の×
タイトル画面からチュートリアル・操作方法も確認できます。

使用ライブラリのライセンスはlicensesに、素材とフォントの利用表記はresources内に同梱しています。
'@
[IO.File]::WriteAllText((Join-Path $package 'ReadMe.txt'), $readme.Replace("`r`n", "`n").Replace("`n", "`r`n") + "`r`n", $utf8)
$copies | Export-Csv -LiteralPath (Join-Path $verification 'copied-files.csv') -NoTypeInformation -Encoding utf8NoBOM
$packageFiles = @(Get-ChildItem -LiteralPath $package -Recurse -File)
$bytes = ($packageFiles | Measure-Object Length -Sum).Sum
Write-Output "RUNTIME_PACKAGE_OK files=$($packageFiles.Count) mib=$([math]::Round($bytes / 1MB, 1))"
Write-Output "PACKAGE=$package"
Write-Output "VERIFICATION=$verification"
