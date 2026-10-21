@echo off
setlocal
chcp 65001 >nul
pushd "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  echo Run scripts\setup.ps1 first.
  popd
  exit /b 1
)
".venv\Scripts\python.exe" -B -X utf8 -u tools\obb_cli.py %*
set "PORT_EXIT=%ERRORLEVEL%"
popd
exit /b %PORT_EXIT%
