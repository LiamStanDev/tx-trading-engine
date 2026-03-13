#ifndef ONYX_DB_PG_CONNECTION_POOL_HPP
#define ONYX_DB_PG_CONNECTION_POOL_HPP

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "onyx/db/pg/connection.hpp"

namespace onyx::db::pg {
class ConnectionPool {
 private:
  std::string conninfo_;
  size_t pool_size_;

  std::queue<std::unique_ptr<Connection>> available_;
  std::mutex mutex_;
  std::condition_variable cv_;

 public:
  explicit ConnectionPool(std::string conninfo, size_t pool_size = 10)
      : conninfo_(std::move(conninfo)), pool_size_(pool_size) {
    for (size_t i = 0; i < pool_size_; ++i) {
      auto conn = std::make_unique<Connection>();
      if (conn->connect(conninfo_.c_str())) {
        available_.push(std::move(conn));
      }
    }
  }

  // RAII Wrapper
  class PooledConnection {
   private:
    std::unique_ptr<Connection> conn_;
    ConnectionPool* pool_;

   public:
    PooledConnection(std::unique_ptr<Connection> conn, ConnectionPool* pool) noexcept
        : conn_(std::move(conn)), pool_(pool) {}

    ~PooledConnection() noexcept {
      if (conn_ && pool_) {
        pool_->return_connection(std::move(conn_));
      }
    }

    Connection* operator->() { return conn_.get(); }
    Connection& operator*() { return *conn_; }
  };

  PooledConnection acquire() noexcept;

 private:
  void return_connection(std::unique_ptr<Connection> conn);
};

}  // namespace onyx::db::pg
#endif
