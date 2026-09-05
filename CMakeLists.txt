cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SnapHeater_U1)
target_compile_options(__idf_main PRIVATE -Wall -Wextra -Werror)
