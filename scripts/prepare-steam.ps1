$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$profile=Get-Content -LiteralPath (Join-Path $root 'dependencies/steam.json') -Raw|ConvertFrom-Json
$target=Join-Path $root '.deps/JavaSteam'
if(!(Test-Path -LiteralPath (Join-Path $target '.git'))){git clone --no-checkout $profile.repository $target;if($LASTEXITCODE){throw 'JavaSteam clone failed.'}}
$dirty=git -C $target status --porcelain
$empty=@(Get-ChildItem -LiteralPath $target -Force|Where-Object Name -ne '.git').Count -eq 0
if($dirty -and !$empty){throw 'JavaSteam source has local changes; preserve them before updating dependencies.'}
git -C $target fetch origin $profile.commit --depth 1
if($LASTEXITCODE){throw 'Pinned JavaSteam fetch failed.'}
git -C $target checkout --detach $profile.commit
if($LASTEXITCODE){throw 'Pinned JavaSteam checkout failed.'}
$actual=git -C $target rev-parse HEAD
if($actual.Trim() -ne $profile.commit){throw 'JavaSteam commit verification failed.'}
Write-Output "Pinned JavaSteam: $actual"
