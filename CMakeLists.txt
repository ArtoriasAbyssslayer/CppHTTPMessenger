cmake_minimum_required(VERSION 3.10)
project(ModernMessenger CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    set(PLATFORM "Windows")
    add_definitions(-DWINDOWS_BUILD)
    # Windows specific flags
    if(MINGW)
        # MinGW uses GCC-style flags
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -static-libgcc -static-libstdc++")
    elseif(MSVC)
        # MSVC uses different warning flags
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc")
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif()
else()
    set(PLATFORM "Linux/Unix")
    add_definitions(-DLINUX_BUILD)
    # Linux/Unix specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set install prefix to user directory on Windows
if(WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Server executable
add_executable(messenger_server 
    ${CMAKE_SOURCE_DIR}/src/server.cpp
)

# Client executable
add_executable(messenger_client 
    ${CMAKE_SOURCE_DIR}/src/client.cpp
)

# Platform-specific linking
if(WIN32)
    # Link Windows socket library
    target_link_libraries(messenger_server ws2_32)
    target_link_libraries(messenger_client ws2_32)
else()
    # Link pthread for Linux/Unix
    target_link_libraries(messenger_server pthread)
    target_link_libraries(messenger_client pthread)
endif()

# Install targets
install(TARGETS messenger_server messenger_client
    RUNTIME DESTINATION bin)

# Custom target for running server
add_custom_target(run-server
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/messenger_server
    DEPENDS messenger_server
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Starting messenger server..."
)

# Custom target for running client
add_custom_target(run-client
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/messenger_client
    DEPENDS messenger_client
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Starting messenger client..."
)

# Print build information
message(STATUS "Modern Messenger - Build Configuration")
message(STATUS "======================================")
message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Source Directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "Build Directory: ${CMAKE_BINARY_DIR}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "======================================")
message(STATUS "Targets:")
message(STATUS "  messenger_server - Chat server")
message(STATUS "  messenger_client - Chat client")
message(STATUS "======================================")
if(WIN32)
    message(STATUS "Windows Build:")
    message(STATUS "  cmake .. -G \"MinGW Makefiles\"  - Configure with MinGW")
    message(STATUS "  cmake .. -G \"Visual Studio 17 2022\" - Configure with VS 2022")
    message(STATUS "  cmake .. -G \"Visual Studio 16 2019\" - Configure with VS 2019")
    message(STATUS "  cmake --build .      - Build all targets")
else()
    message(STATUS "Linux/Unix Build:")
    message(STATUS "  cmake ..             - Configure")
    message(STATUS "  make                 - Build all targets")
    message(STATUS "  make run-server      - Run the server")
    message(STATUS "  make run-client      - Run the client")
endif()
message(STATUS "=======================================")