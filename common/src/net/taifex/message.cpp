#include "onyx/net/taifex/message.hpp"

#include <optional>

namespace onyx::net::taifex {

void ProductSpecTable::update(const ProductSpec& spec) noexcept { table_[spec.prod_id] = spec; }

std::optional<uint8_t> ProductSpecTable::get_decimal_locator(ProdIdKey& prod_id) const noexcept {
  auto it = table_.find(prod_id);

  if (it == table_.end()) [[unlikely]] {
    return std::nullopt;
  }

  return it->second.decimal_locator;
}

}  // namespace onyx::net::taifex
