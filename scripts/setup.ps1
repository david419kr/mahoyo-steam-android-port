param([string]$Python='python')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& $Python -c "import sys; assert sys.version_info >= (3, 12) and sys.maxsize > 2**32, 'Python 3.12+ x64 required'"
if($LASTEXITCODE){throw 'Python 3.12+ x64 is required.'}
& $Python -m venv (Join-Path $root '.venv')
if($LASTEXITCODE){throw 'Python 3.12 x64 or newer is required.'}
$venv=Join-Path $root '.venv/Scripts/python.exe'
& $venv -m pip install --disable-pip-version-check -r (Join-Path $root 'requirements.txt')
if($LASTEXITCODE){throw 'Dependency installation failed.'}
Write-Output 'Dependencies prepared. See BUILDING.md for the Android build command.'
