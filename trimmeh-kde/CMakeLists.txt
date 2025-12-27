cmake_minimum_required(VERSION 3.24)

project(trimmeh-kde LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Qml Widgets)
find_package(KF6StatusNotifierItem REQUIRED)

find_program(NPX npx)
if (NOT NPX)
    message(FATAL_ERROR "npx not found. Install Node.js (with npm/npx) to bundle trimmeh-core-js.")
endif()

set(CORE_TS "${CMAKE_CURRENT_LIST_DIR}/../trimmeh-core-js/src/kde-entry.ts")
set(CORE_JS "${CMAKE_CURRENT_BINARY_DIR}/trimmeh-core.js")

add_custom_command(
    OUTPUT "${CORE_JS}"
    COMMAND "${NPX}" esbuild "${CORE_TS}" --bundle --format=iife --platform=neutral --outfile="${CORE_JS}"
    DEPENDS "${CORE_TS}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
    COMMENT "Bundling trimmeh-core-js for KDE"
)

add_custom_target(trimmeh_core_bundle DEPENDS "${CORE_JS}")

add_executable(trimmeh-kde-probe
    src/main.cpp
)

target_link_libraries(trimmeh-kde-probe PRIVATE Qt6::Core Qt6::DBus)

add_executable(trimmeh-kde
    src/app_main.cpp
    src/clipboard_watcher.cpp
    src/clipboard_watcher.h
    src/klipper_bridge.cpp
    src/klipper_bridge.h
    src/preferences_dialog.cpp
    src/preferences_dialog.h
    src/settings.h
    src/tray_app.cpp
    src/tray_app.h
    src/trim_core.cpp
    src/trim_core.h
)

add_dependencies(trimmeh-kde trimmeh_core_bundle)

target_link_libraries(trimmeh-kde PRIVATE Qt6::Core Qt6::DBus Qt6::Qml Qt6::Widgets KF6::StatusNotifierItem)

add_custom_command(TARGET trimmeh-kde POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CORE_JS}" $<TARGET_FILE_DIR:trimmeh-kde>/trimmeh-core.js
    COMMENT "Copying trimmeh-core.js next to trimmeh-kde"
)
