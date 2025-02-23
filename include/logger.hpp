#pragma once

#include <iostream>

class logger {
  template <typename... P>
    requires(sizeof...(P) >= 1)
  void log_impl(std::ostream &o, P &&...args) {
    ((o << args), ...);
  }

  void set(std::size_t level, bool v) {
    if (v)
      log_level_ |= level;
    else
      log_level_ &= ~level;
  }

  std::size_t log_level_{};

public:
  static constexpr std::size_t inf{1}, wrn{1 << 1}, err{1 << 2};
  logger(std::size_t v = logger::err) : log_level_{v} {}

  logger(const logger &) = default;
  logger &operator=(const logger &) = default;
  logger(logger &&) = default;
  logger &operator=(logger &&) = default;
  ~logger() = default;

  template <std::size_t lvl> void set(bool v) {
    static_assert(lvl == inf || lvl == wrn || lvl == err);
    this->set(lvl, v);
  }

  template <typename... P> void logi(P &&...args) {
    if (log_level_ & inf)
      log_impl(std::cout, "(INF): ", std::forward<P>(args)...);
  }

  template <typename... P> void logs(P &&...args) {
    if (log_level_ & inf)
      log_impl(std::cout, std::forward<P>(args)...);
  }

  template <typename... P> void logw(P &&...args) {
    if (log_level_ & wrn)
      log_impl(std::cout, "(WRN): ", std::forward<P>(args)...);
  }

  template <typename... P> void loge(P &&...args) {
    if (log_level_ & err)
      log_impl(std::cerr, "(ERR): ", std::forward<P>(args)...);
  }
};
