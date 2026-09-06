cmake_minimum_required(VERSION 3.16)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/main/app_config.h")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/main/app_config.h" SHU1_VERSION_LINE REGEX "^#define SHU1_FW_VERSION ")
string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" PROJECT_VER "${SHU1_VERSION_LINE}")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SnapHeater_U1)

# Applies to the parser implementation, including calls from managed components.
target_compile_definitions(__idf_json PRIVATE CJSON_NESTING_LIMIT=16)
target_compile_options(__idf_main PRIVATE -Wall -Wextra -Werror)
