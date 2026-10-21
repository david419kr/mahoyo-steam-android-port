param(
 [Parameter(Mandatory=$true)][string]$Source,
 [string]$SdkRoot=$env:ANDROID_SDK_ROOT,
 [string]$Gradle='gradle',
 [string]$Python='',
 [string]$SigningKey=$env:WOH_KEYSTORE
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$Python){$Python=Join-Path $root '.venv/Scripts/python.exe'}
if(!$SdkRoot){$SdkRoot=$env:ANDROID_HOME}
if(!$SdkRoot){throw 'Pass -SdkRoot or set ANDROID_SDK_ROOT.'}
$Source=(Resolve-Path -LiteralPath $Source).Path
$SdkRoot=(Resolve-Path -LiteralPath $SdkRoot).Path
& (Join-Path $PSScriptRoot 'prepare-steam.ps1')
& $Python -B (Join-Path $root 'tools/prepare_apk_assets.py') --source $Source
if($LASTEXITCODE){throw 'Owned-source asset preparation failed.'}
if(!(Test-Path -LiteralPath (Join-Path $root '.deps/android/arm64-v8a/lib/libavif.a'))){throw 'Run setup.ps1 -AvifPackages first.'}
if(!$SigningKey){$SigningKey=Join-Path $root '.local/signing/port-release.jks'}
$SigningKey=[IO.Path]::GetFullPath($SigningKey)
if(!(Test-Path -LiteralPath $SigningKey)){
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($SigningKey))|Out-Null
 $storePass=if($env:WOH_STORE_PASSWORD){$env:WOH_STORE_PASSWORD}else{'local-port'}
 $keyPass=if($env:WOH_KEY_PASSWORD){$env:WOH_KEY_PASSWORD}else{'local-port'}
 $keytool=if($env:JAVA_HOME){Join-Path $env:JAVA_HOME 'bin/keytool.exe'}else{'keytool'}
 & $keytool -genkeypair -keystore $SigningKey -storepass $storePass -keypass $keyPass -alias woh-port -keyalg RSA -keysize 3072 -validity 10000 -dname 'CN=Mahoyo Android Local Build'
 if($LASTEXITCODE){throw 'Signing key creation failed.'}
}
$env:WOH_KEYSTORE=$SigningKey
$sdkProperty=$SdkRoot.Replace('\','/').Replace(':','\:')
[IO.File]::WriteAllText((Join-Path $root 'android/local.properties'),"sdk.dir=$sdkProperty`n")
Push-Location (Join-Path $root 'android')
try{& $Gradle --no-daemon --max-workers=2 --console=plain :app:testReleaseUnitTest :app:assembleRelease;if($LASTEXITCODE){throw 'Gradle build failed.'}}finally{Pop-Location}
$tools=Join-Path $SdkRoot 'build-tools/36.0.0'
$apk=Join-Path $root 'android/app/build/outputs/apk/release/app-release.apk'
& (Join-Path $tools 'apksigner.bat') verify --verbose $apk
if($LASTEXITCODE){throw 'APK signature verification failed.'}
& (Join-Path $tools 'zipalign.exe') -c -P 16 4 $apk
if($LASTEXITCODE){throw 'APK 16KiB zip alignment failed.'}
$manifest=& (Join-Path $tools 'aapt2.exe') dump xmltree --file AndroidManifest.xml $apk
if($manifest -match 'android:(testOnly|debuggable).*0xffffffff'){throw 'Release enables debug/test flags.'}
$badging=& (Join-Path $tools 'aapt2.exe') dump badging $apk
if(($badging|Select-String '^package:').Line -notmatch "name='org\.mahoyo\.port\.unofficial'"){throw 'Unexpected application package.'}
Write-Output "APK: $apk"
Get-FileHash -LiteralPath $apk -Algorithm SHA256
