cmake_minimum_required(VERSION 3.25.0 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/clientBrandingDefaults.cmake)

set(PROJECT ${CLIENT_TARGET_NAME})
project(${PROJECT})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER "Autogen")
set_property(GLOBAL PROPERTY AUTOMOC_TARGETS_FOLDER "Autogen")
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "Autogen")

set(PACKAGES
    Core Gui Network Xml
    RemoteObjects Quick Svg QuickControls2
    Core5Compat Concurrent LinguistTools
    GuiPrivate  # AresVPN Client: QWindowSystemInterface, for --qml-drive (18-3h)
)

execute_process(
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMAND git rev-parse --short HEAD
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions(-DGIT_COMMIT_HASH="${GIT_COMMIT_HASH}")

add_definitions(-DPROD_AGW_PUBLIC_KEY="$ENV{PROD_AGW_PUBLIC_KEY}")
add_definitions(-DPROD_S3_ENDPOINT="$ENV{PROD_S3_ENDPOINT}")
add_definitions(-DFALLBACK_S3_ENDPOINT="$ENV{FALLBACK_S3_ENDPOINT}")

add_definitions(-DDEV_AGW_PUBLIC_KEY="$ENV{DEV_AGW_PUBLIC_KEY}")
add_definitions(-DDEV_AGW_ENDPOINT="$ENV{DEV_AGW_ENDPOINT}")
add_definitions(-DDEV_S3_ENDPOINT="$ENV{DEV_S3_ENDPOINT}")

add_definitions(-DFREE_V2_ENDPOINT="$ENV{FREE_V2_ENDPOINT}")
add_definitions(-DPREM_V1_ENDPOINT="$ENV{PREM_V1_ENDPOINT}")

if(WIN32 OR (APPLE AND NOT IOS) OR (LINUX AND NOT ANDROID))
    set(PACKAGES ${PACKAGES} Widgets)
endif()

find_package(Qt6 REQUIRED COMPONENTS ${PACKAGES})
find_package(OpenSSL REQUIRED)

set(LIBS ${LIBS}
    Qt6::Core Qt6::Gui
    Qt6::Network Qt6::Xml Qt6::RemoteObjects
    Qt6::Quick Qt6::Svg Qt6::QuickControls2
    Qt6::Core5Compat Qt6::Concurrent
    Qt6::GuiPrivate  # AresVPN Client: QWindowSystemInterface, for --qml-drive (18-3h)
)

if(WIN32 OR (APPLE AND NOT IOS) OR (LINUX AND NOT ANDROID))
    set(LIBS ${LIBS} Qt6::Widgets)
endif()

qt_standard_project_setup()
qt_add_executable(${PROJECT} MANUAL_FINALIZATION)
target_include_directories(${PROJECT} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
target_compile_definitions(${PROJECT} PRIVATE
    CLIENT_KEYCHAIN_NAME_STR=\"${CLIENT_KEYCHAIN_NAME}\"
)

if(WIN32 OR (APPLE AND NOT IOS AND NOT MACOS_NE) OR (LINUX AND NOT ANDROID))
    qt_add_repc_replicas(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/../ipc/ipc_interface.rep)
    qt_add_repc_replicas(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/../ipc/ipc_process_interface.rep)
endif()

qt6_add_resources(QRC ${QRC}
    ${CMAKE_CURRENT_LIST_DIR}/images/images.qrc
    ${CMAKE_CURRENT_LIST_DIR}/images/flagKit.qrc
    ${CMAKE_CURRENT_LIST_DIR}/client_scripts/clientScripts.qrc
    # AresVPN Client (18-3d): the licence texts are embedded as well as installed. Installing them
    # discharges the duty to REPRODUCE them; GPL-3 section 5's Appropriate Legal Notice has to be
    # displayable BY THE RUNNING PROGRAM, and a path on disk is not that on Android.
    ${CMAKE_CURRENT_LIST_DIR}/licenses.qrc
    ${CLIENT_QML_QRC_FILE}
    # AresVPN Client: server_scripts/serverScripts.qrc is not embedded - the self-hosted install flow
    # is unreachable (see ServersUiController::hasServerWithWriteAccess); the directory stays for merges.
    ${CLIENT_EXTRA_QRC}
)

# -- i18n begin
if(CLIENT_TS_FILES)
    set(AMNEZIAVPN_TS_FILES ${CLIENT_TS_FILES})
else()
    set(AMNEZIAVPN_TS_FILES
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_ru_RU.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_zh_CN.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_fa_IR.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_ar_EG.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_my_MM.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_uk_UA.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_ur_PK.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_hi_IN.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_es_ES.ts
        ${CMAKE_CURRENT_LIST_DIR}/translations/${CLIENT_TS_PREFIX}_ko_KR.ts
    )
endif()

file(GLOB_RECURSE AMNEZIAVPN_QML_FILES ${CMAKE_CURRENT_LIST_DIR}/ui/qml/*.qml)

qt6_add_translations(${PROJECT}
    TS_FILES ${AMNEZIAVPN_TS_FILES}
    SOURCES ${AMNEZIAVPN_QML_FILES}
    RESOURCE_PREFIX "/translations"
)
# -- i18n end

set(IS_CI ${CI})
if(IS_CI)
    message("Detected CI env")
    find_program(CCACHE "ccache")
    if(CCACHE)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE}")
    endif()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/3rdparty.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/sources.cmake)

set(_client_include_dirs
    ${CMAKE_CURRENT_LIST_DIR}/../ipc
    ${CMAKE_CURRENT_LIST_DIR}/../common/logger
    ${CMAKE_CURRENT_LIST_DIR}/../common/crypto
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
if(CLIENT_BRANDING_INCLUDE_DIR)
    list(APPEND _client_include_dirs ${CLIENT_BRANDING_INCLUDE_DIR})
endif()
list(APPEND _client_include_dirs ${CMAKE_CURRENT_LIST_DIR})
include_directories(${_client_include_dirs})

if(MACOS_NE)
    message("MACOS_NE is ON")
    add_definitions(-DQ_OS_MAC)
    add_definitions(-DMACOS_NE)
    message("Add macros for MacOS Network Extension")
else()
    message("MACOS_NE is OFF")
endif()

include_directories(mozilla)
include_directories(mozilla/shared)
include_directories(mozilla/models)

configure_file(${CMAKE_CURRENT_LIST_DIR}/../version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/core/utils)
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/core/utils/appUiConfig.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/core/utils/appUiConfig.h
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/core/utils/swiftBridgeConfig.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/core/utils/swiftBridgeConfig.h
    @ONLY
)
# Swift can't consume a `-DGROUP_ID="value"` compile define (Swift -D flags are
# presence-only), unlike the Objective-C++ sources that read GROUP_ID directly.
# Generate a Swift constant instead so Log.swift can use the branded app group.
set(CLIENT_GROUP_IDENTIFIER_SWIFT_FILE ${CMAKE_CURRENT_BINARY_DIR}/platforms/ios/GroupIdentifier.swift)
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/platforms/ios/GroupIdentifier.swift.in
    ${CLIENT_GROUP_IDENTIFIER_SWIFT_FILE}
    @ONLY
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT} PRIVATE "MZ_DEBUG")
endif()

if(WIN32)
    configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/platforms/windows/amneziavpn.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/amneziavpn.rc
    )

    set(LIBS ${LIBS}
        user32
        rasapi32
        shlwapi
        iphlpapi
        ws2_32
        gdi32
    )

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

if(APPLE)
    cmake_policy(SET CMP0099 NEW)
    cmake_policy(SET CMP0114 NEW)


    set(CMAKE_XCODE_GENERATE_SCHEME FALSE)
    set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${BUILD_VPN_DEVELOPMENT_TEAM})
    set(CMAKE_XCODE_ATTRIBUTE_GROUP_ID_IOS ${BUILD_IOS_GROUP_IDENTIFIER})
    
    if (BUILD_VPN_KEYCHAIN)
        set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--keychain ${BUILD_VPN_KEYCHAIN}")
    endif()
endif()

if(LINUX AND NOT ANDROID)
    set(LIBS ${LIBS} -static-libstdc++ -static-libgcc -ldl)
    link_directories(${CMAKE_CURRENT_LIST_DIR}/platforms/linux)
    set(LIBS ${LIBS} -Wl,--no-as-needed $<TARGET_LINKER_FILE:Qt6::RemoteObjects> -Wl,--as-needed)
endif()

if(WIN32 OR (APPLE AND NOT IOS AND NOT MACOS_NE) OR (LINUX AND NOT ANDROID))
    add_compile_definitions(AMNEZIA_DESKTOP)
endif()

if(ANDROID)
    include(cmake/android.cmake)
endif()

if(IOS)
    include(cmake/ios.cmake)
    include(cmake/ios-arch-fixup.cmake)
elseif(APPLE AND MACOS_NE)
    include(cmake/macos_ne.cmake)
elseif(APPLE)
    include(cmake/osxtools.cmake)
    include(cmake/macos.cmake)
endif()

list(APPEND SOURCES ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

target_link_libraries(${PROJECT} PRIVATE ${LIBS})
target_compile_definitions(${PROJECT} PRIVATE "MZ_$<UPPER_CASE:${MZ_PLATFORM_NAME}>")

target_sources(${PROJECT} PRIVATE ${SOURCES} ${HEADERS} ${RESOURCES} ${QRC} ${I18NQRC})

# Finalize the executable so Qt can gather/deploy QML modules and plugins correctly (Android needs this).
if(COMMAND qt_import_qml_plugins)
    qt_import_qml_plugins(${PROJECT})
endif()
if(COMMAND qt_finalize_executable)
    qt_finalize_executable(${PROJECT})
else()
    qt_finalize_target(${PROJECT})
endif()

install(TARGETS ${PROJECT}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RUNTIME_DEPENDENCY_SET client_deps
    COMPONENT AresVPNClient
)

# AresVPN Client (AresProject ROADMAP 18-3d): the licence texts ship WITH the binary. Nothing
# installed them before, so an installed tree carried a GPL-3 application and not one word of its
# licence, no third-party attribution, and neither of the two texts we are separately obliged to
# reproduce: the OFL for PT Root UI (#D180 rule 4 - an OFL.txt beside the font is a duty, not a
# courtesy) and the Wintun prebuilt-binaries EULA, which the recipe discards and which #D175
# records as binding whatever OUR licence is. The written offer of Corresponding Source is the
# repository, which the About screen already links.
install(FILES
    ${CMAKE_SOURCE_DIR}/LICENSE
    ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md
    ${CMAKE_SOURCE_DIR}/licenses/wintun-prebuilt-binaries-license.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/OFL-PT-Root-UI.txt
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT AresVPNClient
)

# AresVPN Client (AresProject ROADMAP 18-3d): *compliance as a MECHANISM, not a memory*.
#
# GPL-3 section 6 wants the Corresponding Source of every covered component we convey, and a link
# without a revision does not identify the code the shipped bytes came from. THIRD_PARTY_LICENSES.md
# carried that gap as a sentence for a human to remember - "the exact source revision of the shipped
# binary is not yet established" - which is the shape this project keeps paying for.
#
# So the manifest is GENERATED from the conan recipes that actually produced this build, installed
# beside the licence texts, and the generator REFUSES a source reference that moves. It runs here
# rather than in the preload because conan resolves during the configure above, and it is read
# rather than assumed: an empty scan is a failure, not a clean bill (#L041).
#
# It does not fail an ordinary build. `-DARES_RELEASE=ON` says *this build is intended to be
# conveyed*, and only then is an unpinned component fatal - an AUTHORISATION rather than a switch,
# the shape `--allow-dataplane` already uses in this project. As measured on 2026-09-04: Windows is
# clean; Android is not, because `openvpn-pt-android` is pinned to a BRANCH.
find_program(_ares_rev_python NAMES python3 python py)
set(_ares_rev_script "${CMAKE_SOURCE_DIR}/cmake/aresvpn/source-revisions.py")
set(_ares_rev_out "${CMAKE_BINARY_DIR}/SOURCE-REVISIONS.txt")
if(ANDROID)
    set(_ares_rev_os android)
elseif(WIN32)
    set(_ares_rev_os windows)
elseif(APPLE)
    set(_ares_rev_os apple)
else()
    set(_ares_rev_os all)
endif()

if(_ares_rev_python AND EXISTS "${_ares_rev_script}")
    execute_process(
        COMMAND "${_ares_rev_python}" "${_ares_rev_script}" --os "${_ares_rev_os}" --out "${_ares_rev_out}"
        RESULT_VARIABLE _ares_rev_rc
        OUTPUT_VARIABLE _ares_rev_out_text
        ERROR_VARIABLE _ares_rev_err_text
    )
    if(NOT _ares_rev_rc EQUAL 0)
        if(ARES_RELEASE)
            message(FATAL_ERROR
                "AresVPN source revisions: a component this ${_ares_rev_os} build conveys has a "
                "source reference that MOVES, so its Corresponding Source offer would not identify "
                "the code the binary was built from (GPL-3 s6).\n${_ares_rev_out_text}${_ares_rev_err_text}")
        else()
            message(WARNING
                "AresVPN source revisions: NOT release-clean for ${_ares_rev_os}. This build may not "
                "be conveyed to anyone. Re-run with -DARES_RELEASE=ON to make this fatal.\n"
                "${_ares_rev_out_text}${_ares_rev_err_text}")
        endif()
    endif()
    if(EXISTS "${_ares_rev_out}")
        install(FILES "${_ares_rev_out}"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT AresVPNClient
        )
    endif()
elseif(ARES_RELEASE)
    message(FATAL_ERROR "AresVPN source revisions: no python, so the Corresponding Source manifest "
                        "cannot be produced and this release build must not proceed.")
endif()

# ===========================================================================================
# DOES A SCREEN A CUSTOMER CAN REACH STILL CARRY AMNEZIA'S MARK? (AresProject #D190, 18-3b)
#
# `cmake/aresvpn/brand-check.py` has answered that since 18-3b and NOTHING RAN IT. It was a
# command to remember, and on 2026-09-04 `#D184` rebuilt the About screen, the notice's wording
# changed from "marks of their owners" to "are their marks", and the allow-list entry protecting
# that notice silently stopped matching. Nobody knew for a day. `#L013`'s update is the rule this
# repays: prefer a check that RUNS to a rule that must be read.
#
# Same authorisation shape as the source revisions above: a warning on an ordinary build, FATAL
# under `-DARES_RELEASE=ON`, because a build nobody conveys carrying a mark on a dead screen is a
# defect to fix and not a reason to stop working.
find_program(_ares_brand_python NAMES python3 python py)
set(_ares_brand_script "${CMAKE_SOURCE_DIR}/cmake/aresvpn/brand-check.py")
if(_ares_brand_python AND EXISTS "${_ares_brand_script}")
    execute_process(
        COMMAND "${_ares_brand_python}" "${_ares_brand_script}" --root "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _ares_brand_rc
        OUTPUT_VARIABLE _ares_brand_out
        ERROR_VARIABLE _ares_brand_err
    )
    if(NOT _ares_brand_rc EQUAL 0)
        if(ARES_RELEASE)
            message(FATAL_ERROR
                "AresVPN brand check: a screen a customer can REACH carries Amnezia's mark, or an "
                "entry in the deliberate-exceptions list matches nothing (which means it went "
                "stale, or the notice it permits has been deleted).\n${_ares_brand_out}${_ares_brand_err}")
        else()
            message(WARNING
                "AresVPN brand check: NOT clean. This build may not be conveyed. Re-run with "
                "-DARES_RELEASE=ON to make this fatal.\n${_ares_brand_out}${_ares_brand_err}")
        endif()
    endif()
elseif(ARES_RELEASE)
    message(FATAL_ERROR "AresVPN brand check: no python, so a release build cannot establish that "
                        "no reachable screen carries Amnezia's mark.")
endif()

if(APPLE)
    set(RUNTIME_DEPS_DIR ${CMAKE_INSTALL_BINDIR}/AmneziaVPN.app/Contents/Frameworks)
else()
    set(RUNTIME_DEPS_DIR ${CMAKE_INSTALL_BINDIR})
endif()

install(RUNTIME_DEPENDENCY_SET client_deps
    PRE_EXCLUDE_REGEXES
        [[api-ms-win-.*]]
        [[ext-ms-.*]]
        [[kernel32\.dll]]
        [[hvsifiletrust\.dll]]
        [[libc\.so\..*]] [[libgcc_s\.so\..*]] [[libm\.so\..*]] [[libstdc\+\+\.so\..*]]
        [[.*\.framework]]
        [[^(lib)?[Qq]t.*]]
    POST_EXCLUDE_REGEXES
        [[^.*[\\/]system32[\\/].*\.dll$]]
        [[^/lib.*]]
        [[^/usr/lib.*]]
    DIRECTORIES ${CONAN_RUNTIME_LIB_DIRS}
    COMPONENT AresVPNClient
    DESTINATION "${RUNTIME_DEPS_DIR}"
)

set(deploy_tool_options "")
if(WIN32)
    set(deploy_tool_options "--force-openssl --force")
endif()

qt_generate_deploy_qml_app_script(
    TARGET ${PROJECT}
    OUTPUT_SCRIPT QT_DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_TOOL_OPTIONS ${deploy_tool_options}
)
install(SCRIPT ${QT_DEPLOY_SCRIPT}
    COMPONENT AresVPNClient
)

if (APPLE AND NOT IOS AND NOT MACOS_NE)
    list(APPEND OVPN_SCRIPTS "${CMAKE_SOURCE_DIR}/deploy/data/macos/update-resolv-conf.sh")
    set_target_properties(${PROJECT} PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    )
endif()
if (LINUX AND NOT ANDROID)
    list(APPEND OVPN_SCRIPTS "${CMAKE_SOURCE_DIR}/deploy/data/linux/update-resolv-conf.sh")
endif()

if(OVPN_SCRIPTS)
    add_custom_command(TARGET ${PROJECT} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OVPN_SCRIPTS}
        "$<TARGET_FILE_DIR:${PROJECT}>"
    )

    install(FILES ${OVPN_SCRIPTS}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT AresVPNClient
        PERMISSIONS
            OWNER_READ OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )
endif()
