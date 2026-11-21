@echo off
color 0B
echo ==========================================
echo    SnapFormat installer (Windows 11)
echo ==========================================
echo.

:: 1. Check for administrator rights
NET SESSION >nul 2>&1
if %errorLevel% == 0 (
    goto :admin
) else (
    echo Requesting administrator rights...
    powershell -Command "Start-Process '%~dpnx0' -Verb RunAs"
    exit /B
)

:admin
:: 2. Go to the folder where the ZIP was extracted
cd /d "%~dp0"

echo Signing and registering the package...
powershell -ExecutionPolicy Bypass -File packaging\register-msix.ps1
if %errorLevel% neq 0 (
    echo.
    echo Installation failed. See the messages above.
    pause
    exit /B 1
)

echo.
echo ==========================================
echo   Installation completed successfully!
echo   You can close this window.
echo ==========================================
pause
