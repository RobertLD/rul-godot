cmake_minimum_required(VERSION 3.21)
project(rul_godot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RULGD_BUILD_GODOT "Build the Godot GDExtension" OFF)
option(BUILD_TESTING "Build tests" ON)
include(FetchContent)

FetchContent_Declare(rul
    GIT_REPOSITORY https://github.com/RobertLD/rul.git
    GIT_TAG        v0.0.2
)
FetchContent_MakeAvailable(rul)

add_library(rul_wfc INTERFACE)
add_library(rul::wfc ALIAS rul_wfc)
target_include_directories(rul_wfc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(rul_wfc INTERFACE rul::rul)

if(RULGD_BUILD_GODOT)
    add_subdirectory(godot)
endif()


if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
