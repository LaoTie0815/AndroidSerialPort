cmake_minimum_required(VERSION 3.4.1)

include_directories(${CMAKE_SOURCE_DIR}/src)
#include_directories(${CMAKE_SOURCE_DIR})
add_definitions(-std=c++14)
add_definitions(-O0)
#set(CMAKE_C_COMPILER "clang")
#set(CMAKE_CXX_COMPILER "clang++")

IF (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Windows")
    ADD_DEFINITIONS(-DWindows)
ELSE (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
    ADD_DEFINITIONS(-DLinux)
ENDIF ()

add_library( # Sets the name of the library.
        serial

        # Sets the library as a shared library.
        SHARED

        src/mtc.cpp
        src/mtc.h
        src/mid_exceptions.cpp
        src/mid_exceptions.h
        src/JNIEnvPtr.cpp
        src/definition.h
        src/serial.cpp
        src/tlv.cpp
        src/Utils.cpp
        jni.cpp
        mtc_log.h
        )

find_library( # Sets the name of the path variable.
        log-lib

        log)


target_link_libraries( # Specifies the target library.
        serial

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})