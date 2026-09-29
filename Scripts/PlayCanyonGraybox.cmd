@echo off
setlocal
set "CANYON_EDITOR=D:\epic\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe"
set "CANYON_PROJECT=%~dp0..\TreasureSketch.uproject"
if not exist "%CANYON_EDITOR%" (
    echo Unreal Editor was not found at "%CANYON_EDITOR%".
    pause
    exit /b 1
)
"%CANYON_EDITOR%" "%CANYON_PROJECT%" -game -CanyonGrayboxPreview -Windowed -ResX=1280 -ResY=720 %*
