@echo off
if not exist "%~dp0build\nds\ssb-mario.nds" (
  echo Falta ssb-mario.nds. Compila tools\build_nds.py --demo mario.
  pause
  exit /b 1
)
start "" /D "%~dp0.tools\melonDS" "%~dp0.tools\melonDS\melonDS.exe" "%~dp0build\nds\ssb-mario.nds"
