# 生成可移动的 P4 发布目录，不复制项目数据库，也不删除已有 data。
param(
    [string]$QtRoot = 'D:/Qt/6.8.3/mingw_64',
    [string]$MinGWBin = 'D:/Qt/Tools/mingw1310_64/bin',
    [string]$CMake = 'C:/mingw64/bin/cmake.exe'
)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$destination = Join-Path $project 'dist/JLU_InstantMessaging'
$oldPath = $env:PATH
Push-Location $project
try {
    $env:PATH = "$MinGWBin;$QtRoot/bin;$oldPath"
    & $CMake --preset gui-release
    if ($LASTEXITCODE -ne 0) { throw 'Release 配置失败' }
    & $CMake --build --preset gui-release
    if ($LASTEXITCODE -ne 0) { throw 'Release 构建失败，请先关闭旧程序' }
    & $CMake --install build/gui-release --prefix $destination --config Release
    if ($LASTEXITCODE -ne 0) { throw '复制程序失败，请先关闭旧程序' }
    & "$QtRoot/bin/windeployqt.exe" --release --compiler-runtime --no-translations --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --dir $destination "$destination/jlu_im_gui.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Qt 运行依赖部署失败' }
    # 两个 Release 入口使用同一套 MinGW 13.1，避免运行库混用。
    foreach ($dll in @('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        Copy-Item -LiteralPath "$MinGWBin/$dll" -Destination $destination -Force
    }
    "[Paths]`nPrefix=.`nPlugins=." | Set-Content -Encoding ascii "$destination/qt.conf"
    $licenses = Join-Path $destination 'licenses'
    New-Item -ItemType Directory -Force $licenses | Out-Null
    Copy-Item -Path 'third_party/qt_licenses/*' -Destination $licenses -Force
    Copy-Item -LiteralPath "$QtRoot/sbom/qtbase-6.8.3.spdx.json" -Destination $licenses -Force
    Copy-Item -LiteralPath "$QtRoot/sbom/qtbase-6.8.3.source.spdx" -Destination $licenses -Force
    $mingwLicenses = Join-Path (Split-Path -Parent $MinGWBin) 'licenses'
    foreach ($name in @('gcc','winpthreads','mingw-w64')) {
        Copy-Item -LiteralPath "$mingwLicenses/$name" -Destination $licenses -Recurse -Force
    }
    Copy-Item -LiteralPath 'third_party/sqlite/README.md' -Destination "$licenses/SQLite.md" -Force
    Copy-Item -LiteralPath 'scripts/发布版使用说明.md' -Destination "$destination/使用说明.md" -Force
    # 压缩包不包含使用者的数据库，更新发布目录时原 data 仍保留。
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zipPath = Join-Path $project 'dist/JLU_InstantMessaging_P4_Windows_x64.zip'
    $stream = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::Create)
    $archive = New-Object System.IO.Compression.ZipArchive($stream, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem -LiteralPath $destination -File -Recurse) {
            $relative = $file.FullName.Substring($destination.Length + 1).Replace('\', '/')
            if ($relative.StartsWith('data/') -or $file.Name -like '*.db' -or $file.Name -like '*.db-*') { continue }
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, "JLU_InstantMessaging/$relative") | Out-Null
        }
    }
    finally { $archive.Dispose(); $stream.Dispose() }
    Write-Output "发布目录：$destination"
    Write-Output "便携压缩包：$zipPath"
}
finally {
    $env:PATH = $oldPath
    Pop-Location
}
