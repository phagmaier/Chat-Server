
cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0167 NEW)

# Set the project name
project(ServerApp)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set optimization level to O3
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# Add warning flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Specify the source files
set(SOURCES
    ../src/main.cpp
    ../src/server.cpp
    ../src/db.cpp
)

# Find Boost libraries
find_package(Boost REQUIRED COMPONENTS system iostreams)

# Find SQLite3 (still good to call for checking if headers are present)
find_package(SQLite3 REQUIRED)

# Create the executable
add_executable(server ${SOURCES})

# Link Boost and SQLite3 libraries
target_link_libraries(server
    Boost::system
    Boost::iostreams
    sqlite3 # Directly link the library by its common name (e.g., libsqlite3.so)
    pthread
)

