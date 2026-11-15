cmake_minimum_required(VERSION 3.16)
project(BatteryInspector VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Qml Quick DBus Test)

set(APP_ID "batteryinspector.sprainbrains")
set(APP_BINARY "batteryinspector")
set(TRANSLATION_DOMAIN "${APP_ID}")
set(ARCH "$ENV{ARCH}" CACHE STRING "Target architecture")
if(NOT ARCH)
  set(ARCH "amd64")
endif()
set(CLICK_FRAMEWORK "$ENV{CLICK_FRAMEWORK}" CACHE STRING "Click framework")
if(NOT CLICK_FRAMEWORK)
  set(CLICK_FRAMEWORK "ubuntu-touch-24.04-1.x")
endif()
set(APPARMOR_POLICY "$ENV{APPARMOR_POLICY}" CACHE STRING "AppArmor policy version")
if(NOT APPARMOR_POLICY)
  set(APPARMOR_POLICY "2404.1")
endif()

configure_file(manifest.json.in ${CMAKE_CURRENT_BINARY_DIR}/manifest.json @ONLY)
configure_file(batteryinspector.apparmor.in ${CMAKE_CURRENT_BINARY_DIR}/batteryinspector.apparmor @ONLY)
configure_file(batteryinspector.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/batteryinspector.desktop COPYONLY)

add_library(batteryinspector_core STATIC
  src/batteryprovider.cpp
  src/history.cpp
  src/metrics.cpp
  src/reportwriter.cpp
  src/sysfsreader.cpp
  src/upowersource.cpp
)
target_link_libraries(batteryinspector_core PUBLIC Qt5::Core Qt5::DBus)
target_include_directories(batteryinspector_core PUBLIC src)

qt5_add_resources(QML_RESOURCES qml/qml.qrc)
add_executable(${APP_BINARY} src/main.cpp ${QML_RESOURCES})
target_link_libraries(${APP_BINARY} PRIVATE Qt5::Gui Qt5::Qml Qt5::Quick batteryinspector_core)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(TARGETS ${APP_BINARY} RUNTIME DESTINATION .)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
              ${CMAKE_CURRENT_BINARY_DIR}/batteryinspector.apparmor
              ${CMAKE_CURRENT_BINARY_DIR}/batteryinspector.desktop
        DESTINATION .)
install(DIRECTORY assets DESTINATION .)
