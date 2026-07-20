@echo off
REM Sync the HOST install's saves into the JOIN install so both clients can load
REM the *identical* save. This is required for handle-mapped NPC replication:
REM the same NPC only shares a `hand` across machines when both load the same
REM serialized save.
REM
REM Usage:
REM   scripts\sync_save.cmd ["C:\path\to\source Kenshi"] ["C:\path\to\Kenshi-Join"] [safe|mirror]
REM
REM Copy mode (3rd argument):
REM   safe   (DEFAULT) - robocopy /E: additive copy. Adds new + updates changed
REM                      files but NEVER deletes anything in the destination that
REM                      is missing from the source. This is the safe default so a
REM                      mis-pointed -Sync can never wipe a save that only exists
REM                      on the destination.
REM   mirror           - robocopy /MIR: destructive exact mirror. PURGES files in
REM                      the destination that are not in the source. OPT-IN only,
REM                      and guarded by a pre-flight check that ABORTS (rather than
REM                      silently deleting) if any destination-only file is found.
REM
REM Run this BEFORE launching, any time you've made/updated the save you want to
REM co-op on. Close both Kenshi instances first so save files aren't mid-write.
setlocal EnableDelayedExpansion

set "SRC=%~1"
if "%SRC%"=="" set "SRC=C:\Program Files (x86)\Steam\steamapps\common\Kenshi"

set "DST=%~2"
if "%DST%"=="" set "DST=%USERPROFILE%\Kenshi-Join"

REM Copy mode: default to the non-destructive additive copy.
set "MODE=%~3"
if "%MODE%"=="" set "MODE=safe"

if not exist "%SRC%\save" (
    echo ERROR: no save folder at "%SRC%\save".
    exit /b 1
)
if not exist "%DST%\kenshi_x64.exe" (
    echo ERROR: join install not found at "%DST%". Run setup_join_install.cmd first.
    exit /b 1
)

if /I "%MODE%"=="mirror" goto :mirror

REM ---- SAFE mode (default): additive /E copy, never deletes ----
echo Copying saves - safe additive /E, no deletes:
echo   from: %SRC%\save
echo   to:   %DST%\save
echo.
REM /E copies subdirs incl. empty ones and updates changed files but leaves any
REM destination-only files untouched - nothing is ever deleted.
robocopy "%SRC%\save" "%DST%\save" /E /R:1 /W:1 /NFL /NDL /NP /NJH /NJS
goto :checkrc

:mirror
REM ---- MIRROR mode (opt-in, DESTRUCTIVE): exact /MIR, purges dest-only files ----
REM Detect destination-only files via robocopy's EXIT CODE, whose bit 2 means
REM "extra files/dirs exist in the destination". This is locale-INDEPENDENT;
REM grepping the "*EXTRA" text tag is NOT, because robocopy localizes it (e.g.
REM "*Archivo EXTRA" on a Spanish OS). A /L run only lists, it never touches disk.
robocopy "%SRC%\save" "%DST%\save" /L /MIR /R:0 /W:0 /NP /NJH /NJS >nul
set "PRECHECK=%ERRORLEVEL%"
set /a "EXTRA_BIT=PRECHECK & 2"
if %EXTRA_BIT% GEQ 2 (
    echo ERROR: destructive mirror aborted - the destination has files/folders that
    echo        do NOT exist in the source and /MIR would PERMANENTLY DELETE them.
    echo        Pending changes below - the EXTRA-tagged entries are the ones /MIR deletes:
    echo.
    robocopy "%SRC%\save" "%DST%\save" /L /MIR /R:0 /W:0 /NP /NJH /NJS
    echo.
    echo        Review the list above. Re-run with the safe default sync, or delete
    echo        those files by hand if you truly want an exact mirror.
    exit /b 1
)
echo Mirroring saves - DESTRUCTIVE /MIR:
echo   from: %SRC%\save
echo   to:   %DST%\save
echo.
robocopy "%SRC%\save" "%DST%\save" /MIR /R:1 /W:1 /NFL /NDL /NP /NJH /NJS
goto :checkrc

:checkrc
set "RC=%ERRORLEVEL%"
if %RC% GEQ 8 (
    echo ERROR: robocopy failed with code %RC%.
    exit /b %RC%
)

echo.
echo Saves synced. Load the SAME save name in both the host and join windows.
endlocal
