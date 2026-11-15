cmake_minimum_required(VERSION 3.25)

project(ScreenCap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
    set(platformSource Source/Capture-macOS.mm)
elseif (WIN32)
    set(platformSource Source/Capture-Windows.cpp)
else ()
    set(platformSource Source/Capture-Linux.cpp)
endif ()

add_executable(screencap Source/Main.cpp Source/Capture.cpp ${platformSource})
target_include_directories(screencap SYSTEM PRIVATE ThirdParty/stb)

if (APPLE)
    target_compile_options(screencap PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(screencap PRIVATE
            "-framework Foundation" "-framework CoreGraphics" "-framework ScreenCaptureKit")
elseif (WIN32)
    target_compile_definitions(screencap PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(screencap PRIVATE user32 gdi32 dwmapi shcore)
else ()
    find_package(X11 REQUIRED)
    target_link_libraries(screencap PRIVATE X11::X11)
endif ()
