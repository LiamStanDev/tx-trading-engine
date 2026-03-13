# ============================
# 定義 Target
# ============================
add_library(onyx-common STATIC)
add_library(onyx::common ALIAS onyx-common)

# ============================
# Source files
# ============================
target_sources(
    onyx-common
    PRIVATE
        ./src/error.cpp
        ./src/core/type.cpp
        ./src/bench/latency_recorder.cpp
        ./src/io/socket_address.cpp
        ./src/io/socket.cpp
        ./src/io/tcp_socket.cpp
        ./src/io/udp_socket.cpp
        ./src/io/file.cpp
        ./src/io/mapped_file.cpp
        ./src/db/pg/connection.cpp
        ./src/db/pg/connection_pool.cpp
        ./src/ipc/shared_memory.cpp
        ./src/sys/cpu_affinity.cpp
        ./src/net/taifex/codec.cpp
        ./src/net/taifex/message.cpp
        ./src/net/taifex/parser.cpp
)

# ============================
# Target 屬性設定
# ============================
# Include 目錄
target_include_directories(
    onyx-common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 鏈接編譯器選項 (使用 PUBLIC 所以後續依賴的都會引入)
target_link_libraries(
    onyx-common
    PRIVATE
        project::compiler-options # 避免依賴也用相同方式編譯
        gssapi_krb5 # PostgreSQL GSSAPI 支援所需
    PUBLIC
        fmt::fmt # 因為 hpp 有暴露，故依賴該此庫的都要依賴
        libpq::libpq
)

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
message(STATUS "onyx-common: configured")
message(STATUS "  Include: ${CMAKE_CURRENT_SOURCE_DIR}/include")
