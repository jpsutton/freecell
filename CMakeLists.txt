cmake_minimum_required(VERSION 3.16)
project(freecell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_library(fccore STATIC src/game.cpp src/game.h)
target_include_directories(fccore PUBLIC src)

add_executable(freecell
    src/main.cpp
    src/mainwindow.cpp src/mainwindow.h
    src/board.cpp src/board_paint.cpp src/board.h src/board_consts.h
    src/stats.cpp src/stats.h
    assets/cards.qrc
)
target_link_libraries(freecell PRIVATE fccore Qt6::Widgets Qt6::Svg)

add_executable(gametest tests/gametest.cpp)
target_link_libraries(gametest PRIVATE fccore)
enable_testing()
add_test(NAME gametest COMMAND gametest)
