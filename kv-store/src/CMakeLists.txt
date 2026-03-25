cmake_minimum_required(VERSION 3.16)
project(KVStore CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -fsanitize=address,thread -fno-omit-frame-pointer")

# Server library
add_library(kvlib STATIC
    src/thread_pool.cpp
    src/consistent_hash.cpp
    src/wal.cpp
    src/cache.cpp
    src/node.cpp
    src/server.cpp
    src/protocol.cpp
)

target_include_directories(kvlib PUBLIC src/)
target_link_libraries(kvlib PRIVATE pthread)

# Server binary
add_executable(kv_server src/main.cpp)
target_link_libraries(kv_server kvlib pthread)

# Client binary
add_executable(kv_client client/client.cpp src/protocol.cpp)
target_include_directories(kv_client PRIVATE src/)
target_link_libraries(kv_client pthread)

# Tests
enable_testing()
add_executable(kv_tests tests/test_kv.cpp)
target_link_libraries(kv_tests kvlib pthread)
add_test(NAME kv_unit_tests COMMAND kv_tests)
