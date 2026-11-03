@echo off
rem run_sim.bat - rerun conv 1x1 pwconv testbench with Vivado Simulator 2020.2
rem output: sim.log / events.log   (search "RESULT:" in sim.log)
setlocal
cd /d "%~dp0"

set VIV=%VIVADO_BIN%
if "%VIV%"=="" set VIV=D:\Vivado\2020.2\bin

echo [1/3] xvlog compile ...
call "%VIV%\xvlog.bat" -f filelist.f -log xvlog.log
if errorlevel 1 goto fail

echo [2/3] xelab elaborate ...
call "%VIV%\xelab.bat" tb_conv_pwconv -s tb_sim -debug typical -log xelab.log
if errorlevel 1 goto fail

echo [3/3] xsim run ...
call "%VIV%\xsim.bat" tb_sim -runall -log sim.log
if errorlevel 1 goto fail

echo Done. See sim.log (search "RESULT:").
exit /b 0

:fail
echo FAILED. Check xvlog.log / xelab.log / sim.log
exit /b 1
