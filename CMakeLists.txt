cmake_minimum_required(VERSION 3.8)
project(dovecot_virtual_keywords_plugin)

set(CMAKE_C_STANDARD 99)

INCLUDE_DIRECTORIES("/usr/include/dovecot")

set(SOURCE_FILES virtual-keywords-plugin.c virtual-keywords-plugin.h)
add_library(virtual_keywords_plugin SHARED ${SOURCE_FILES})
