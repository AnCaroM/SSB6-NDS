@echo off
if not exist "%~dp0build\nds\ssb-scene.nds" (
  echo Compila primero tools\build_nds.py --demo scene.
  pause
  exit /b 1
)
start "" /D "%~dp0.tools\melonDS" "%~dp0.tools\melonDS\melonDS.exe" "%~dp0build\nds\ssb-scene.nds"
