@echo off
setlocal enabledelayedexpansion
set "in=%~1"
set "out=%~dpn1.png"
set "log=%TEMP%\win11qc_convert.log"
set "exe=D:\Documentos\win11-quick-converter\build\src\converter-cli\Release\converter-cli.exe"

echo [%date% %time%] Invoked with: %* >> "%log%"
echo [%date% %time%] CWD: %CD% >> "%log%"
echo [%date% %time%] Running: "%exe%" --input "%in%" --output "%out%" --format png >> "%log%"
"%exe%" --input "%in%" --output "%out%" --format png >> "%log%" 2>&1
set "rc=%ERRORLEVEL%"
echo [%date% %time%] ExitCode: %rc% >> "%log%"
if exist "%out%" (
	echo [%date% %time%] Output created: "%out%" >> "%log%"
) else (
	echo [%date% %time%] Output NOT created >> "%log%"
)
endlocal
exit /b %rc%
