$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$profile=Get-Content -LiteralPath (Join-Path $root 'dependencies/steam.json') -Raw|ConvertFrom-Json
$target=Join-Path $root '.deps/JavaSteam'
function Get-SourceHash([string]$text){$hash=[Security.Cryptography.SHA256]::Create();try{return [BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($text))).Replace('-','').ToLowerInvariant()}finally{$hash.Dispose()}}
$patches=@($profile.androidDownloaderPatch,$profile.androidCodecPatch)
if(@($patches|Where-Object {!$_}).Count){throw 'Pinned Android download patches are missing.'}
foreach($patch in $patches){
 $patchedFile=[IO.Path]::GetFullPath((Join-Path $target $patch.path))
 if(!$patchedFile.StartsWith([IO.Path]::GetFullPath($target)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Patch path escaped dependency checkout.'}
}
if(!(Test-Path -LiteralPath (Join-Path $target '.git'))){git clone --no-checkout $profile.repository $target;if($LASTEXITCODE){throw 'JavaSteam clone failed.'}}
$dirty=git -C $target status --porcelain
$empty=@(Get-ChildItem -LiteralPath $target -Force|Where-Object Name -ne '.git').Count -eq 0
if($dirty -and !$empty){
 $allowed=@($patches|ForEach-Object {' M '+$_.path})
 if(@($dirty|Where-Object {$_ -notin $allowed}).Count){throw 'JavaSteam source has unrecognized local changes; preserve them before updating dependencies.'}
 foreach($patch in $patches){
  $current=[IO.File]::ReadAllText((Join-Path $target $patch.path)).Replace("`r`n","`n")
  if((Get-SourceHash $current) -notin @($patch.baseSha256,$patch.patchedSha256)){throw 'JavaSteam patched source has unrecognized local changes; preserve them before updating dependencies.'}
 }
}
git -C $target fetch origin $profile.commit --depth 1
if($LASTEXITCODE){throw 'Pinned JavaSteam fetch failed.'}
git -C $target checkout --detach $profile.commit
if($LASTEXITCODE){throw 'Pinned JavaSteam checkout failed.'}
$actual=git -C $target rev-parse HEAD
if($actual.Trim() -ne $profile.commit){throw 'JavaSteam commit verification failed.'}
foreach($patch in $patches){
$patchedFile=Join-Path $target $patch.path
$text=[IO.File]::ReadAllText($patchedFile).Replace("`r`n","`n")
$digest=Get-SourceHash $text
if($digest -eq $patch.baseSha256){
 foreach($change in $patch.replacements){if([regex]::Matches($text,[regex]::Escape($change.before)).Count -ne 1){throw 'Pinned downloader patch context changed.'};$text=$text.Replace($change.before,$change.after)}
 if((Get-SourceHash $text) -ne $patch.patchedSha256){throw 'Patched downloader checksum mismatch.'}
 [IO.File]::WriteAllText($patchedFile,$text,[Text.UTF8Encoding]::new($false))
}elseif($digest -ne $patch.patchedSha256){throw 'Unexpected downloader source checksum.'}
Write-Output "Android patch $($patch.path): $($patch.patchedSha256)"
}
Write-Output "Pinned JavaSteam: $actual"
