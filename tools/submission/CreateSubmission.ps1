[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$project = Join-Path $workspace 'project'
$stage = Join-Path $workspace 'generated\kurihaku_20261001'
$root = Join-Path $stage '小泉羚（日本工学院専門学校_ゲームクリエイター科）'
$game = Join-Path $root '実行ファイル（SKYBREAK）'
$utf8 = [Text.UTF8Encoding]::new($false)

# 既存の提出物には上書きせず、現在の作業ツリーから新しい提出用コピーを作る。
if (Test-Path -LiteralPath $stage) { throw "Already exists: $stage" }
New-Item -ItemType Directory -Path $game -Force | Out-Null

function Copy-ProjectFile([string]$relative) {
    $source = Join-Path $project $relative
    $destination = Join-Path $game $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
}

# 保護対象やビルドキャッシュを走査しないよう、必要なディレクトリだけを列挙する。
Push-Location $project
try {
    $files = & rg --files --hidden --no-ignore src include shaders resources externals tools `
        -g '!**/enc_temp_folder/**' -g '!**/.git/**' -g '!**/.vs/**' `
        -g '!**/generated/**' -g '!**/x64/**' -g '!**/__pycache__/**' `
        -g '!**/Shaders/Compiled/**'
    if ($LASTEXITCODE -ne 0) { throw 'Failed to enumerate project files.' }
    foreach ($relative in $files) {
        $normalized = $relative.Replace('\', '/')
        # resources 内の .obj は3Dモデルなので除外しない。
        if ($normalized.StartsWith('externals/') -or $normalized.StartsWith('tools/')) {
            if ($normalized -match '\.(pdb|obj|pch|tlog|user|exe|ilk|idb|lastbuildstate|log)$') { continue }
        }
        if ($normalized -eq 'externals/assimp/lib/Debug/assimp-vc143-mtd.lib') { continue }
        if ($normalized -eq 'tools/RunSmokeTest.ps1') { continue }
        Copy-ProjectFile $relative
    }
    foreach ($relative in @('CG2.sln', 'CG2.vcxproj', 'CG2.vcxproj.filters', 'CG2.rc', 'resource.h', '.editorconfig')) {
        Copy-ProjectFile $relative
    }
} finally { Pop-Location }

Copy-Item -LiteralPath (Join-Path $workspace 'generated\outputs\Release\CG2.exe') -Destination (Join-Path $game 'SKYBREAK.exe')
foreach ($dll in @('dxcompiler.dll', 'dxil.dll')) {
    Copy-Item -LiteralPath (Join-Path $workspace "generated\outputs\Release\$dll") -Destination (Join-Path $game $dll)
}
Copy-Item -LiteralPath (Join-Path $workspace 'output\pdf\プログラム説明資料（SKYBREAK）.pdf') -Destination $root
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ReadMe.txt') -Destination $game

$licenses = Join-Path $game 'licenses'
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
$assimpHeader = Get-Content -LiteralPath (Join-Path $project 'externals\assimp\include\assimp\version.h') -Raw -Encoding UTF8
$assimpLicense = [regex]::Match($assimpHeader, '(?s)\A/\*(.*?)\*/').Groups[1].Value.Trim()
[IO.File]::WriteAllText((Join-Path $licenses 'assimp.txt'), $assimpLicense + "`r`n", $utf8)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ThirdParty.txt') -Destination (Join-Path $licenses 'ThirdParty.txt')
foreach ($name in @('DirectXTex.txt', 'DirectXShaderCompiler.txt')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "licenses\$name") -Destination $licenses
}

$checklist = @'
提出前の残り作業

次の2ファイルを「小泉羚（日本工学院専門学校_ゲームクリエイター科）」フォルダ直下に追加する。
  作品実演動画（SKYBREAK）.mp4  … 2分以内
  自己PRシート（小泉羚）.xlsx   … 主催者の指定フォーマット

4点がそろったら、同フォルダをZIP圧縮する。
ZIP名: 小泉羚（日本工学院専門学校_ゲームクリエイター科）.zip

提出締切: 2026年10月1日(木) 23:55
送信後「ファイルを正常に送信しました。」の表示を確認する。
このチェックリストと検証ログはZIPへ入れない。
'@
[IO.File]::WriteAllText((Join-Path $stage '提出前チェック.txt'), $checklist + "`r`n", $utf8)
Write-Output "SUBMISSION_ROOT=$root"
Write-Output "GAME_ROOT=$game"
