#include "onyx/db/pg/connection_pool.hpp"

namespace onyx::db::pg {

ConnectionPool::PooledConnection ConnectionPool::acquire() noexcept {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return !available_.empty(); });
  auto conn = std::move(available_.front());
  available_.pop();
  return PooledConnection(std::move(conn), this);
}

void ConnectionPool::return_connection(std::unique_ptr<Connection> conn) {
  std::lock_guard<std::mutex> lock(mutex_);
  available_.push(std::move(conn));
  cv_.notify_one();
}

}  // namespace onyx::db::pg
