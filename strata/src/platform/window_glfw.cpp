// Desktop window: GLFW with an OpenGL 3.3 core context.
#include "platform/window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>

#include "core/log.hpp"
#include "game/input.hpp"
#include "gl/gl.hpp"

namespace strata {

namespace {
void errorCallback(int code, const char* desc) { LOG_ERROR("GLFW error %d: %s", code, desc); }

void STRATA_GLAPI glDebugCallback(GLenum, GLenum, GLuint, GLenum severity, GLsizei, const GLchar* message,
                                  const void*) {
  if (severity == GL_DEBUG_SEVERITY_HIGH) LOG_ERROR("GL: %s", message);
}

gl::GenericProc loader(const char* name) { return glfwGetProcAddress(name); }

Input* inputOf(GLFWwindow* w) { return static_cast<Input*>(glfwGetWindowUserPointer(w)); }
}  // namespace

struct Window::Impl {
  GLFWwindow* window = nullptr;
  bool vsync = true;
  bool captured = false;
  int windowedX = 100, windowedY = 100, windowedW = 1280, windowedH = 720;
};

Window::Window() = default;
Window::~Window() { destroy(); }

bool Window::create(int width, int height, bool vsync, Input& input) {
  destroy();
  glfwSetErrorCallback(errorCallback);
  if (!glfwInit()) {
    LOG_ERROR("could not initialise GLFW");
    return false;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
  glfwWindowHint(GLFW_SAMPLES, 0);
  GLFWwindow* w = glfwCreateWindow(width, height, "Strata", nullptr, nullptr);
  if (!w) {
    LOG_ERROR("could not create an OpenGL 3.3 core window; is your graphics driver up to date?");
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(w);
  const char* missing = nullptr;
  if (!gl::loadFunctions(loader, &missing)) {
    LOG_ERROR("OpenGL entry point %s is missing", missing ? missing : "?");
    glfwDestroyWindow(w);
    glfwTerminate();
    return false;
  }
  if (gl::hasDebugOutput()) {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(glDebugCallback, nullptr);
  }
  impl_ = std::make_unique<Impl>();
  impl_->window = w;
  impl_->windowedW = width;
  impl_->windowedH = height;

  glfwSetWindowUserPointer(w, &input);
  glfwSetKeyCallback(w, [](GLFWwindow* win, int key, int, int action, int) { inputOf(win)->onKey(key, action); });
  glfwSetCharCallback(w, [](GLFWwindow* win, unsigned int cp) { inputOf(win)->onChar(cp); });
  glfwSetMouseButtonCallback(w, [](GLFWwindow* win, int b, int action, int) { inputOf(win)->onButton(b, action); });
  // Cursor positions arrive in screen coordinates; scale them to framebuffer
  // pixels (they differ on high-DPI displays).
  glfwSetCursorPosCallback(w, [](GLFWwindow* win, double x, double y) {
    int ww = 1, wh = 1, fw = 1, fh = 1;
    glfwGetWindowSize(win, &ww, &wh);
    glfwGetFramebufferSize(win, &fw, &fh);
    inputOf(win)->onCursor(x, y, static_cast<float>(fw) / static_cast<float>(std::max(ww, 1)),
                           static_cast<float>(fh) / static_cast<float>(std::max(wh, 1)));
  });
  glfwSetScrollCallback(w, [](GLFWwindow* win, double, double dy) { inputOf(win)->onScroll(dy); });
  glfwSetWindowFocusCallback(w, [](GLFWwindow* win, int f) { inputOf(win)->onFocus(f == GLFW_TRUE); });
  glfwSetFramebufferSizeCallback(w, [](GLFWwindow* win, int, int) { inputOf(win)->onResize(); });
  setVsync(vsync);
  return true;
}

void Window::destroy() {
  if (!impl_) return;
  glfwDestroyWindow(impl_->window);
  impl_.reset();
  glfwTerminate();
}

void Window::pollEvents() { glfwPollEvents(); }

void Window::swapBuffers() { glfwSwapBuffers(impl_->window); }

bool Window::closeRequested() const { return !impl_ || glfwWindowShouldClose(impl_->window); }

void Window::framebufferSize(int& w, int& h) const { glfwGetFramebufferSize(impl_->window, &w, &h); }

float Window::pixelRatio() const {
  float sx = 1.0f, sy = 1.0f;
  glfwGetWindowContentScale(impl_->window, &sx, &sy);
  return std::max(1.0f, std::max(sx, sy));
}

void Window::setCursorCaptured(bool captured) {
  if (!impl_) return;
  impl_->captured = captured;
  glfwSetInputMode(impl_->window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  if (glfwRawMouseMotionSupported())
    glfwSetInputMode(impl_->window, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
}

bool Window::cursorCaptured() const { return impl_ && impl_->captured; }

void Window::setFullscreen(bool on) {
  if (!impl_) return;
  Impl& m = *impl_;
  GLFWmonitor* current = glfwGetWindowMonitor(m.window);
  if (on && !current) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (!mode) return;
    glfwGetWindowPos(m.window, &m.windowedX, &m.windowedY);
    glfwGetWindowSize(m.window, &m.windowedW, &m.windowedH);
    glfwSetWindowMonitor(m.window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  } else if (!on && current) {
    glfwSetWindowMonitor(m.window, nullptr, m.windowedX, m.windowedY, m.windowedW, m.windowedH, 0);
  }
  setVsync(m.vsync);  // some drivers reset the swap interval on mode changes
}

bool Window::fullscreen() const { return impl_ && glfwGetWindowMonitor(impl_->window) != nullptr; }

void Window::setVsync(bool on) {
  if (!impl_) return;
  impl_->vsync = on;
  glfwSwapInterval(on ? 1 : 0);
}

bool Window::canQuit() { return true; }
bool Window::hasTouchScreen() { return false; }
double Window::now() { return glfwGetTime(); }
bool Window::promptText(const std::string&, std::string&) { return false; }
void Window::persistStorage() {}

}  // namespace strata
