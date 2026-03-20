cmake_minimum_required(VERSION 3.20)
project(NumPadHotkeys VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(NumPadHotkeys WIN32
    src/main.cpp
    src/hook.cpp
    src/tray.cpp
    src/config.cpp
    src/ui_config.cpp
    res/NumPadHotkeys.rc
)

target_include_directories(NumPadHotkeys PRIVATE src)

target_compile_definitions(NumPadHotkeys PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(NumPadHotkeys PRIVATE
    user32
    shell32
    comctl32
    ole32
    gdi32
    comdlg32
)

if(MSVC)
    target_compile_options(NumPadHotkeys PRIVATE /O2 /W4 /WX /wd4100 /wd4505)
    # /wd4100 = unreferenced formal parameter
    # /wd4505 = unreferenced local function
    set_target_properties(NumPadHotkeys PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
else()
    target_compile_options(NumPadHotkeys PRIVATE
        -O2 -Wall -Wextra
        -Wno-unused-parameter
        -Wno-unused-function
    )
    target_link_options(NumPadHotkeys PRIVATE
        -mwindows
        -static-libgcc
        -static-libstdc++
    )
endif()
