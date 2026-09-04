@echo off
:: AresVPN Client - the Windows INSTALLER (AresProject ROADMAP 18-3d/18-3h, #D189).
::
:: The sibling of deploy\aresvpn\build_windows.cmd, which deliberately says "(Release, no
:: installer)": that script produces two executables and a relocatable tree, and a folder is not a
:: product. This one packages an EXISTING build directory with CPack's IFW generator.
::
:: WHY A SEPARATE SCRIPT rather than a flag on the build driver: packaging re-runs windeployqt over
:: a staging copy and takes minutes with no compilation in it, and the two questions are different -
:: "does it build" and "does it install". Upstream's deploy\build.bat couples them behind
:: ARG_BUILD_INSTALLERS; keeping them apart means a packaging failure cannot be read as a build one.
::
:: NOT a patch to deploy\build.bat: #D177 rule 3. Every upstream file this fork edits is a merge
:: conflict for ever, and cmake\CPack.cmake is already fork-owned for the branding.
::
:: NOTHING PRODUCED HERE MAY BE CONVEYED. #D178 makes the source public no later than the first
:: binary leaves our hands, and the repository is still private; there is also no signing identity,
:: so this installer and the binaries inside it are UNSIGNED and Windows will say so loudly.
::
:: Usage:  deploy\aresvpn\package_windows.cmd
:: Env:    QIF_ROOT_PATH, VCVARS_PATH, BUILD_DIR override the defaults below.
setlocal
:: The tree is derived from this script's own location, never from a home directory (#L012).
set "ROOT=%~dp0..\.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
if not defined BUILD_DIR set "BUILD_DIR=%ROOT%\deploy\build"
if not defined QIF_ROOT_PATH set "QIF_ROOT_PATH=C:\Qt\Tools\QtInstallerFramework\4.7"
if not defined VCVARS_PATH set "VCVARS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

if not exist "%QIF_ROOT_PATH%\bin\binarycreator.exe" (
    echo FATAL: no Qt Installer Framework at "%QIF_ROOT_PATH%".
    echo        CPACK_GENERATOR is IFW and cpack cannot bake one without binarycreator. Install with:
    echo          python -m aqt install-tool windows desktop tools_ifw qt.tools.ifw.47 -O C:\Qt
    echo        or set QIF_ROOT_PATH to an existing install.
    exit /b 2
)
if not exist "%VCVARS_PATH%" ( echo FATAL: no vcvarsall.bat at "%VCVARS_PATH%". Set VCVARS_PATH. & exit /b 2 )
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo FATAL: "%BUILD_DIR%" is not a configured build directory. Run deploy\aresvpn\build_windows.cmd first.
    exit /b 2
)
:: The build must be OURS. An unbranded build here would package upstream's product under our
:: driver's name, which is the failure deploy\aresvpn\build_android.sh exists to refuse (#L020).
if not exist "%BUILD_DIR%\client\Release\AresVPNClient.exe" (
    echo FATAL: no AresVPNClient.exe in "%BUILD_DIR%\client\Release".
    echo        Either the build has not run, or it ran without -C cmake\aresvpn\branding.cmake.
    exit /b 2
)

:: Strawberry's cmake is first on PATH and is not the one that configured this tree; vcvars puts
:: VS's cmake/cpack in front (ARCHITECTURE.md section 9).
set "PATH=%PATH:C:\Strawberry\c\bin;=%"
call "%VCVARS_PATH%" amd64 >nul || exit /b 4

cd /d "%BUILD_DIR%" || exit /b 9
echo root:      %ROOT%
echo build:     %BUILD_DIR%
echo qtifw:     %QIF_ROOT_PATH%
echo START %DATE% %TIME%
cpack -G IFW -D "QTIFWDIR=%QIF_ROOT_PATH%" || goto :fail

:: ============================================================================================
:: THE ARTEFACT IS ASSERTED, NOT THE EXIT CODE (#L012, #L019). cpack exits 0 on a package that is
:: missing things a customer would notice, and the licence check below is here because the FIRST
:: installer ever built from this tree had no licence page at all (#D189).
:: ============================================================================================
set "RC=0"
set "PKG="
for %%F in ("%BUILD_DIR%\AresVPNClient_*_windows_x64.exe") do set "PKG=%%~fF"
if not defined PKG (
    echo FAIL: cpack reported success and no AresVPNClient_*_windows_x64.exe exists.
    set "RC=4"
    goto :report
)
for %%F in ("%PKG%") do echo installer: %%~nxF  %%~zF bytes

:: The generated IFW tree, which is where the metadata a customer reads actually lives.
set "IFWDIR="
for /d %%D in ("%BUILD_DIR%\_CPack_Packages\win64\IFW\AresVPNClient_*_windows_x64") do set "IFWDIR=%%~fD"
if not defined IFWDIR (
    echo FAIL: no _CPack_Packages IFW tree to inspect - the assertions below cannot run.
    set "RC=4"
    goto :report
)

findstr /c:"<Licenses>" "%IFWDIR%\packages\AresVPNClient\meta\package.xml" >nul
if errorlevel 1 (
    echo FAIL: the installer presents NO LICENCE PAGE - meta\package.xml has no ^<Licenses^> element.
    echo       CPackIFW ignores CPACK_RESOURCE_FILE_LICENSE; the component needs a LICENSES argument
    echo       in cmake\CPack.cmake. This is the defect #D189 was opened by.
    set "RC=5"
) else (
    echo ok: the installer presents a licence page
)

if not exist "%IFWDIR%\packages\AresVPNClient\data\LICENSE" (
    echo FAIL: the payload does not carry LICENSE - GPL-3 requires the text to be reproduced ^(#D184^).
    set "RC=5"
) else (
    for %%F in ("%IFWDIR%\packages\AresVPNClient\data\LICENSE") do echo ok: payload LICENSE %%~zF bytes
)
for %%N in (THIRD_PARTY_LICENSES.md OFL-PT-Root-UI.txt wintun-prebuilt-binaries-license.txt) do (
    if not exist "%IFWDIR%\packages\AresVPNClient\data\%%N" (
        echo FAIL: the payload does not carry %%N ^(#D184, #D180 rule 4, #D175^).
        set "RC=5"
    )
)

:: The brand, read out of the installer's own bytes rather than off its filename (#L019).
findstr /m /c:"AmneziaVPN" "%PKG%" >nul
if errorlevel 1 (
    echo ok: no AmneziaVPN string in the installer
) else (
    echo NOTE: the installer's bytes still carry "AmneziaVPN" - name the occurrences before conveying.
)

:report
echo END %DATE% %TIME% RC=%RC%
if "%RC%"=="0" ( echo PACKAGE-DONE ) else ( echo PACKAGE-FAILED RC=%RC% )
echo REMINDER: unsigned, and #D178 forbids handing this to anyone while the repository is private.
exit /b %RC%

:fail
echo END %DATE% %TIME% RC=1
echo PACKAGE-FAILED RC=1
exit /b 1
