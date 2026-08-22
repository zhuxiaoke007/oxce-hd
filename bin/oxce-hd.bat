@echo off
rem oxce-hd launcher: mounts runtime data and a local userdata folder.
setlocal
set DIR=%~dp0
start "" "%DIR%openxcom.exe" -data "%DIR%..\runtime" -user "%DIR%..\userdata"
endlocal
