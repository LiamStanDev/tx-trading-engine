#include "onyx/net/taifex/message.hpp"

namespace onyx::net::taifex {

void ProductSpecTable::update(const ProductSpec& spec) noexcept {
  std::string_view prod_id_sv(spec.prod_id.data(), spec.prod_id.size());
  size_t last = prod_id_sv.find_last_not_of(' ');
  size_t len = (last == std::string_view::npos) ? 10 : last + 1;
  prod_id_sv = prod_id_sv.substr(0, len);
  std::string key(prod_id_sv);  // WARN: 這邊有產生堆分配
  table_[std::move(key)] = spec;
}

std::optional<uint8_t> ProductSpecTable::get_decimal_locator(
    std::string_view prod_id) const noexcept {
  std::string key(prod_id);

  auto it = table_.find(key);

  if (it == table_.end()) [[unlikely]] {
    return std::nullopt;
  }

  return it->second.decimal_locator;
}

}  // namespace onyx::net::taifex
