cmake_minimum_required(VERSION 3.20)
project(endstone_chest_form_cpp VERSION 1.0.2 LANGUAGES CXX)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Configurable Endstone API Version
set(ENDSTONE_API_VERSION "0.11.0" CACHE STRING "Target Endstone API version")

# Shared compilation configurations
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Source Files
set(SRC_FILES
    src/chest_form.cpp
    src/chest_form_manager.cpp
    src/plugin.cpp
)

# Include directories
set(INC_DIRS
    include
)

# Fetch Endstone API Headers
include(FetchContent)
FetchContent_Declare(
    endstone
    GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
    GIT_TAG "v${ENDSTONE_API_VERSION}"
)
FetchContent_MakeAvailable(endstone)

# Disable strict constexpr errors under Clang for external dependencies (like Protocol)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    add_compile_options(-Wno-invalid-constexpr)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-constexpr" CACHE STRING "" FORCE)
endif()

# Find or Fetch SculkCatalystMC/Protocol
find_package(Protocol CONFIG QUIET)
if(NOT Protocol_FOUND)
    message(STATUS "Protocol library not found locally. Fetching from GitHub...")
    # Configure Protocol library to build only what we need (codec only, no connection/networking)
    set(PROTOCOL_ENABLE_CODEC ON CACHE BOOL "" FORCE)
    set(PROTOCOL_ENABLE_AUTHENTICATION OFF CACHE BOOL "" FORCE)
    set(PROTOCOL_ENABLE_CONNECTION OFF CACHE BOOL "" FORCE)
    set(PROTOCOL_ENABLE_EXCEPTIONS ON CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        Protocol
        GIT_REPOSITORY https://github.com/SculkCatalystMC/Protocol.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(Protocol)
else()
    message(STATUS "Protocol library found locally!")
endif()

# Build the Shared Library/Plugin using the Endstone helper function
endstone_add_plugin(chest_form_cpp ${SRC_FILES})

# Compiler Definitions & Header Inclusion
target_include_directories(chest_form_cpp PUBLIC ${INC_DIRS})
target_compile_definitions(chest_form_cpp PRIVATE CHEST_FORM_API_EXPORTS)

# Compiler-specific settings
if(MSVC)
    target_compile_options(chest_form_cpp PRIVATE /W4 /utf-8)
else()
    target_compile_options(chest_form_cpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Link Dependencies
target_link_libraries(chest_form_cpp 
    PRIVATE 
        Protocol::Protocol
)

# Install API headers for integration by other plugins
install(DIRECTORY include/chest_form_api/
    DESTINATION include/chest_form_api
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS chest_form_cpp
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
