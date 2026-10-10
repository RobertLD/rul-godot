
set(GODOTCPP_SYSTEM_HEADERS ON)
if(NOT DEFINED GODOTCPP_API_VERSION)
    set(GODOTCPP_API_VERSION "4.7" CACHE STRING "")
endif()
if(NOT DEFINED GODOTCPP_BUILD_PROFILE)
    set(GODOTCPP_BUILD_PROFILE "${CMAKE_SOURCE_DIR}/build_profile.json" CACHE PATH "")
endif()

add_subdirectory(godot-cpp)

file(GLOB RUL_GODOT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)
add_library(rul_godot_ext SHARED)
target_sources(rul_godot_ext PRIVATE ${RUL_GODOT_SOURCES})
target_compile_features(rul_godot_ext PUBLIC cxx_std_20)
target_link_libraries(rul_godot_ext PRIVATE godot-cpp rul::wfc)
get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)

# target_doc_sources() wants a list of .xml files, not a directory; skip while empty.
# godot-cpp only find_package()s Python in its own scope, so repeat it here.
file(GLOB RUL_DOC_CLASSES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/doc_classes/*.xml")
if(RUL_DOC_CLASSES)
    find_package(Python3 3.4 REQUIRED)
    target_doc_sources(rul_godot_ext "${RUL_DOC_CLASSES}")
endif()

set_target_properties(rul_godot_ext PROPERTIES
    CXX_VISIBILITY_PRESET ${GODOTCPP_SYMBOL_VISIBILITY}
    POSITION_INDEPENDENT_CODE ON
    BUILD_RPATH_USE_ORIGIN ON
    PREFIX "lib"
    OUTPUT_NAME "rul${GODOTCPP_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/demo/addons/rul/bin>"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/demo/addons/rul/bin>"
)
