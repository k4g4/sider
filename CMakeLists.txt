cmake_minimum_required()
project(sider)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SERVER_SOURCE_FILES src/main.cpp src/protocol.cpp)
set(CLIENT_SOURCE_FILES src/peer.cpp src/protocol.cpp)

add_executable(server ${SERVER_SOURCE_FILES})
add_executable(client ${CLIENT_SOURCE_FILES})

include_directories(include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)
