@echo off
echo Iniciando NOVA Studio en Windows...
cd /d "%~dp0build_win\Release"
start "" "nova_ui_qt.exe"
