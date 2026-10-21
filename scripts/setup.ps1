param([string]$Python='python', [string]$SteamlessDirectory='', [string]$MediaDirectory='', [string]$AvifPackages='')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& $Python -c "import sys; assert sys.version_info >= (3, 12) and sys.maxsize > 2**32, 'Python 3.12+ x64 required'"
if($LASTEXITCODE){throw 'Python 3.12+ x64 is required.'}
& $Python -m venv (Join-Path $root '.venv')
if($LASTEXITCODE){throw 'Python 3.12 x64 or newer is required.'}
$venv=Join-Path $root '.venv/Scripts/python.exe'
& $venv -m pip install --disable-pip-version-check -r (Join-Path $root 'requirements.txt')
if($LASTEXITCODE){throw 'Dependency installation failed.'}
foreach($item in @(@($SteamlessDirectory,'steamless','Steamless.CLI.exe'),@($MediaDirectory,'media','ffmpeg.exe'))){
 if($item[0]){
  $source=(Resolve-Path -LiteralPath $item[0]).Path
  if(!(Test-Path -LiteralPath (Join-Path $source $item[2]))){throw "Missing tool: $($item[2])"}
  $target=Join-Path $root ".deps/$($item[1])"
  $targetPath=[IO.Path]::GetFullPath($target)
  if($targetPath -eq $source -or $targetPath.StartsWith($source.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Choose a tool directory outside the repository dependency output.'}
  [IO.Directory]::CreateDirectory($target)|Out-Null
  Get-ChildItem -LiteralPath $source | Copy-Item -Destination $target -Recurse -Force
 }
}
if($AvifPackages){& $venv (Join-Path $root 'scripts/prepare-avif-deps.py') --packages $AvifPackages;if($LASTEXITCODE){throw 'Codec staging failed.'}}
Write-Output 'Dependencies prepared. See BUILDING.md for the Android build command.'
