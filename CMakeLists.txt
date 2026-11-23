cmake_minimum_required(VERSION 3.21)
project(PIDStudio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)

add_library(pid_core STATIC src/controlengine.cpp)
target_include_directories(pid_core PUBLIC src)
set_target_properties(pid_core PROPERTIES AUTOMOC OFF)

add_library(pid_io STATIC src/csv.cpp src/experimentplan.cpp
    src/csvmonitor.cpp src/sessionlogger.cpp)
target_include_directories(pid_io PUBLIC src)
target_link_libraries(pid_io PUBLIC pid_core Qt6::Core)

add_library(pid_ui STATIC src/mainwindow.cpp src/trendplot.cpp)
target_link_libraries(pid_ui PUBLIC pid_io Qt6::Widgets)

foreach(target pid_core pid_io pid_ui)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

qt_add_executable(PIDStudio WIN32 src/main.cpp resources.qrc)
target_link_libraries(PIDStudio PRIVATE pid_ui)
install(TARGETS PIDStudio RUNTIME DESTINATION bin BUNDLE DESTINATION .)
install(DIRECTORY examples DESTINATION share/PIDStudio)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE pid_core)
    set_target_properties(test_core PROPERTIES AUTOMOC OFF)
    add_test(NAME control_engine COMMAND test_core)

    qt_add_executable(test_io tests/test_io.cpp resources.qrc)
    target_link_libraries(test_io PRIVATE pid_io Qt6::Test)
    add_test(NAME csv_and_files COMMAND test_io)

    qt_add_executable(test_gui tests/test_gui.cpp resources.qrc)
    target_link_libraries(test_gui PRIVATE pid_ui Qt6::Test)
    add_test(NAME demo_workflow COMMAND test_gui)
    if(NOT WIN32)
        set_tests_properties(demo_workflow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
