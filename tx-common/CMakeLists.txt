# ============================
# 定義 Target
# ============================
set(SOURCES
  ./src/core/price.cpp
  ./src/network/socket_address.cpp
  ./src/network/socket.cpp
  ./src/network/tcp_socket.cpp
  ./src/network/udp_socket.cpp
  ./src/ipc/ipc_error.cpp
  ./src/ipc/shared_memory.cpp
)


add_library(tx-common STATIC ${SOURCES}) # Header-only

# ============================
# 設定 include 路徑
# ============================
target_include_directories(tx-common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ============================
# 添加第三方庫
# ============================
find_package(fmt REQUIRED)
if(NOT fmt_FOUND)
  message(STATUS "系統未發現 fmt，正在從 GitHub 下載...")
  include(FetchContent)
  FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
  )
  FetchContent_MakeAvailable(fmt)
endif()

target_link_libraries(tx-common
  PUBLIC
    fmt::fmt
)


# ============================
# 添加測試
# ============================
add_subdirectory(tests)

# ============================
# 添加基準測試
# ============================
add_subdirectory(bench)

# =========================
# 輸出資訊
# =========================
message(STATUS "tx-common: library configured")
message(STATUS "  Include dir: ${CMAKE_CURRENT_SOURCE_DIR}/include")
