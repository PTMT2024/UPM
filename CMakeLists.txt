include(CMakePrintHelpers)
cmake_minimum_required(VERSION 3.5.1)
project(user_space_page_migration)

set(CMAKE_BUILD_TYPE Relase CACHE STRING "Set build type to Release by default" FORCE)
cmake_print_variables(CMAKE_BUILD_TYPE)

set(CMAKE_CXX_STANDARD 17)

IF(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(CMAKE_C_FLAGS "-lm -Wall -Wpedantic -Winline -Wextra -Wno-long-long -Werror -g -O0 -w -fPIC -mcmodel=medium -fsanitize=address -static-libasan -fno-omit-frame-pointer")
ELSE()
        set(CMAKE_C_FLAGS "-lm -Wall -Wpedantic -Winline -Wextra -Wno-long-long -Werror -g -O2 -w -fPIC -mcmodel=medium")

ENDIF()

#set(CMAKE_C_FLAGS "-g -w -O2 -fPIC")
# set(PCM_PATH "/home/sherry/projects/pcm")

include_directories(LFU-Cache)
include_directories(algorithm)
include_directories(parameters)
include_directories(/usr/local/include)
# include_directories(mini_pf_counter_lib/include)
# include_directories(${PCM_PATH}/src)

# link_directories(${PCM_PATH}/build/lib)
# link_directories(mini_pf_counter_lib/lib)


IF(CMAKE_BUILD_TYPE STREQUAL Debug)
        ADD_DEFINITIONS(-DDEBUG)
ENDIF()

add_executable(user_space_page_migration
        LFU-Cache/FrequencyList.cpp
        LFU-Cache/FrequencyList.h
        LFU-Cache/LFU.cpp
        LFU-Cache/LFU.h
        LFU-Cache/LFUTest.cpp
        LFU-Cache/timer.hpp
        algorithm/algorithm.h
        algorithm/algorithm.cpp
        parameters/parameters.h 
        parameters/parameters.c
        perf_profile/profile.c
        perf_profile/profile.h
        samples/samples.cpp
        samples/samples.h
        smaps/smaps.c
        smaps/smaps.h
        counting_bloom/counting_bloom.c
        counting_bloom/counting_bloom.h
        main.cpp
        common.h)


IF(CMAKE_BUILD_TYPE STREQUAL Debug)
        target_compile_options(user_space_page_migration PRIVATE -fsanitize=address)
        target_link_options(user_space_page_migration PRIVATE -fsanitize=address)
ENDIF()

target_link_libraries(user_space_page_migration m pthread dl numa pfm)


