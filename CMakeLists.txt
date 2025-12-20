cmake_minimum_required(VERSION 3.21)
project(unPAKer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

add_library(unpaker_core
    src/pak_parser.cpp
    src/parsers/vpk_parser.cpp
    src/parsers/ue_parser.cpp
    src/parsers/generic_parser.cpp
    src/memory_tracker.cpp
    src/application_manager.cpp
    src/file_validator.cpp
    src/config.cpp
    src/logger.cpp
)

target_include_directories(unpaker_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/parsers
)

target_compile_options(unpaker_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_library(unpaker_gui
    src/gui_manager.cpp
)

target_include_directories(unpaker_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(unpaker_gui PUBLIC
    unpaker_core
    user32
    gdi32
    comctl32
    shlwapi
    winhttp
)

target_compile_options(unpaker_gui PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_executable(unPAKer
    src/main.cpp
    unPAKer.rc
)

if(MSVC)
    set_source_files_properties(unPAKer.rc PROPERTIES LANGUAGE RC)
endif()

target_link_libraries(unPAKer PRIVATE
    unpaker_core
    unpaker_gui
)

target_include_directories(unPAKer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT unPAKer)

install(TARGETS unPAKer DESTINATION bin)
install(TARGETS unpaker_core DESTINATION lib)
install(TARGETS unpaker_gui DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/unpaker)


