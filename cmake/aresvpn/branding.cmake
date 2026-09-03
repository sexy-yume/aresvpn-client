# AresVPN Client - the product identity, applied through upstream's own branding seam.
#
# This is a CMake CACHE PRELOAD script: pass it as `cmake -C cmake/aresvpn/branding.cmake ...`
# (deploy/aresvpn/build-windows.cmd does). Every CLIENT_* variable below is read by
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
# The AresVPN console the client logs in to (POST /api/profile). Overridable at runtime through
# Conf/aresEndpoint (the dev menu); this is the compiled default (AresProject #D142, #D173).
set(CLIENT_ARES_ENDPOINT      "https://console.ares-vpn.org:8080" CACHE STRING "AresVPN console base URL")

# Translations keep upstream's prefix until the derived-.ts mechanism exists (AresProject ROADMAP
# 18-3b, survey 8.8 mechanism 3): the runtime looks up `<prefix>_<locale>.qm`, and the files under
# client/translations/ are upstream's and are never edited in place.
# set(CLIENT_TS_PREFIX "aresvpnclient" CACHE STRING "Translation filename prefix")
