@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem =====================================================================
rem  One-click training for the gauge detector (yolov4-tiny).
rem
rem    train.bat            train at 640 (default, best accuracy)
rem    train.bat 416        train at 416 (matches a Zynq-7020 board)
rem    train.bat check      check environment only, no training
rem
rem  Pipeline: python -> anchors -> train.py -> export_deploy.py
rem  Log file: logs_meiter_v1\train_console.log
rem  Override python: set GAUGE_PYTHON=D:\Python311\python.exe
rem =====================================================================

set "SIZE=%~1"
set "MODE=train"
if /i "%SIZE%"=="check" (
    set "MODE=check"
    set "SIZE=640"
)
if not defined SIZE set "SIZE=640"

set /a _MOD=%SIZE% %% 32
if not "%_MOD%"=="0" (
    echo [ERROR] size must be a multiple of 32, got "%SIZE%"
    exit /b 1
)

rem ---------- 1) locate python ----------
set "PY="
if defined GAUGE_PYTHON set "PY=%GAUGE_PYTHON%"
if not defined PY (
    py -3 -c "import sys" >nul 2>&1
    if not errorlevel 1 set "PY=py -3"
)
if not defined PY (
    if exist "D:\Python311\python.exe" set "PY=D:\Python311\python.exe"
)
if not defined PY (
    python -c "import sys" >nul 2>&1
    if not errorlevel 1 set "PY=python"
)
if not defined PY (
    echo [ERROR] python not found. set GAUGE_PYTHON=D:\Python311\python.exe
    exit /b 1
)
echo [1/4] python = %PY%

if /i "%MODE%"=="check" goto :envcheck

rem ---------- 2) anchors for this size ----------
set "ANCHOR=model_data\yolo_anchors_meiter_%SIZE%.txt"
if exist "%ANCHOR%" (
    echo [2/4] anchors exist: %ANCHOR%
    goto :anchors_ok
)
echo [2/4] anchors for %SIZE% missing, computing from dataset labels ...
%PY% auto_anchors.py %SIZE%
if errorlevel 1 (
    echo [ERROR] auto_anchors.py failed
    exit /b 1
)
:anchors_ok

rem ---------- 3) train ----------
if not exist "logs_meiter_v1" mkdir "logs_meiter_v1"
set "LOG=logs_meiter_v1\train_console.log"
echo [3/4] training at %SIZE% ... (log: %LOG%)
echo.
powershell -NoProfile -ExecutionPolicy Bypass -Command "$env:GAUGE_INPUT_SIZE='%SIZE%'; & %PY% -u train.py *>&1 | Tee-Object -FilePath '%LOG%'; exit $LASTEXITCODE"
if errorlevel 1 (
    echo.
    echo [ERROR] training failed, see %LOG%
    exit /b 1
)

rem ---------- 4) export for the board ----------
echo.
echo [4/4] exporting deployment artifacts ...
%PY% export_deploy.py --size %SIZE% --weights logs_meiter_v1\best_epoch_weights.pth --out deploy
if errorlevel 1 (
    echo [ERROR] export failed
    exit /b 1
)

echo.
echo =====================================================================
echo  Done.
echo    weights : logs_meiter_v1\best_epoch_weights.pth
echo    onnx    : deploy\yolov4tiny_gauge_%SIZE%_fp32.onnx
echo    spec    : deploy\deploy_spec.json
echo    readme  : deploy\README_deploy.md
echo =====================================================================
exit /b 0

:envcheck
echo [check] python version:
%PY% -c "import sys; print('  ', sys.version)"
%PY% -c "import torch; print('   torch', torch.__version__, 'cuda', torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else '')"
%PY% -c "import os,yaml" 2>nul
if errorlevel 1 echo   [warn] pyyaml not installed
echo [check] anchors files:
for %%F in (model_data\yolo_anchors_meiter_*.txt) do echo    %%F
echo [check] optional packages:
%PY% -c "import onnx,onnxruntime;print('   onnx',onnx.__version__,'| onnxruntime',onnxruntime.__version__)" || echo    [warn] onnx/onnxruntime missing: pip install onnx onnxruntime
echo [check] dataset:
%PY% -c "import os; d='meiter_analog_clean'; [print('  ', s, len(os.listdir(os.path.join(d,s,'images')))) for s in ('train','val','test')]"
exit /b 0
