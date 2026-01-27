# ============================
# 定義 Target
# ============================
add_library(tx-common STATIC)
add_library(tx::common ALIAS tx-common)

# ============================
# Source files
# ============================
target_sources(
    tx-common
    PRIVATE
        ./src/error.cpp
        ./src/core/type.cpp
        ./src/io/socket_address.cpp
        ./src/io/socket.cpp
        ./src/io/tcp_socket.cpp
        ./src/io/udp_socket.cpp
        ./src/io/file.cpp
        ./src/io/mapped_file.cpp
        ./src/io/buf_reader.cpp
        ./src/ipc/shared_memory.cpp
        ./src/sys/cpu_affinity.cpp
)

# ============================
# Target 屬性設定
# ============================
# Include 目錄
target_include_directories(
    tx-common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 鏈接編譯器選項 (使用 PUBLIC 所以後續依賴的都會引入)
target_link_libraries(tx-common PRIVATE project::compiler-options fmt::fmt)

# ============================
# 測試
# ============================
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ============================
# Benchmarks
# ============================
if(BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()

# ============================
# 輸出資訊
# ============================
message(STATUS "tx-common: configured")
message(STATUS "  Include: ${CMAKE_CURRENT_SOURCE_DIR}/include")
