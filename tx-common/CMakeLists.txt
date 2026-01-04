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
        ./src/core/context_error.cpp
        ./src/core/price.cpp
        ./src/network/error.cpp
        ./src/network/socket_address.cpp
        ./src/network/socket.cpp
        ./src/network/tcp_socket.cpp
        ./src/network/udp_socket.cpp
        ./src/ipc/error.cpp
        ./src/ipc/shared_memory.cpp
        ./src/protocols/fix/error.cpp
        ./src/protocols/fix/field.cpp
        ./src/protocols/fix/parser.cpp
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
target_link_libraries(tx-common PUBLIC project::compiler-options fmt::fmt)

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
