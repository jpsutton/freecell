cmake_minimum_required(VERSION 3.16)
# The release version. Bump it in the staging -> main pull request; scripts/ci/version.sh reads it.
project(freecell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_library(fccore STATIC src/game.cpp src/game.h)
target_include_directories(fccore PUBLIC src)

add_executable(freecell WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp src/mainwindow.h
    src/board.cpp src/board_paint.cpp src/board.h src/board_consts.h
    src/stats.cpp src/stats.h
    assets/cards.qrc
)
target_link_libraries(freecell PRIVATE fccore Qt6::Widgets Qt6::Svg)
target_compile_definitions(freecell PRIVATE FREECELL_VERSION="${PROJECT_VERSION}")

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
endif()

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
