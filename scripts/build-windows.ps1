param(
    [string]$JavaHome = 'C:\Program Files\Android\Android Studio\jbr',
    [string]$Compiler = '',
    [string]$JniHeader = '',
    [string]$SteamExe = ''
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$deps=Join-Path $root '.deps/windows'
$package=Join-Path $root 'output/windows'
$python=Join-Path $root '.venv/Scripts/python.exe'
if(-not $SteamExe){
    $installed=Join-Path $env:LOCALAPPDATA 'MahoyoUnofficial/engine/.steam-ready'
    if(Test-Path $installed){$SteamExe=Join-Path ((Get-Content $installed -Raw | ConvertFrom-Json).source) 'WoH.exe'}
}
if(-not $SteamExe -or -not(Test-Path -LiteralPath $SteamExe)){throw 'Pass -SteamExe with the original Steam WoH.exe to extract its launcher icon.'}
$icon=Join-Path $root 'build/windows/icon/launcher.ico'
& $python -B -X utf8 (Join-Path $root 'desktop/scripts/extract-steam-icon.py') $SteamExe $icon
if($LASTEXITCODE){throw 'Steam icon extraction failed'}
if(-not(Test-Path "$JavaHome/bin/javac.exe")){throw 'Set -JavaHome to a JDK containing javac.exe.'}
if(-not $Compiler){$Compiler=(Get-Command clang++.exe -ErrorAction Stop).Source}
$manifest=Get-Content (Join-Path $root 'dependencies/windows.json') -Raw | ConvertFrom-Json
foreach($artifact in $manifest.artifacts){
    $file=Join-Path $deps $artifact.path
    New-Item -ItemType Directory -Path (Split-Path $file) -Force | Out-Null
    if(-not(Test-Path $file)){Invoke-WebRequest $artifact.url -OutFile $file}
    if((Get-FileHash $file -Algorithm SHA256).Hash -ne $artifact.sha256){throw "Dependency checksum mismatch: $($artifact.path)"}
    if($artifact.extract){$directory=Join-Path $deps $artifact.extract;if(-not(Test-Path $directory)){Expand-Archive -LiteralPath $file -DestinationPath $directory}}
}
if(-not(Test-Path "$deps/include/jni.h")){
    if(-not $JniHeader){$JniHeader=Join-Path $env:LOCALAPPDATA 'Android/Sdk/ndk/29.0.13113456/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include/jni.h'}
    if(-not(Test-Path $JniHeader)){throw 'Pass -JniHeader with the path to jni.h from a JDK or Android NDK.'}
    New-Item -ItemType Directory -Path "$deps/include" -Force | Out-Null
    Copy-Item $JniHeader "$deps/include/jni.h"
    $win32=Join-Path (Split-Path $JniHeader) 'win32'
    if(Test-Path $win32){Copy-Item $win32 "$deps/include/win32" -Recurse -Force}
}
$ffmpeg=(Get-ChildItem "$deps/ffmpeg-pinned" -Directory | Select-Object -First 1).FullName
$runtime=(Get-ChildItem "$deps/jre" -Directory | Select-Object -First 1).FullName
$native=Join-Path $root 'build/windows/native'
cmake -S $root -B $native -G Ninja "-DCMAKE_CXX_COMPILER=$Compiler" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DWOH_BUILD_DESKTOP=ON "-DWOH_JNI_INCLUDE=$deps/include" "-DWOH_FFMPEG_ROOT=$ffmpeg" "-DWOH_LAUNCHER_ICON=$icon"
if($LASTEXITCODE){throw 'Windows native configuration failed'}
cmake --build $native --target woh_desktop mahoyo_windows mahoyo_sync -j 2
if($LASTEXITCODE){throw 'Windows native build failed'}
$env:JAVA_HOME=$JavaHome
& $python -B -X utf8 (Join-Path $root 'desktop/scripts/compile.py')
if($LASTEXITCODE){throw 'Windows Java compilation failed'}
New-Item -ItemType Directory -Path "$root/build/windows/assets/icons" -Force | Out-Null
Copy-Item "$root/build/windows/icon/launcher-*.png" "$root/build/windows/assets/icons" -Force
foreach($directory in @('app','lib','native','assets/steam-profile','media','runtime','licenses')){New-Item -ItemType Directory -Path (Join-Path $package $directory) -Force | Out-Null}
& "$JavaHome/bin/jar.exe" --create --file "$package/app/mahoyo.jar" -C "$root/build/windows/classes" .
if($LASTEXITCODE){throw 'Windows application packaging failed'}
Copy-Item "$native/desktop/native/Mahoyo.exe" "$package/Mahoyo.exe" -Force
Copy-Item "$native/desktop/native/SaveSync.exe" "$package/SaveSync.exe" -Force
Copy-Item "$native/desktop/native/woh_android.dll" "$package/native/woh_android.dll" -Force
Copy-Item "$deps/lib/*.jar" "$package/lib" -Force
Copy-Item "$root/data/steam11-source-profile.json" "$package/assets/steam-profile/source-profile.json" -Force
Copy-Item "$root/data/ui-text.json" "$package/assets/ui-text.json" -Force
New-Item -ItemType Directory -Path "$package/assets/icons" -Force | Out-Null
Copy-Item "$root/build/windows/icon/launcher-*.png" "$package/assets/icons" -Force
Copy-Item "$ffmpeg/bin/*.dll","$ffmpeg/bin/ffmpeg.exe","$ffmpeg/bin/ffprobe.exe" "$package/media" -Force
Copy-Item "$ffmpeg/LICENSE.txt" "$package/licenses/FFmpeg-LGPL.txt" -Force
Copy-Item "$runtime/*" "$package/runtime" -Recurse -Force
Copy-Item "$root/licenses/desktop/*" "$package/licenses" -Force
Copy-Item "$root/licenses/runtime/iced-x86-LICENSE.txt" "$package/licenses" -Force
Copy-Item "$root/LICENSE" "$package/LICENSE.txt" -Force
Copy-Item "$root/desktop/README.md" "$package/README.md" -Force
Copy-Item "$root/dependencies/windows.json" "$package/licenses/dependencies.json" -Force
$apk=Join-Path $root 'android/app/build/outputs/apk/release/app-release.apk'
if(Test-Path $apk){New-Item -ItemType Directory -Path "$package/android" -Force | Out-Null;Copy-Item $apk "$package/android/mahoyo-save-sync.apk" -Force}
Write-Output "Windows launcher: $package/Mahoyo.exe"
