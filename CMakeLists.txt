cmake_minimum_required(VERSION 3.16)
# The release version. Bump it in the staging -> main pull request; scripts/ci/version.sh reads it.
project(freecell VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Release builds (CI) check GitHub for new versions; builds from source don't.
option(FREECELL_UPDATER "Check GitHub releases for updates" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
if(FREECELL_UPDATER)
    find_package(Qt6 REQUIRED COMPONENTS Network)
endif()

add_library(fccore STATIC src/game.cpp src/game.h)
target_include_directories(fccore PUBLIC src)

# Touch layout: an app bar instead of the menu bar, and a letterboxed board.
# On by default for Android; turn it on elsewhere to try the layout.
if(ANDROID)
    set(touch_default ON)
else()
    set(touch_default OFF)
endif()
option(FREECELL_TOUCH_UI "Use the touch-screen layout" ${touch_default})
if(FREECELL_TOUCH_UI AND FREECELL_UPDATER)
    message(FATAL_ERROR "FREECELL_UPDATER is desktop only (it cannot replace an installed APK); turn it off for the touch layout")
endif()
if(FREECELL_TOUCH_UI AND Qt6_VERSION VERSION_LESS 6.9)
    message(FATAL_ERROR "FREECELL_TOUCH_UI needs Qt 6.9 or later (safe area margins)")
endif()

set(freecell_sources
    src/main.cpp
    src/mainwindow.cpp src/mainwindow.h
    src/board.cpp src/board_paint.cpp src/board.h src/board_consts.h
    src/stats.cpp src/stats.h
    src/dialogs.cpp src/dialogs.h
    assets/cards.qrc
)
if(FREECELL_TOUCH_UI)
    list(APPEND freecell_sources src/touchui.cpp src/touchui.h assets/touch.qrc)
endif()

if(ANDROID)
    # On Android the app is a shared library that androiddeployqt packages.
    qt_policy(SET QTP0002 NEW)
    qt_add_executable(freecell ${freecell_sources})
else()
    add_executable(freecell WIN32 MACOSX_BUNDLE ${freecell_sources})
endif()
target_link_libraries(freecell PRIVATE fccore Qt6::Widgets Qt6::Svg)
target_compile_definitions(freecell PRIVATE FREECELL_VERSION="${PROJECT_VERSION}")
if(FREECELL_UPDATER)
    target_sources(freecell PRIVATE src/updater.cpp src/updater.h)
    target_compile_definitions(freecell PRIVATE FREECELL_UPDATER)
    target_link_libraries(freecell PRIVATE Qt6::Network)
endif()
if(FREECELL_TOUCH_UI)
    target_compile_definitions(freecell PRIVATE FREECELL_TOUCH_UI)
endif()

if(APPLE)
    set(icns ${CMAKE_SOURCE_DIR}/packaging/macos/FreeCell.icns)
    target_sources(freecell PRIVATE ${icns})
    set_source_files_properties(${icns} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(freecell PROPERTIES
        OUTPUT_NAME FreeCell
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.jpsutton.freecell
        MACOSX_BUNDLE_BUNDLE_NAME FreeCell
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE FreeCell.icns)
elseif(WIN32)
    target_sources(freecell PRIVATE packaging/windows/freecell.rc)
    set_target_properties(freecell PROPERTIES OUTPUT_NAME FreeCell)
elseif(ANDROID)
    # versionCode must grow with every release: 1.2.3 -> 10203.
    math(EXPR version_code
        "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")
    set_target_properties(freecell PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/packaging/android
        QT_ANDROID_PACKAGE_NAME io.github.jpsutton.freecell
        QT_ANDROID_APP_NAME FreeCell
        QT_ANDROID_APP_ICON @mipmap/ic_launcher
        QT_ANDROID_VERSION_NAME ${PROJECT_VERSION}
        QT_ANDROID_VERSION_CODE ${version_code}
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36)
endif()

# Android packages the app itself, and the logic tests do not run on the device.
if(NOT ANDROID)
    include(GNUInstallDirs)
    install(TARGETS freecell
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION .)
    if(UNIX AND NOT APPLE)
        install(FILES packaging/linux/io.github.jpsutton.freecell.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(FILES packaging/icons/freecell-256.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME io.github.jpsutton.freecell.png)
    endif()

    add_executable(gametest tests/gametest.cpp)
    target_link_libraries(gametest PRIVATE fccore)
    enable_testing()
    add_test(NAME gametest COMMAND gametest)
endif()
