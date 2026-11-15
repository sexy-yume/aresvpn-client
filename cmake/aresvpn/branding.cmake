# AresVPN Client - the product identity, applied through upstream's own branding seam.
#
# This is a CMake CACHE PRELOAD script: pass it as `cmake -C cmake/aresvpn/branding.cmake ...`
# (deploy/aresvpn/build_windows.cmd and build_android.sh do). Every CLIENT_* variable below is read by
# client/cmake/branding/*.cmake behind an `if(NOT CLIENT_...)` guard, so a value present in the
# cache before the project runs wins and NO upstream file is edited for it. That is the whole
# point: upstream added this seam in b1a37b37 ("feat: add brandings"), and a fork that sets the
# values here merges upstream's releases without conflict on any of them.
#
# Identifiers carry no spaces, deliberately - APPLICATION_NAME is also a settings scope, a log
# file name, an .ovpn file name and a registry key; the human-readable "AresVPN Client" lives in
# the UI strings and the version resources. Amnezia's names and marks are not licensed by the GPL
# (AresProject #D177, #D178); everything the seam does NOT reach is in the patch series beside it.

set(CLIENT_TARGET_NAME        "AresVPNClient"          CACHE STRING "Client executable target name")
set(CLIENT_APPLICATION_NAME   "AresVPNClient"          CACHE STRING "Application display and executable name")
set(CLIENT_SERVICE_NAME       "AresVPNClient-service"  CACHE STRING "Service executable name")
set(CLIENT_ORGANIZATION_NAME  "AresVPN"                CACHE STRING "QSettings organization name")
set(CLIENT_APP_INSTANCE_NAME  "AresVPNClientInstance"  CACHE STRING "Single-instance local server name")
set(CLIENT_KEYCHAIN_NAME      "AresVPNClient-Keychain" CACHE STRING "QtKeychain service name used for encrypted settings keys")
set(CLIENT_ANDROID_PACKAGE    "org.aresvpn.client"     CACHE STRING "Android package name for Play Store version lookup")
# The AresVPN console the client logs in to (POST /api/profile). This is the compiled default; a
# `Conf/aresEndpoint` value in the settings store overrides it, and no screen writes one - it is set
# by hand, for a test console (AresProject #D142, #D173).
set(CLIENT_ARES_ENDPOINT      "https://console.ares-vpn.org:8080" CACHE STRING "AresVPN console base URL")

# -- version: the fork's OWN, carried by every build that loads this preload (audit H3-2) --------
#
# `<base>.<build>`. The base is set here by hand and moves only when the product says so. The build
# is the fork's commit count, `git rev-list --count HEAD`: every commit on `dev` builds a newer
# version than the one before it, a rebuild of one commit carries the same one, and the Windows and
# Android builds of one commit carry the same one - the value the console's update manifest is
# judged against (`aresSession::judgeManifest`, AresProject #D268). The base starts above upstream's
# 5.0.2.0, which every build before this one carried, so a manifest can require those to update.
#
# Android's versionCode is upstream's base code plus the same count (APP_ANDROID_VERSION_CODE_OFFSET,
# the root CMakeLists' own seam), so it rises with every commit and stays above every APK that
# carried upstream's 2159: Android refuses to install a lower versionCode over a higher one.
#
# FORCE, because a build directory keeps its cache and a reconfigure after a new commit would
# otherwise carry the old count; both drivers configure on every run. The count needs the full
# history, which both drivers build from (the working copy; the container's full local clone).
set(ARES_CLIENT_VERSION_BASE "6.0.0")
execute_process(
    COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
    RESULT_VARIABLE _ares_git_rc
    OUTPUT_VARIABLE _ares_build
    ERROR_VARIABLE _ares_git_err
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _ares_git_rc EQUAL 0)
    message(FATAL_ERROR
        "AresVPN branding: the build number is the fork's commit count, and\n"
        "`git rev-list --count HEAD` failed (${_ares_git_rc}): ${_ares_git_err}\n"
        "Build from a git clone of the fork, with git on PATH.")
endif()
set(AMNEZIAVPN_VERSION "${ARES_CLIENT_VERSION_BASE}.${_ares_build}"
    CACHE STRING "Client app version: the AresVPN base and the fork's commit count" FORCE)
set(APP_ANDROID_VERSION_CODE_OFFSET "${_ares_build}"
    CACHE STRING "Added to upstream's Android versionCode: the fork's commit count" FORCE)
message(STATUS "AresVPN version ${AMNEZIAVPN_VERSION} (Android versionCode: upstream's base + ${_ares_build})")

# -- translations: DERIVED, never edited in place (AresProject ROADMAP 18-3b mechanism 3, #D180) --
#
# 599 translated strings across ten languages carried Amnezia's marks, and the GPL licenses their
# code and not their name. `client/translations/` is upstream's and churns (104k lines in four
# months), so it is READ here and never written: derive-translations.py writes
# `aresvpnclient_<locale>.ts` into deploy/aresvpn/generated/translations and CLIENT_TS_FILES points
# the build at those. Upstream's ten files stay byte-identical and merge clean for ever.
#
# This runs in the PRELOAD rather than in one build script on purpose: client/CMakeLists.txt reads
# CLIENT_TS_FILES at configure time, so generating here means EVERY entry point gets the derived
# files - the fork's build_windows.cmd, upstream's deploy/build.bat with this preload, or a bare
# cmake -C. A mechanism that only works when you remember to run a script first is the shape this
# project keeps paying for.
#
# AND IT FAILS CLOSED. If python is missing or the derivation errors, this stops the configure with
# the command to run. The alternative - falling back to upstream's prefix - would silently build a
# binary that says Amnezia in ten languages, and would look exactly like a successful build.
set(CLIENT_TS_PREFIX "aresvpnclient" CACHE STRING "Translation filename prefix")

set(_ares_ts_src "${CMAKE_CURRENT_LIST_DIR}/../../client/translations")
set(_ares_ts_out "${CMAKE_CURRENT_LIST_DIR}/../../deploy/aresvpn/generated/translations")
set(_ares_ts_gen "${CMAKE_CURRENT_LIST_DIR}/derive-translations.py")
# AresVPN Client (18-3h): the derivation rewrites Amnezia's marks inside translations that already
# exist; it cannot invent an entry for a string upstream never had, and the rebuilt screens are full
# of those. `cmake/aresvpn/translations/<locale>.ts` carries OUR strings and is appended to the
# derived file for that locale. Measured before it existed: of 93 unique qsTr strings on the eight
# screens this fork owns, 22 were Korean and 71 were English - visible on one screen at a time.

find_program(_ares_python NAMES python3 python py)
if(NOT _ares_python)
    message(FATAL_ERROR
        "AresVPN branding: no python on PATH, so the derived .ts files cannot be produced.\n"
        "Refusing rather than falling back to upstream's translations, which still carry\n"
        "Amnezia's marks in ten languages. Install python, or run by hand:\n"
        "  python ${_ares_ts_gen} --in ${_ares_ts_src} --out ${_ares_ts_out}")
endif()

execute_process(
    COMMAND "${_ares_python}" "${_ares_ts_gen}" --in "${_ares_ts_src}" --out "${_ares_ts_out}"
            --overlay "${CMAKE_CURRENT_LIST_DIR}/translations"
    RESULT_VARIABLE _ares_ts_rc
    OUTPUT_VARIABLE _ares_ts_out_text
    ERROR_VARIABLE _ares_ts_err_text)
if(NOT _ares_ts_rc EQUAL 0)
    message(FATAL_ERROR
        "AresVPN branding: deriving the translations FAILED (rc=${_ares_ts_rc}).\n"
        "${_ares_ts_out_text}\n${_ares_ts_err_text}")
endif()
message(STATUS "AresVPN translations derived:\n${_ares_ts_out_text}")

set(_ares_ts_files "")
foreach(_loc ru_RU zh_CN fa_IR ar_EG my_MM uk_UA ur_PK hi_IN es_ES ko_KR)
    set(_f "${_ares_ts_out}/aresvpnclient_${_loc}.ts")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "AresVPN branding: ${_f} was not produced - refusing to build with "
                            "upstream's translations, which still carry Amnezia's marks.")
    endif()
    list(APPEND _ares_ts_files "${_f}")
endforeach()
set(CLIENT_TS_FILES "${_ares_ts_files}" CACHE STRING "Derived AresVPN translation files")
