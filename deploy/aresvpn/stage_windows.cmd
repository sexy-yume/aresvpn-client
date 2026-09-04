@echo off
:: AresVPN Client - refresh the RELOCATABLE STAGED TREE (AresProject #D190).
::
:: `HANDOFF.md` has said since 2026-09-04 that "build and stage are ONE step now, at the operator's
:: request" - and no script in either repository did it. The staging was `cmake --install` typed by
:: hand, so the sentence was a claim about a mechanism that did not exist, which is the defect class
:: this project punishes (`#L038`: a comment asserting something is handled is a claim, and claims
:: are checkable). This is the mechanism.
::
:: WHAT THE STAGE IS FOR, and why it is not the installer. `deploy/aresvpn/package_windows.cmd`
:: produces the thing a customer runs. This produces a FOLDER, and the folder is what the clean
:: Windows VM reads over a shared folder (`scripts/client/guest-test.ps1` in AresProject, ROADMAP
:: 18-3c) - no install, no service registration, no driver, so a test machine is not changed by
:: being measured (`#L023`).
::
:: IT REFUSES RATHER THAN KILLING A RUNNING CLIENT. `cmake --install` over a live executable fails
:: with a sharing violation halfway through and leaves a HALF-UPDATED tree, which is worse than not
:: staging at all: the next run measures a mixture of two builds (`#L017`, a change that
:: half-succeeds). So the client must not be running, and this says so by name.
::
:: Usage:  deploy\aresvpn\stage_windows.cmd  [stage dir]
setlocal
set "ROOT=%~dp0..\.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\deploy\build"
set "STAGE=%~1"
if "%STAGE%"=="" set "STAGE=%USERPROFILE%\Desktop\aresvpn-stage"

if not exist "%BUILD_DIR%\client\Release\AresVPNClient.exe" (
    echo FATAL: no AresVPNClient.exe in "%BUILD_DIR%\client\Release".
    echo        Run deploy\aresvpn\build_windows.cmd first - and with the branding preload, or the
    echo        build produced upstream's name and this would stage the wrong product.
    exit /b 2
)

:: A running client holds its own exe and its DLLs open, and staging over it leaves a tree that is
:: half one build and half another.
::
:: THE FIRST VERSION OF THIS GUARD DID NOT FIRE, and the mechanism is worth keeping. It was
::     tasklist /FI "IMAGENAME eq AresVPNClient.exe" | find /i "AresVPNClient.exe"
:: and `find` resolved to MSYS's find(1), not Windows's find.exe, whenever this script was invoked
:: from a Git Bash environment - so the pipeline failed with *No such file or directory*, the
:: errorlevel was the wrong command's, and the guard concluded NOT RUNNING while the client was
:: running with the stage's own exe open (`#L015`: a pipeline whose status belongs to another
:: command; ARCHITECTURE.md section 9's family of MSYS tools shadowing Windows ones). cmake then
:: failed halfway with *Permission denied*, which is the outcome the guard exists to prevent.
::
:: So it tests THE THING ITSELF and uses no external program: open the staged executable for
:: append and close it. No bytes are written; a locked file refuses. That cannot be shadowed by a
:: PATH, and it also catches a lock held by something that is not our process at all.
if exist "%STAGE%\AresVPNClient.exe" (
    (call ) 2>nul >>"%STAGE%\AresVPNClient.exe" || (
        echo FATAL: "%STAGE%\AresVPNClient.exe" is LOCKED - the client is running, or something else
        echo        holds it. Staging over it leaves a tree that is half one build and half another,
        echo        and every later measurement is of that mixture. Close the client and run this
        echo        again. NOTHING HAS BEEN CHANGED.
        exit /b 3
    )
)
if exist "%STAGE%\AresVPNClient-service.exe" (
    (call ) 2>nul >>"%STAGE%\AresVPNClient-service.exe" || (
        echo FATAL: "%STAGE%\AresVPNClient-service.exe" is LOCKED. Same reason. NOTHING HAS BEEN CHANGED.
        exit /b 3
    )
)

echo build:  %BUILD_DIR%
echo stage:  %STAGE%
cmake --install "%BUILD_DIR%" --config Release --prefix "%STAGE%" || goto :fail

:: THE ARTEFACT, not the exit code (#L012). cmake --install exits 0 having copied nothing if the
:: install rules changed underneath it, and the four licence texts are a GPL-3 duty (#D184) that a
:: staged tree owes exactly as an installed one does.
set "RC=0"
if not exist "%STAGE%\AresVPNClient.exe" ( echo FAIL: no AresVPNClient.exe in the stage & set "RC=4" )
if not exist "%STAGE%\AresVPNClient-service.exe" ( echo FAIL: no service exe in the stage & set "RC=4" )
for %%N in (LICENSE THIRD_PARTY_LICENSES.md OFL-PT-Root-UI.txt wintun-prebuilt-binaries-license.txt) do (
    if not exist "%STAGE%\%%N" ( echo FAIL: the stage does not carry %%N ^(#D184^) & set "RC=4" )
)
if exist "%STAGE%\AresVPNClient.exe" for %%F in ("%STAGE%\AresVPNClient.exe") do echo staged: AresVPNClient.exe %%~zF bytes  %%~tF

:: ORPHANS. `cmake --install` is ADDITIVE - it never deletes a file it has stopped installing - so a
:: stage accumulates whatever a previous build put there. Measured the day #D192 dropped the Xray
:: geo databases: both stages still held geoip.dat and geosite.dat, 30 329 347 B of a component the
:: product no longer ships, and the VM's copy is what a clean-machine test reads. That is #L061's
:: stale-artefact family - the previous run's output sitting where this run's is expected - and it
:: is worse in a stage than in a log, because the thing under test loads it.
::
:: NAMED, NOT DELETED. Deleting by a list of things we happen to remember dropping is how a stage
:: loses a file somebody added on purpose; and the install manifest that WOULD make this exact is
:: `%BUILD_DIR%\install_manifest.txt`, which lists what this build installed. Comparing the stage
:: against it is the right check and is the next thing to build here.
for %%N in (geoip.dat geosite.dat) do (
    if exist "%STAGE%\%%N" (
        echo NOTE: "%STAGE%\%%N" is an ORPHAN - #D192 stopped shipping it and cmake --install does
        echo       not delete. Remove it, or a clean-machine test measures a tree we do not ship.
    )
)
if "%RC%"=="0" ( echo STAGE-DONE ) else ( echo STAGE-FAILED RC=%RC% )
exit /b %RC%

:fail
echo STAGE-FAILED RC=1
exit /b 1
