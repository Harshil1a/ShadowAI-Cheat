cmake_minimum_required(VERSION 3.16)
project(ShadowAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set the executable name to a custom premium application name
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Workaround for macOS 14+ Xcode SDK removing legacy AGL framework
if(APPLE)
    if(NOT TARGET WrapOpenGL::WrapOpenGL)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Gui
    Multimedia
    MultimediaWidgets
)

# Windows-specific settings
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
endif()

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/overlaywindow.cpp
    src/overlaywindow.h
    src/settingswindow.cpp
    src/settingswindow.h
    src/screencapture.cpp
    src/screencapture.h
    src/aimanager.cpp
    src/aimanager.h
    src/hotkeymanager.cpp
    src/hotkeymanager.h
    src/trayicon.cpp
    src/trayicon.h
    src/dashboard.cpp
    src/dashboard.h
    src/appconfig.cpp
    src/appconfig.h
    src/audiorecorder.cpp
    src/audiorecorder.h
    src/accountmanager.cpp
    src/accountmanager.h
    resources/resources.qrc
)

if(WIN32)
    add_executable(RuntimeBroker WIN32 ${SOURCES})
else()
    add_executable(RuntimeBroker ${SOURCES})
endif()

# Set binary output to legitimate Windows/Mac background service
set_target_properties(RuntimeBroker PROPERTIES OUTPUT_NAME "RuntimeBroker")

if(APPLE)
    set_target_properties(RuntimeBroker PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "AudioService"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.apple.audio.service"
    )
endif()

target_link_libraries(RuntimeBroker PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Gui
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# Windows-specific libraries for hotkeys and screenshots
if(WIN32)
    target_link_libraries(RuntimeBroker PRIVATE
        user32
        gdi32
        dwmapi
        ole32
    )
endif()

target_include_directories(RuntimeBroker PRIVATE src)

# Installation
install(TARGETS RuntimeBroker DESTINATION bin)
