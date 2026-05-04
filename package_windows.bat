@echo off
setlocal enabledelayedexpansion

cmake -S . -B build-rmp -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build-rmp --config Release
if errorlevel 1 exit /b 1

cpack --config build-rmp\CPackConfig.cmake -G ZIP
if errorlevel 1 exit /b 1

cpack --config build-rmp\CPackConfig.cmake -G NSIS
if errorlevel 1 (
	echo NSIS not available: skipped .exe installer generation.
)

echo Windows packages generated in build-rmp\\
