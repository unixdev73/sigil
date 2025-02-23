#include "sigil.hpp"
#include <chrono>
#include <logger.hpp>
#include <thread>

namespace ch = std::chrono;

bool initialize(context *, int argc, char **argv);
bool render(context *c);
bool update(context *c);

int main(int argc, char **argv) {
  logger l{logger::err};
  context ctx{};

  if (!initialize(&ctx, argc, argv)) {
    l.loge("Initialization failed\n");
    return 1;
  }

  constexpr ch::milliseconds time_per_frame{std::size_t((1.0 / 60.0) * 1000)};
  auto frame_start = ch::steady_clock::now();

  while (glfwWindowShouldClose(ctx.window.handle) != GLFW_TRUE) {
    auto now = ch::steady_clock::now();

    if (ch::duration_cast<ch::milliseconds>(now - frame_start) > time_per_frame)
      frame_start = now;
    else {
      std::this_thread::sleep_for(ch::microseconds{100});
      continue;
    }

    glfwPollEvents();

    if (!update(&ctx)) {
      l.loge("Updating failed\n");
      return 1;
    }

    if (!render(&ctx)) {
      l.loge("Rendering failed\n");
      return 2;
    }
  }
}
