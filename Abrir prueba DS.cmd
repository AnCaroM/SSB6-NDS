@echo off
if not exist "%~dp0build\nds\ssb-motion.nds" (
  echo Falta build\nds\ssb-motion.nds. Ejecuta primero tools\build_nds.py.
  pause
  exit /b 1
)
start "" /D "%~dp0.tools\melonDS" "%~dp0.tools\melonDS\melonDS.exe" "%~dp0build\nds\ssb-motion.nds"
