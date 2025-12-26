cmake_minimum_required(VERSION 3.24)

project(trimmeh-kde LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus)

add_executable(trimmeh-kde-probe
    src/main.cpp
)

target_link_libraries(trimmeh-kde-probe PRIVATE Qt6::Core Qt6::DBus)

