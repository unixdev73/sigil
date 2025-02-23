#pragma once

namespace raii {
template <typename T>
concept resource_policy = requires(T a) {
  a.handle;
  a.destroy();
};

template <typename T>
  requires resource_policy<T>
struct resource : public T {
  resource &operator=(const resource &) = delete;
  resource(const resource &) = delete;

  resource &operator=(resource &&o) {
    if (this != &o) {
      if (this->cleanup())
        this->destroy();

      T::operator=(static_cast<resource &&>(o));
      this->cleanup(o.cleanup());
      o.release();
    }

    return *this;
  }

  resource(resource &&o) { *this = static_cast<resource &&>(o); }

  template <typename... P>
  resource(P &&...args) : T(static_cast<P &&>(args)...) {}

  template <typename... P> resource(P &...args) : T(args...) {}

  resource() : cleanup_{false} {}
  ~resource() {
    if (cleanup_)
      this->destroy();
  }

  bool cleanup() const { return cleanup_; }
  void cleanup(bool v) { cleanup_ = v; }
  void destroy() { T::destroy(); }

  decltype(T::handle) release() {
    auto h = static_cast<decltype(T::handle) &&>(this->handle);
    this->cleanup(false);
    this->handle = {};
    return h;
  }

private:
  bool cleanup_{true};
};
} // namespace raii
