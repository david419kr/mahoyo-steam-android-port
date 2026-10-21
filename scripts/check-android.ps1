$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$python=Join-Path $root '.venv/Scripts/python.exe'
foreach($name in @('android-language-check.py','android-reader-pull-check.py','android-panel-check.py','android-slot-check.py')){
 & $python -B -X utf8 (Join-Path $PSScriptRoot $name)
 if($LASTEXITCODE){throw "Failed: $name"}
}
