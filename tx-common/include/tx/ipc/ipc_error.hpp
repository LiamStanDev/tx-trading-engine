#ifndef TX_TRADING_ENGINE_IPC_IPC_ERROR_HPP
#define TX_TRADING_ENGINE_IPC_IPC_ERROR_HPP

#include <cstdint>
#include <string>
#include <tx/core/result.hpp>

namespace tx::ipc {

enum class IpcErrorCode : uint8_t {
  /// @brief IPC 操作錯誤碼
  // 共享記憶體相關
  ShmCreateFailed,  // shm_open 創建失敗
  ShmOpenFailed,    // shm_open 打開失敗
  ShmUnlinkFailed,  // shm_unlink 刪除失敗
  MmapFailed,       // mmap 映射失敗
  MunmapFailed,     // munmap 解除映射失敗
  FtruncateFailed,  // ftruncate 調整大小失敗
  FstatFailed,      // fstat 查詢失敗

  // 參數驗證相關
  InvalidName,        // 無效的 SHM 名稱
  InvalidSize,        // 無效的大小（0 或過大）
  InvalidPermission,  // 無效的權限設定

  // 狀態相關
  AlreadyExists,     // SHM 已存在（O_EXCL 失敗）
  NotFound,          // SHM 不存在
  PermissionDenied,  // 權限不足

  // 系統資源相關
  OutOfMemory,   // 記憶體不足
  TooManyFiles,  // 打開的檔案描述符過多
  SystemError,   // 其他系統錯誤

  // 參數錯誤
  InvalidParameter  // 參數錯誤
};

class IpcError {
 private:
  IpcErrorCode code_;
  int errno_;
  std::string detail_;

  /// @brief 創建錯誤
  /// @return IpcError
  explicit IpcError(IpcErrorCode code, std::string detail, int err) noexcept
      : code_(code), errno_(err), detail_(std::move(detail)) {}

 public:
  /// @brief 創建錯誤
  /// @param code 錯誤碼
  /// @param detail 詳細訊息（可選）
  /// @return IpcError
  explicit IpcError(IpcErrorCode code, std::string detail = "") noexcept
      : IpcError(code, std::move(detail), 0) {}

  /// @brief 從當前 errno 創建錯誤 (明確表示從 errno 建立)
  /// @param code 錯誤碼
  /// @param detail 詳細訊息（可選）
  /// @return IpcError
  static IpcError from_errno(IpcErrorCode code,
                             std::string detail = "") noexcept;

  // ===========================
  // 訪問方法
  // ===========================
  /// @brief 取得錯誤碼
  [[nodiscard]] IpcErrorCode code() const noexcept { return code_; }

  /// @brief 取得系統錯誤碼
  [[nodiscard]] int system_errno() const noexcept { return errno_; }

  /// @brief 取得詳細訊息
  [[nodiscard]] const std::string& detail() const noexcept { return detail_; }

  /// @brief 取得錯誤碼名稱
  /// @return 錯誤碼的字串表示（string literal）
  [[nodiscard]] const char* code_name() const noexcept;

  /// @brief 生成完整錯誤訊息
  /// @return 格式化的錯誤訊息
  /// @details 格式：[ErrorCode]: Detail (errno: N, strerror)
  [[nodiscard]] std::string message() const noexcept;
};

template <typename T = void>
using IpcResult = Result<T, IpcError>;

}  // namespace tx::ipc

#endif
