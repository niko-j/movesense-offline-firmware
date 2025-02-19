cmake_minimum_required(VERSION 3.4)
enable_language(C CXX ASM)

if(NOT DEFINED MOVESENSE_CORE_LIBRARY)
    # Give error that user must provide  path to movescount-core library
    message(FATAL_ERROR "Path to movesense-core library not set. Add -DMOVESENSE_CORE_LIBRARY=<path_to_core_lib>  to cmake command line")
endif()

if(NOT IS_ABSOLUTE ${MOVESENSE_CORE_LIBRARY})
    set(MOVESENSE_CORE_LIBRARY ${CMAKE_BINARY_DIR}/${MOVESENSE_CORE_LIBRARY})
endif()

# Create a list with all the modules
if(NOT DEFINED MOVESENSE_MODULES)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/../modules/OfflineMeasurements)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/../modules/GestureService)

    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/../modules/GattService)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/../modules/GattService/protocol)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/../modules/GattService/protocol/types)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/../modules/GattService/protocol/packets)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/../modules/GattService/protocol/utils)
endif()

include(${MOVESENSE_CORE_LIBRARY}/MovesenseFromStaticLib.cmake REQUIRED)

