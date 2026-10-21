$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$profile=Get-Content -LiteralPath (Join-Path $root 'dependencies/steam.json') -Raw|ConvertFrom-Json
$target=Join-Path $root '.deps/JavaSteam'
function Get-SourceHash([string]$text){$hash=[Security.Cryptography.SHA256]::Create();try{return [BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($text))).Replace('-','').ToLowerInvariant()}finally{$hash.Dispose()}}
$patch=$profile.androidDownloaderPatch
if(!$patch){throw 'Pinned Android downloader patch is missing.'}
$patchedFile=[IO.Path]::GetFullPath((Join-Path $target $patch.path))
if(!$patchedFile.StartsWith([IO.Path]::GetFullPath($target)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Downloader patch path escaped dependency checkout.'}
if(!(Test-Path -LiteralPath (Join-Path $target '.git'))){git clone --no-checkout $profile.repository $target;if($LASTEXITCODE){throw 'JavaSteam clone failed.'}}
$dirty=git -C $target status --porcelain
$empty=@(Get-ChildItem -LiteralPath $target -Force|Where-Object Name -ne '.git').Count -eq 0
if($dirty -and !$empty){
 $unexpected=@($dirty|Where-Object {$_ -ne (' M '+$patch.path)})
 $current=if(Test-Path -LiteralPath $patchedFile){[IO.File]::ReadAllText($patchedFile).Replace("`r`n","`n")}else{''}
 if($unexpected.Count -or (Get-SourceHash $current) -ne $patch.patchedSha256){throw 'JavaSteam source has unrecognized local changes; preserve them before updating dependencies.'}
}
git -C $target fetch origin $profile.commit --depth 1
if($LASTEXITCODE){throw 'Pinned JavaSteam fetch failed.'}
git -C $target checkout --detach $profile.commit
if($LASTEXITCODE){throw 'Pinned JavaSteam checkout failed.'}
$actual=git -C $target rev-parse HEAD
if($actual.Trim() -ne $profile.commit){throw 'JavaSteam commit verification failed.'}
$text=[IO.File]::ReadAllText($patchedFile).Replace("`r`n","`n")
$digest=Get-SourceHash $text
if($digest -eq $patch.baseSha256){
 foreach($change in $patch.replacements){if([regex]::Matches($text,[regex]::Escape($change.before)).Count -ne 1){throw 'Pinned downloader patch context changed.'};$text=$text.Replace($change.before,$change.after)}
 if((Get-SourceHash $text) -ne $patch.patchedSha256){throw 'Patched downloader checksum mismatch.'}
 [IO.File]::WriteAllText($patchedFile,$text,[Text.UTF8Encoding]::new($false))
}elseif($digest -ne $patch.patchedSha256){throw 'Unexpected downloader source checksum.'}
Write-Output "Pinned JavaSteam: $actual"
Write-Output "Android downloader patch: $($patch.patchedSha256)"
