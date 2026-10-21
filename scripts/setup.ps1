param([string]$Python='python', [string]$SteamlessDirectory='')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& $Python -c "import sys; assert sys.version_info >= (3, 12) and sys.maxsize > 2**32, 'Python 3.12+ x64 required'"
if($LASTEXITCODE){throw 'Python 3.12+ x64 is required.'}
& $Python -m venv (Join-Path $root '.venv')
if($LASTEXITCODE){throw 'Python 3.12 x64 or newer is required.'}
$venv=Join-Path $root '.venv/Scripts/python.exe'
& $venv -m pip install --disable-pip-version-check -r (Join-Path $root 'requirements.txt')
if($LASTEXITCODE){throw 'Dependency installation failed.'}
if($SteamlessDirectory){
 $source=(Resolve-Path -LiteralPath $SteamlessDirectory).Path
 if(!(Test-Path -LiteralPath (Join-Path $source 'Steamless.CLI.exe'))){throw 'Missing Steamless.CLI.exe'}
 $target=Join-Path $root '.deps/steamless'
 $targetPath=[IO.Path]::GetFullPath($target)
 if($targetPath -eq $source -or $targetPath.StartsWith($source.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Choose a tool directory outside the repository dependency output.'}
 [IO.Directory]::CreateDirectory($target)|Out-Null
 Get-ChildItem -LiteralPath $source | Copy-Item -Destination $target -Recurse -Force
}
Write-Output 'Dependencies prepared. See BUILDING.md for the Android build command.'
