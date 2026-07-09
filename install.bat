@echo off
color 0B
echo ==========================================
echo    Instal.lador de SnapFormat (Windows 11)
echo ==========================================
echo.

:: 1. Comprovar permisos d'administrador
NET SESSION >nul 2>&1
if %errorLevel% == 0 (
    goto :admin
) else (
    echo Sol.licitant permisos d'administrador...
    powershell -Command "Start-Process '%~dpnx0' -Verb RunAs"
    exit /B
)

:admin
:: 2. Anar a la carpeta on s'ha extret el ZIP
cd /d "%~dp0"

echo Registrant el paquet i els certificats a Windows...
powershell -ExecutionPolicy Bypass -File packaging\register-msix.ps1

echo.
echo ==========================================
echo   Instal.lacio completada amb exit!
echo   Ja pots tancar aquesta finestra.
echo ==========================================
pause