cmake_minimum_required(VERSION 3.20)
project(endstone_chestform_api VERSION 1.0.10 LANGUAGES CXX)

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
    
    # Apply local patch to BlockPos.cpp in fetched source to correct Y coordinate serialization
    message(STATUS "Applying local patch to BlockPos.cpp in: ${protocol_SOURCE_DIR}")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/patches/BlockPos.cpp" 
         DESTINATION "${protocol_SOURCE_DIR}/src/sculk/protocol/codec/level/block")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/patches/BinaryStream.hpp" 
         DESTINATION "${protocol_SOURCE_DIR}/include/sculk/protocol/utility")

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
        if(TARGET Protocol)
            target_compile_options(Protocol PRIVATE -Wno-invalid-constexpr)
        endif()
    endif()
else()
    message(STATUS "Protocol library found locally!")
endif()

# Build the Shared Library/Plugin using the Endstone helper function
endstone_add_plugin(chestform_api ${SRC_FILES})


# Compiler Definitions & Header Inclusion
target_include_directories(chestform_api PUBLIC ${INC_DIRS})
target_compile_definitions(chestform_api PRIVATE CHEST_FORM_API_EXPORTS)

# Compiler-specific settings
if(MSVC)
    target_compile_options(chestform_api PRIVATE /W4 /utf-8)
else()
    target_compile_options(chestform_api PRIVATE -Wall -Wextra -Wpedantic -Wno-invalid-constexpr)
endif()

# Link Dependencies
target_link_libraries(chestform_api 
    PRIVATE 
        Protocol::Protocol
)

# Install API headers for integration by other plugins
install(DIRECTORY include/chest_form_api/
    DESTINATION include/chest_form_api
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS chestform_api
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
