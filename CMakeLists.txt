cmake_minimum_required(VERSION 3.25)

project(ScreenCap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
    set(platformSource Source/Capture-macOS.mm Source/VideoEncoder-macOS.mm Source/Responsibility-macOS.cpp)
elseif (WIN32)
    set(platformSource Source/Capture-Windows.cpp Source/VideoEncoder-Windows.cpp)
else ()
    set(platformSource Source/Capture-Linux.cpp Source/VideoEncoder-Linux.cpp)
endif ()

add_executable(screencap Source/Main.cpp Source/Capture.cpp Source/Recorder.cpp Source/Permissions.cpp ${platformSource})
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
            "-framework AVFoundation" "-framework CoreVideo" "-framework CoreMedia")
elseif (WIN32)
    target_compile_definitions(screencap PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(screencap PRIVATE user32 gdi32 dwmapi shcore ole32 mfplat mfreadwrite mfuuid)
else ()
    find_package(X11 REQUIRED)
    target_link_libraries(screencap PRIVATE X11::X11)
endif ()
