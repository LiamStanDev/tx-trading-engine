
#ifndef TX_TRADING_ENGINE_PROTOCOLS_FIX_BUILDER_HPP
#define TX_TRADING_ENGINE_PROTOCOLS_FIX_BUILDER_HPP

#include <string>
#include <string_view>
#include <vector>

#include "tx/protocols/fix/error.hpp"

namespace tx::protocols::fix {

namespace constraints {

inline constexpr size_t kDefaultFieldCapacity = 32;
inline constexpr size_t kMaxBodyLength = 99999;
inline constexpr int kChecksumModulo = 256;
inline constexpr size_t kBodyLengthMaxDigits = 5;  // "99999" = 5
inline constexpr size_t kBodyLengthFieldReserve =
    2 + kBodyLengthMaxDigits + 1;  // "9=" + 5 digits + SOH = 8

}  // namespace constraints

/// @brief FIX 訊息構造器
class MessageBuilder {
 private:
  struct Field {
    int tag;
    std::string value;
  };

  std::string begin_string_ = "FIX.4.2";
  std::string msg_type_;
  std::string sender_comp_id_;
  std::string target_comp_id_;
  int msg_seq_num_{0};
  std::string sending_time_;
  std::vector<Field> fields_;

 public:
  // ============================
  // 建構函數
  // ============================

  /// @brief 建構子
  explicit MessageBuilder(std::string_view msg_type) : msg_type_(msg_type) {
    fields_.reserve(constraints::kDefaultFieldCapacity);
  }

  // ============================
  // Header 欄位
  // ============================

  /// @brief 設定發送方 ID（Tag 49）
  MessageBuilder& set_sender(std::string_view sender) {
    sender_comp_id_ = sender;
    return *this;
  }

  /// @brief 設定接收方 ID（Tag 56）
  MessageBuilder& set_target(std::string_view target) {
    target_comp_id_ = target;
    return *this;
  }
  /// @brief 設定訊息序號（Tag 34）
  /// @details 必須 > 0，用於訊息去重與順序保證
  MessageBuilder& set_msg_seq_num(int seq_num) {
    msg_seq_num_ = seq_num;
    return *this;
  }
  /// @brief 設定發送時間（Tag 52）
  /// @param time UTC 時間字串，格式：YYYYMMDD-HH:MM:SS
  MessageBuilder& set_sending_time(std::string_view time) {
    sending_time_ = time;
    return *this;
  }

  // ============================
  // Body 欄位
  // ============================

  /// @brief 新增字串型欄位
  MessageBuilder& add_field(int tag, std::string_view value) {
    fields_.push_back({tag, std::string(value)});
    return *this;
  }
  /// @brief 新增整數型欄位
  MessageBuilder& add_field(int tag, int value) {
    fields_.push_back({tag, std::to_string(value)});
    return *this;
  }
  /// @brief 新增浮點數型欄位
  MessageBuilder& add_field(int tag, double value, int precision = 2) {
    fields_.push_back({tag, fmt::format("{:.{}f}", value, precision)});
    return *this;
  }

  // ============================
  // 構造訊息
  // ============================

  /// @brief 建構完整 FIX 訊息
  /// @return 成功返回 FIX 協定字串，失敗返回錯誤碼
  /// @details
  ///   - 驗證必要欄位：MsgType, Sender, Target, SeqNum, SendingTime
  ///   - 自動計算 BodyLength 與 Checksum
  Result<std::string> build() const noexcept;

 private:
  static void append_tag_str(std::string& str, int tag,
                             std::string_view value) noexcept;

  static void append_tag_int(std::string& str, int tag, int value) noexcept;

  /// @brief 追加 checksum tag @details 獨立於 append_tag_int 是因爲 FIX
  /// 格式規定要補齊到三位
  static void append_checksum(std::string& str, int checksum) noexcept;

  [[nodiscard]] static int calculate_checksum(std::string_view msg) noexcept;

  /// @brief 估算最終訊息大小
  /// @details 用於 std::string::reserve() 減少記憶體重分配
  [[nodiscard]] size_t estimate_size() const noexcept;
};

}  // namespace tx::protocols::fix

#endif
