# CMakeLists.txt for ESP-IDF framework
# Note: This file is only used when framework = espidf
# For Arduino framework, PlatformIO uses library.json and auto-discovery

get_filename_component(dir ${CMAKE_CURRENT_LIST_FILE} PATH)

# Collect all source files (now in root directory, not src/)
FILE(GLOB_RECURSE app_sources 
    ${dir}/*.cpp
    ${dir}/*/*.cpp
    ${dir}/*/*/*.cpp
)

idf_component_register(SRCS ${app_sources}      
                    INCLUDE_DIRS "."
                    REQUIRES wear_levelling
)
