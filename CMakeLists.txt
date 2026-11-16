cmake_minimum_required(VERSION 3.25)

# macOS only: the Permissions window comes from Rudder's PermissionsPanel on
# eacp, both fetched by lockstep. Windows and Linux stay a plain command line
# with no dependencies.
if (CMAKE_HOST_APPLE)
    set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/lockstep.cmake)
endif ()

project(ScreenCap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
    set(platformSource Source/Capture-macOS.mm Source/VideoEncoder-macOS.mm Source/Responsibility-macOS.cpp
            Source/Permissions-macOS.cpp Source/Setup-macOS.cpp)

    lockstep_add_package(
            NAME eacp
            GITHUB_REPOSITORY eyalamirmusic/eacp
            GIT_TAG develop
            SYSTEM YES
            OPTIONS
                "EACP_ENABLE_EXAMPLES OFF"
                "EACP_ENABLE_TESTS OFF")

    # Work against a local Rudder checkout with:
    #   cmake -B build -DLOCKSTEP_Rudder_SOURCE=$HOME/projects/Rudder
    lockstep_add_package(
            NAME Rudder
            GITHUB_REPOSITORY jamierpond/Rudder
            GIT_TAG main
            OPTIONS "RUDDER_CLIENT_ONLY ON")
elseif (WIN32)
    set(platformSource Source/Capture-Windows.cpp Source/VideoEncoder-Windows.cpp Source/Permissions-Default.cpp
            Source/DesktopSession-Windows.cpp Source/Setup-Windows.cpp)
else ()
    set(platformSource Source/Capture-Linux.cpp Source/VideoEncoder-Linux.cpp Source/Permissions-Default.cpp
            Source/DesktopSession-Linux.cpp Source/Setup-Linux.cpp Source/SetupTestPattern-Linux.cpp)
endif ()

add_executable(screencap Source/Main.cpp Source/Capture.cpp Source/Recorder.cpp ${platformSource})

# A copy of Rudder's: Windows and Linux builds don't fetch Rudder.
include(cmake/PondVersion.cmake)
pond_add_version_library(ScreenCapVersion)
target_link_libraries(screencap PRIVATE ScreenCapVersion)
target_include_directories(screencap SYSTEM PRIVATE ThirdParty/stb)

if (APPLE)
    # A bundle, so macOS privacy grants (TCC) and tccutil know screencap by its bundle id.
    set_target_properties(screencap PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME ScreenCap
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Info.plist
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    # build/screencap stays the command line, as on Windows and Linux.
    add_custom_command(TARGET screencap POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink ScreenCap.app/Contents/MacOS/ScreenCap ${CMAKE_BINARY_DIR}/screencap)
    target_compile_options(screencap PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(screencap PRIVATE
            "-framework Foundation" "-framework CoreGraphics" "-framework ScreenCaptureKit"
            "-framework AVFoundation" "-framework CoreVideo" "-framework CoreMedia"
            PermissionsPanel eacp-ui)
elseif (WIN32)
    target_compile_definitions(screencap PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(screencap PRIVATE user32 gdi32 dwmapi shcore ole32 mfplat mfreadwrite mfuuid wtsapi32)
else ()
    find_package(X11 REQUIRED)
    # X11::X11 carries only the directory holding X11/X.h. Under nix that is xorgproto, and Xlib.h lives in libx11.
    target_include_directories(screencap SYSTEM PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(screencap PRIVATE X11::X11)

    # `screencap setup` runs this script, built into the binary because the installed screencap is one file.
    set(linuxSetupScriptPath ${CMAKE_CURRENT_SOURCE_DIR}/scripts/setup-linux-virtual-desktop.sh)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${linuxSetupScriptPath})
    file(READ ${linuxSetupScriptPath} linuxSetupScript)
    file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/generated/LinuxSetupScript.h @ONLY CONTENT
            "#pragma once\n\nnamespace screencap\n{\nconstexpr auto linuxSetupScript = R\"setup_script(@linuxSetupScript@)setup_script\";\n}\n")
    target_include_directories(screencap PRIVATE ${CMAKE_BINARY_DIR}/generated)
endif ()
