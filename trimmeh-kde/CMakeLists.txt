cmake_minimum_required(VERSION 3.22)

project(trimmeh-kde VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)

find_program(NPX_EXECUTABLE npx REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml)

# KDE Frameworks 6:
# - Some environments provide a meta "KF6" package with COMPONENTS (as referenced by KDE API docs).
# - Fedora packages typically provide per-framework CMake packages (e.g. KF6Config, KF6StatusNotifierItem)
#   and do *not* ship a KF6Config.cmake meta-package.
find_package(KF6 QUIET COMPONENTS StatusNotifierItem Config)
if (NOT KF6_FOUND)
    find_package(KF6StatusNotifierItem REQUIRED)
    find_package(KF6Config REQUIRED)
endif()

set(TRIMMEH_CORE_JS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../trimmeh-core-js/src/index.ts")
set(TRIMMEH_CORE_JS_BUNDLE "${CMAKE_CURRENT_BINARY_DIR}/trimmehCore.bundle.js")

add_custom_command(
    OUTPUT "${TRIMMEH_CORE_JS_BUNDLE}"
    COMMAND "${NPX_EXECUTABLE}" esbuild "${TRIMMEH_CORE_JS_SRC}"
            --bundle
            --format=iife
            --global-name=TrimmehCore
            --platform=neutral
            --target=es2020
            "--outfile=${TRIMMEH_CORE_JS_BUNDLE}"
    DEPENDS "${TRIMMEH_CORE_JS_SRC}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    COMMENT "Bundling trimmeh-core-js for QJSEngine"
    VERBATIM
)

set_source_files_properties("${TRIMMEH_CORE_JS_BUNDLE}" PROPERTIES GENERATED TRUE)

qt_add_executable(trimmeh-kde
    src/main.cpp
    src/trimmeh_app.cpp
    src/trimmeh_app.h
    src/trimmeh_clipboard_watcher.cpp
    src/trimmeh_clipboard_watcher.h
    src/trimmeh_js_core.cpp
    src/trimmeh_js_core.h
    src/trimmeh_settings.cpp
    src/trimmeh_settings.h
    src/trimmeh_vectors_check.cpp
    src/trimmeh_vectors_check.h
)

qt_add_resources(trimmeh-kde "trimmeh-core-js"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES
        "${TRIMMEH_CORE_JS_BUNDLE}"
)

target_link_libraries(trimmeh-kde PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    KF6::StatusNotifierItem
    KF6::ConfigCore
)
