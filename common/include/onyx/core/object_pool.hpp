#ifndef ONYX_OBJECT_POOL_HPP
#define ONYX_OBJECT_POOL_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <memory>

namespace onyx::core {

template <typename T, size_t Capacity>
  requires std::move_constructible<T> &&  // 支持移動構造
           std::destructible<T> &&        // 可解構
           (!std::is_const_v<T>)          // 不可為 const
class ObjectPool {
  static_assert(Capacity > 0, "Capacity should > 0");

 private:
  union Slot {
    T object;          ///< 物件
    size_t next_free;  ///< 若每有物件存放下一個可用位置
    Slot() {}
    ~Slot() {}
  };

  std::array<Slot, Capacity> storage_;  ///< 物件存放區
  size_t free_head_{0};                 ///< 下一個可用位置
  size_t num_allocated_{0};             ///< 已分配數量

  static constexpr size_t INVALID_INDEX = Capacity;

 public:
  // ----------------------------------------------------------------------------
  // RAII
  // ----------------------------------------------------------------------------

  ObjectPool() noexcept {
    for (size_t i = 0; i < Capacity - 1; ++i) {
      storage_[i].next_free = i + 1;
    }
    storage_[Capacity - 1].next_free = INVALID_INDEX;
  }

  ~ObjectPool() noexcept {
    std::array<bool, Capacity> is_free{};
    if (free_head_ != INVALID_INDEX) {
      size_t cur = free_head_;
      while (cur != INVALID_INDEX) {
        is_free[cur] = true;
        cur = storage_[cur].next_free;
      }
    }

    for (size_t idx = 0; idx < Capacity; ++idx) {
      if (!is_free[idx]) {
        std::destroy_at(&storage_[idx].object);
      }
    }
  }

  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;
  ObjectPool(ObjectPool&&) = delete;
  ObjectPool& operator=(ObjectPool&&) = delete;

  // ----------------------------------------------------------------------------
  // 分配與釋放
  // ----------------------------------------------------------------------------

  /// @brief 分配一個物件
  template <typename... Args>
    requires std::constructible_from<T, Args...>
  [[nodiscard]] T* allocate(Args&&... args) noexcept {
    if (free_head_ == INVALID_INDEX) {
      return nullptr;
    }

    size_t index = free_head_;
    free_head_ = storage_[index].next_free;

    T* ptr = std::construct_at(&storage_[index].object,
                               std::forward<Args>(args)...);  // 默認構造
    ++num_allocated_;
    return ptr;
  }

  /// @brief 釋放一個物件
  /// @param ptr 必須為此物件值中的指標
  void deallocate(T* ptr) noexcept {
    if (ptr == nullptr) [[unlikely]] {
      return;
    }

    // 透過指標計算對應位置
    size_t index = static_cast<size_t>(
        (reinterpret_cast<char*>(ptr) - reinterpret_cast<char*>(&storage_[0])) / sizeof(Slot));

    // 不屬於當前物件池
    if (index >= Capacity) [[unlikely]] {
      return;
    }

    std::destroy_at(ptr);
    storage_[index].next_free = free_head_;
    free_head_ = index;

    --num_allocated_;
  }

  //----------------------------------------------------------------------------
  // 狀態查詢
  //----------------------------------------------------------------------------
  [[nodiscard]] size_t capacity() const noexcept { return Capacity; }
  [[nodiscard]] size_t size() const noexcept { return num_allocated_; }
  [[nodiscard]] size_t available() const noexcept { return Capacity - num_allocated_; }
  [[nodiscard]] bool empty() const noexcept { return num_allocated_ == 0; }
  [[nodiscard]] bool full() const noexcept { return num_allocated_ == Capacity; }
};

}  // namespace onyx::core

#endif
