cmake_minimum_required(VERSION 3.16)
project(ressetter-chip VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

# --- Ядро приложения: логика, адаптеры, утилиты (без GUI) ---
add_library(ressetter_core STATIC
    src/utils/Logger.cpp
    src/utils/FileUtils.cpp
    src/adapters/Ch341AI2cAdapter.cpp
    src/adapters/AdapterFactory.cpp
    src/core/ChipProfile.cpp
    src/core/ProfileManager.cpp
    src/core/DumpManager.cpp
    src/core/Checksum.cpp
    src/core/CounterReset.cpp
    src/core/Validator.cpp
    src/core/ChipWorker.cpp
)

target_include_directories(ressetter_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ressetter_core PUBLIC Qt6::Core PkgConfig::LIBUSB)

# --- GUI-приложение ---
add_executable(ressetter-chip
    src/main.cpp
    src/gui/MainWindow.cpp
    src/gui/ConnectionPanel.cpp
    src/gui/CartridgePanel.cpp
    src/gui/OperationsPanel.cpp
    src/gui/HexEditor.cpp
    src/gui/LogPanel.cpp
    src/gui/StatusBar.cpp
)

target_link_libraries(ressetter-chip PRIVATE ressetter_core Qt6::Widgets)

# Версия приложения приходит из project(), чтобы не дублировать её в коде
target_compile_definitions(ressetter-chip PRIVATE
    RESSETTER_CHIP_VERSION="${PROJECT_VERSION}")

install(TARGETS ressetter-chip RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/ressetter-chip.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/99-ch341a.rules
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/udev/rules.d)
install(FILES packaging/ressetter-chip.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(DIRECTORY src/profiles/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ressetter-chip/profiles
        FILES_MATCHING PATTERN "*.json")
install(FILES README.md CHANGELOG.md
        DESTINATION share/doc/ressetter-chip)

option(BUILD_TESTS "Сборка модульных тестов" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
