@echo off
REM ============================================================================
REM  ACID-303 Windows build (VST3 + offline renderer + verification tools)
REM
REM    build.bat              configure and build everything (Release)
REM    build.bat --install    also copy the plugin into the common VST3 folder
REM    build.bat --tests      build, then run the DSP self test and host test
REM
REM  JUCE 8.0.4 is picked up from %JUCE_PATH% when set, otherwise CMake
REM  downloads it automatically (needs git in PATH).
REM ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

if "%BUILD_DIR%"=="" set "BUILD_DIR=Build"
if "%CONFIG%"==""    set "CONFIG=Release"

set "JUCE_ARG="
if not "%JUCE_PATH%"=="" set "JUCE_ARG=-DJUCE_PATH=%JUCE_PATH%"

echo ==^> configuring (%CONFIG%)
cmake -S . -B "%BUILD_DIR%" %JUCE_ARG%
if errorlevel 1 goto :error

echo ==^> building
cmake --build "%BUILD_DIR%" --config %CONFIG% --parallel
if errorlevel 1 goto :error

set "VST3=%BUILD_DIR%\Acid303_artefacts\%CONFIG%\VST3\Acid303.vst3"
echo.
echo ==^> built:
echo     %VST3%
echo     %BUILD_DIR%\%CONFIG%\tb303_render.exe

if /i "%~1"=="--install" (
    set "DEST=%CommonProgramFiles%\VST3"
    if not exist "!DEST!" mkdir "!DEST!"
    if exist "!DEST!\Acid303.vst3" rmdir /s /q "!DEST!\Acid303.vst3"
    xcopy /e /i /y "%VST3%" "!DEST!\Acid303.vst3" >nul
    echo ==^> installed to !DEST!
)

if /i "%~1"=="--tests" (
    echo.
    echo ==^> DSP self test
    "%BUILD_DIR%\%CONFIG%\tb303_render.exe" selftest
    echo ==^> VST3 host test
    "%BUILD_DIR%\tb303_hosttest_artefacts\%CONFIG%\tb303_hosttest.exe" "%VST3%" "%~dp0check.wav" 6 130
)

goto :eof

:error
echo.
echo *** BUILD FAILED ***
exit /b 1
