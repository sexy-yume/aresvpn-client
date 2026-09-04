message("Client android ${CMAKE_ANDROID_ARCH_ABI} build")

if(NOT DEFINED APP_ANDROID_MIN_SDK)
    set(APP_ANDROID_MIN_SDK 28)
endif()

# Option to build Play variant (with Google Play Billing) instead of OSS
# When ON, adds target android_play_apk: cmake --build . --target android_play_apk
option(ANDROID_BUILD_PLAY "Add android_play_apk target for Google Play Billing build" OFF)
set(ANDROID_PLATFORM "android-${APP_ANDROID_MIN_SDK}" CACHE STRING
    "The minimum API level supported by the application or library" FORCE)

# set QTP0002 policy: target properties that specify Android-specific paths may contain generator expressions
qt_policy(SET QTP0002 NEW)

set_target_properties(${PROJECT} PROPERTIES
    QT_ANDROID_VERSION_NAME ${CMAKE_PROJECT_VERSION}
    QT_ANDROID_VERSION_CODE ${APP_ANDROID_VERSION_CODE}
    QT_ANDROID_MIN_SDK_VERSION ${APP_ANDROID_MIN_SDK}
    QT_ANDROID_TARGET_SDK_VERSION 36
    QT_ANDROID_SDK_BUILD_TOOLS_REVISION 36.0.0
)

set(QT_ANDROID_MULTI_ABI_FORWARD_VARS "QT_NO_GLOBAL_APK_TARGET_PART_OF_ALL;CMAKE_BUILD_TYPE")

# We need to include qtprivate api's
# As QAndroidBinder is not yet implemented with a public api
# Check if Qt6::CorePrivate is available (may not be in all Qt versions/configurations)
if(TARGET Qt6::CorePrivate)
    set(LIBS ${LIBS} Qt6::CorePrivate)
endif()
set(LIBS ${LIBS} -ljnigraphics)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/platforms/android)

set(HEADERS ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/android_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/android_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/protocols/androidVpnProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/utils/installedAppsImageProvider.h
)

set(SOURCES ${SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/android_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/android_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/protocols/androidVpnProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/utils/installedAppsImageProvider.cpp
)


find_package(awg-android REQUIRED)
set(LIBS ${LIBS} amnezia::awg-android)
set_property(TARGET ${PROJECT} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS ${AMNEZIA_ANDROID_LIBWG_PATH} ${AMNEZIA_ANDROID_LIBWG_QUICK_PATH})

find_package(amnezia-libxray REQUIRED)
file(COPY ${AMNEZIA_LIBXRAY_PATH} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/android/xray/libXray)

find_package(openvpn-pt-android REQUIRED)
set(LIBS ${LIBS} amnezia::openvpn-pt-android)
# AresVPN Client (AresProject #D192, ROADMAP 18-3e): DO NOT REMOVE THIS LINE. The Cloak plugin is
# a RUNTIME DEPENDENCY of the OpenVPN core we ship, and every source in this repository says
# otherwise.
#
# The standing claim - survey 7.5, `THIRD_PARTY_LICENSES.md`, and ROADMAP 18-3e's own open list -
# is that `libck-ovpn-plugin.so` is 7 424 088 B of GPL-3 Go that **nothing loads**, and it is
# well-argued: upstream's MVVM refactor deleted `Cloak.kt` (847bb692), the one remaining hook
# `OpenVpn.kt::configPluggableTransport` is overridden nowhere, `ContainerUtils::isUnsupportedContainer`
# returns true for `DockerContainer::Cloak`, and `connectionController.cpp` refuses it with
# `LegacyContainerNotSupportedError`. Every one of those is true. All of them are about the KOTLIN
# loader, and the plugin is not loaded by Kotlin.
#
#     readelf -d lib/arm64-v8a/libovpn3.so   ->   NEEDED   libck-ovpn-plugin.so
#
# `libovpn3.so` IS the OpenVPN 3 core this product runs on Android. Remove the plugin and it fails
# to load, which takes OpenVPN out of the APK entirely - and it would have looked like a clean 7 MB
# saving right up to the first customer who chose that protocol.
#
# HOW IT WAS FOUND, because the method is the point: gating this line changed the APK by ZERO bytes,
# the plugin was still at `lib/arm64-v8a/libck-ovpn-plugin.so`, and asking what else could put it
# there ended at the ELF rather than at a fourth theory (#L010). `androiddeployqt`'s dependency walk
# had been pulling it in all along, correctly. A grep for a loader answers "who calls it"; only the
# dynamic section answers "what needs it" (#L004: an inference is not a measurement, and four
# careful readings agreed with each other).
#
# So the GPL-3 duty is real and stays: this component IS conveyed, deliberately, and belongs in the
# notice as a shipped dependency rather than as something scheduled for removal.
set_property(TARGET ${PROJECT} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS ${OPENVPN_PT_ANDROID_LIBCK_OVPN_PLUGIN_PATH})
# !!! THIS IS NOT ENOUGH, AND THE APK SAYS SO. Measured after the change, on the branded artefact
# (#L019 - the commit that made it promised this check and it is why the gap is known):
#
#     lib/arm64-v8a/libck-ovpn-plugin.so   7 424 088 bytes   STILL PRESENT
#     APK 70 394 268 B, unchanged in size
#
# So `QT_ANDROID_EXTRA_LIBS` is not the only route into the package, and the second one has not been
# identified - `androiddeployqt`'s own dependency scan over the linked conan packages' lib folders
# is the leading candidate, since `amnezia::openvpn-pt-android` is in LIBS and its package holds the
# .so beside the ones we do use. The option above is correct for the channel it governs and
# INSUFFICIENT on its own; leaving it without this note would read as a solved problem, which is the
# defect class this fork keeps paying for (#L038).
#
# NEXT STEP for whoever picks this up: build with `--verbose` androiddeployqt output, or diff the
# generated `android-<target>-deployment-settings.json`, to name the second route before changing
# anything else. Dropping `ck_ovpn_plugin_go` from the recipe's build targets closes it at the
# source, at the cost of a fork-owned recipe edit - and #D191 already measured that this component
# publishes no prebuilt, so that edit costs no build time either.

set(APP_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android)

if(APP_ANDROID_MAX_SDK)
    set(APP_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/android-package-source)
    file(REMOVE_RECURSE ${APP_ANDROID_PACKAGE_SOURCE_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/android/ DESTINATION ${APP_ANDROID_PACKAGE_SOURCE_DIR})

    set(manifest_path ${APP_ANDROID_PACKAGE_SOURCE_DIR}/AndroidManifest.xml)
    set(manifest_anchor "android:installLocation=\"auto\">")
    file(READ ${manifest_path} manifest_contents)
    string(REPLACE
        "${manifest_anchor}"
        "${manifest_anchor}\n\n    <uses-sdk android:maxSdkVersion=\"${APP_ANDROID_MAX_SDK}\" />"
        patched_contents "${manifest_contents}")
    if(patched_contents STREQUAL manifest_contents)
        message(FATAL_ERROR
            "Failed to set maxSdkVersion=${APP_ANDROID_MAX_SDK}: anchor '${manifest_anchor}' "
            "not found in ${CMAKE_CURRENT_SOURCE_DIR}/android/AndroidManifest.xml")
    endif()
    file(WRITE ${manifest_path} "${patched_contents}")
endif()

set_property(TARGET ${PROJECT} PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR ${APP_ANDROID_PACKAGE_SOURCE_DIR})

if(QT_USE_TARGET_ANDROID_BUILD_DIR)
    set(_android_build_dir "${CMAKE_CURRENT_BINARY_DIR}/android-build-${PROJECT}")
else()
    set(_android_build_dir "${CMAKE_CURRENT_BINARY_DIR}/android-build")
endif()

add_custom_target(android_gradle_clean
    COMMAND ./gradlew clean
    WORKING_DIRECTORY "${_android_build_dir}"
    COMMENT "Cleaning Android Gradle build cache"
)

# Always-available debug target: build Play Debug APK and copy to standard output path
# so Qt Creator's deploy step picks it up automatically
add_custom_target(android_play_debug_install
    COMMAND ./gradlew assemblePlayDebug
    COMMAND sh -c "cp build/outputs/apk/play/debug/*.apk build/outputs/apk/android-build-${PROJECT}-debug.apk"
    WORKING_DIRECTORY "${_android_build_dir}"
    COMMENT "Building Android Play Debug APK and copying to deploy path"
    DEPENDS ${PROJECT}
)

if(ANDROID_BUILD_PLAY)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_gradle_suffix "Debug")
    else()
        set(_gradle_suffix "Release")
    endif()
    add_custom_target(android_play_apk
        COMMAND ./gradlew assemblePlay${_gradle_suffix}
        WORKING_DIRECTORY "${_android_build_dir}"
        COMMENT "Building Android Play APK (assemblePlay${_gradle_suffix})"
        DEPENDS ${PROJECT}
    )
    add_custom_target(android_play_aab
        COMMAND ./gradlew bundlePlay${_gradle_suffix}
        WORKING_DIRECTORY "${_android_build_dir}"
        COMMENT "Building Android Play AAB (bundlePlay${_gradle_suffix})"
        DEPENDS ${PROJECT}
    )
endif()
