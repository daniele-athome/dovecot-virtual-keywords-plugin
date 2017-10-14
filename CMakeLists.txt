cmake_minimum_required(VERSION 3.7)
project(dovecot_virtual_keywords_plugin)

set(CMAKE_C_STANDARD 99)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

FIND_PACKAGE(Dovecot 2.2 REQUIRED)
INCLUDE_DIRECTORIES(${DOVECOT_INCLUDE_DIRS})

set(SOURCE_FILES virtual-keywords-plugin.c virtual-keywords-plugin.h)
add_library(virtual_keywords_plugin SHARED ${SOURCE_FILES})
