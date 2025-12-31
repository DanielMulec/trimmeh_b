cmake_minimum_required(VERSION 3.24)

project(trimmeh-kde LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Qml Widgets)
find_package(KF6StatusNotifierItem REQUIRED)
find_package(KF6GlobalAccel REQUIRED)

find_program(ESBUILD esbuild)
find_program(NPX npx)
if (NOT ESBUILD AND NOT NPX)
    message(FATAL_ERROR "esbuild or npx not found. Install esbuild or Node.js (with npm/npx) to bundle trimmeh-core-js.")
endif()

set(CORE_TS "${CMAKE_CURRENT_LIST_DIR}/../trimmeh-core-js/src/kde-entry.ts")
set(CORE_JS "${CMAKE_CURRENT_BINARY_DIR}/trimmeh-core.js")

set(ESBUILD_COMMAND "")
set(ESBUILD_ARGS "")
if (ESBUILD)
    set(ESBUILD_COMMAND "${ESBUILD}")
else()
    set(ESBUILD_COMMAND "${NPX}")
    set(ESBUILD_ARGS esbuild)
endif()

add_custom_command(
    OUTPUT "${CORE_JS}"
    COMMAND ${ESBUILD_COMMAND} ${ESBUILD_ARGS} "${CORE_TS}" --bundle --format=iife --platform=neutral --outfile="${CORE_JS}"
    DEPENDS "${CORE_TS}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
    COMMENT "Bundling trimmeh-core-js for KDE"
)

add_custom_target(trimmeh_core_bundle DEPENDS "${CORE_JS}")

add_executable(trimmeh-kde-probe
    src/main.cpp
)

target_link_libraries(trimmeh-kde-probe PRIVATE Qt6::Core Qt6::DBus)

add_executable(trimmeh-kde-vectors
    src/vectors_runner.cpp
    src/trim_core.cpp
    src/trim_core.h
)

add_dependencies(trimmeh-kde-vectors trimmeh_core_bundle)

target_link_libraries(trimmeh-kde-vectors PRIVATE Qt6::Core Qt6::Qml)

add_custom_command(TARGET trimmeh-kde-vectors POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CORE_JS}" $<TARGET_FILE_DIR:trimmeh-kde-vectors>/trimmeh-core.js
    COMMENT "Copying trimmeh-core.js next to trimmeh-kde-vectors"
)

add_executable(trimmeh-kde
    src/app_main.cpp
    src/app_identity.cpp
    src/app_identity.h
    src/autostart_manager.cpp
    src/autostart_manager.h
    src/clipboard_watcher.cpp
    src/clipboard_watcher.h
    src/hotkey_manager.cpp
    src/hotkey_manager.h
    src/klipper_bridge.cpp
    src/klipper_bridge.h
    src/portal_paste_injector.cpp
    src/portal_paste_injector.h
    src/preferences_dialog.cpp
    src/preferences_dialog.h
    src/settings.h
    src/settings_store.cpp
    src/settings_store.h
    src/tray_app.cpp
    src/tray_app.h
    src/trim_core.cpp
    src/trim_core.h
)

add_dependencies(trimmeh-kde trimmeh_core_bundle)

target_link_libraries(trimmeh-kde PRIVATE Qt6::Core Qt6::DBus Qt6::Qml Qt6::Widgets KF6::StatusNotifierItem KF6::GlobalAccel)

add_custom_command(TARGET trimmeh-kde POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CORE_JS}" $<TARGET_FILE_DIR:trimmeh-kde>/trimmeh-core.js
    COMMENT "Copying trimmeh-core.js next to trimmeh-kde"
)

install(TARGETS trimmeh-kde RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${CORE_JS}" DESTINATION ${CMAKE_INSTALL_LIBDIR}/trimmeh)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/resources/dev.trimmeh.TrimmehKDE.desktop"
        DESTINATION share/applications)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/resources/dev.trimmeh.TrimmehKDE.metainfo.xml"
        DESTINATION share/metainfo)
