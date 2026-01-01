# ============================
# 定義 Target
# ============================
add_library(tx-common STATIC)
add_library(tx::common ALIAS tx-common)

# ============================
# Source files
# ============================
target_sources(tx-common
  PRIVATE
    ./src/core/error.cpp
    ./src/core/price.cpp
    ./src/network/socket_address.cpp
    ./src/network/socket.cpp
    ./src/network/tcp_socket.cpp
    ./src/network/udp_socket.cpp
    ./src/ipc/shared_memory.cpp
)

# ============================
# Target 屬性設定
# ============================
# Include 目錄
target_include_directories(tx-common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 鏈接編譯器選項 (使用 Publish 所以後續依賴的都會引入)
target_link_libraries(tx-common
  PUBLIC
    tx::compiler-options
    fmt::fmt
)

# ============================
# 測試
# ============================
if(TX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# ============================
# Benchmarks
# ============================
if(TX_BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()

# ============================
# 輸出資訊
# ============================
message(STATUS "tx-common: configured")
message(STATUS "  Sources: ${TX_COMMON_SOURCES}")
message(STATUS "  Include: ${CMAKE_CURRENT_SOURCE_DIR}/include")
