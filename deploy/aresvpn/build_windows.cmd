@echo off
:: AresVPN Client - Windows build driver (the fork's own; upstream's deploy\build.bat is untouched).
::
:: Same cmake invocation as deploy\build.bat, plus the ONE thing that makes it AresVPN Client:
::   -C cmake\aresvpn\branding.cmake   (cache preload; see that file)
:: and the workstation facts measured while making the reference build green
:: (AresProject ARCHITECTURE.md section 9, ROADMAP 18-3a):
::   1. conan must run on Python >= 3.12 - it lives in a venv this script creates from PY314.
::   2. vswhere -latest can answer a Build Tools install with no vcvarsall.bat; VCVARS_PATH is pinned.
::   3. Strawberry's cmake defaults to the Ninja generator; it is dropped from PATH, VS's cmake is used.
::   4. A CP949 system code page makes OpenVPN's /WX fatal on a UTF-8 comment: CL=/utf-8.
::
:: Usage:  deploy\aresvpn\build_windows.cmd            (Release, no installer)
::         (an underscore, not a hyphen: upstream's .gitignore:29 `*build-*` would ignore this file)
:: Env:    ARES_CLIENT_VENV, PY314, VCVARS_PATH, QT_ROOT_PATH override the defaults below.
setlocal
set "ROOT=%~dp0..\.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\deploy\build"
if not defined PY314 set "PY314=%LOCALAPPDATA%\Programs\Python\Python314\python.exe"
if not defined ARES_CLIENT_VENV set "ARES_CLIENT_VENV=%LOCALAPPDATA%\ares-client-venv314"
if not defined VCVARS_PATH set "VCVARS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not defined QT_ROOT_PATH set "QT_ROOT_PATH=C:\Qt\6.10.1"

if not exist "%PY314%" ( echo FATAL: Python 3.14 not at "%PY314%" - conan needs ^>= 3.12. Set PY314. & exit /b 2 )
if not exist "%VCVARS_PATH%" ( echo FATAL: no vcvarsall.bat at "%VCVARS_PATH%". Set VCVARS_PATH. & exit /b 2 )
if not exist "%QT_ROOT_PATH%\msvc2022_64\lib\cmake\Qt6Core5Compat" (
    echo FATAL: Qt 6.10.x msvc2022_64 with qt5compat is not under "%QT_ROOT_PATH%". Install with:
    echo   python -m pip install aqtinstall==3.3.0
    echo   python -m aqt install-qt windows desktop 6.10.1 win64_msvc2022_64 -m qtremoteobjects qt5compat qtshadertools -O C:\Qt
    exit /b 2
)
if not exist "%ARES_CLIENT_VENV%\Scripts\conan.exe" (
    echo creating "%ARES_CLIENT_VENV%" with conan 2.28.0
    "%PY314%" -m venv "%ARES_CLIENT_VENV%" || exit /b 3
    "%ARES_CLIENT_VENV%\Scripts\python.exe" -m pip install -q "conan==2.28.0" || exit /b 3
)
set "PATH=%ARES_CLIENT_VENV%\Scripts;%PATH%"
set "PATH=%PATH:C:\Strawberry\c\bin;=%"
set "CL=/utf-8"
set "CMAKE_GENERATOR=Visual Studio 17 2022"
set "CMAKE_GENERATOR_PLATFORM=x64"
conan --version || exit /b 8

call "%VCVARS_PATH%" amd64 || exit /b 4
cd /d "%ROOT%" || exit /b 9
echo START %DATE% %TIME%  root=%ROOT%
cmake -S "%ROOT%" -B "%BUILD_DIR%" -C "%ROOT%\cmake\aresvpn\branding.cmake" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=%QT_ROOT_PATH%\msvc2022_64" "-DCMAKE_VS_GLOBALS=UseMultiToolTask=true;EnforceProcessCountAcrossBuilds=true" || goto :fail
cmake --build "%BUILD_DIR%" --config Release -- /m || goto :fail
set "RC=0"
if not exist "%BUILD_DIR%\client\Release\AresVPNClient.exe" ( echo FAIL: AresVPNClient.exe is ABSENT after a green build - the branding preload did not take & set "RC=7" )
if not exist "%BUILD_DIR%\service\server\Release\AresVPNClient-service.exe" ( echo FAIL: AresVPNClient-service.exe is ABSENT & set "RC=7" )
echo END %DATE% %TIME% RC=%RC%
dir /b "%BUILD_DIR%\client\Release\*.exe" "%BUILD_DIR%\service\server\Release\*.exe" 2>nul
echo BUILD-DONE RC=%RC%
exit /b %RC%

:fail
echo END %DATE% %TIME% RC=1
echo BUILD-DONE RC=1
exit /b 1
