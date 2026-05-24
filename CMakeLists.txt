cmake_minimum_required(VERSION 3.16)
project(ProjectAQtBoard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    src/main.cpp
    src/gui/mainwindow.h
    src/gui/mainwindow.cpp
    src/gui/gamewidget.h
    src/gui/gamewidget.cpp
    src/core/board.h
    src/core/board.cpp
    src/core/bench.h
    src/core/bench.cpp
    src/core/shop.h
    src/core/shop.cpp
    src/core/unit.h
    src/core/unit.cpp
    src/core/unit_factory.h
    src/core/unit_factory.cpp
    src/core/player.h
    src/core/player.cpp
    src/core/equipment.h
    src/core/equipment.cpp
    src/core/equipment_inventory.h
    src/core/equipment_inventory.cpp
    src/core/equipment_factory.h
    src/core/equipment_factory.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus)
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
    src
    src/gui
    src/core
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
