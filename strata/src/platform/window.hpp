// The host window: GL context, input events, cursor capture, fullscreen and
// timing. One implementation per platform:
//   window_glfw.cpp  desktop (GLFW, OpenGL 3.3 core)
//   window_web.cpp   browsers, including phones and tablets (canvas, WebGL 2)
#pragma once

#include <memory>
#include <string>

namespace strata {

class Input;

class Window {
 public:
  Window();
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  // Opens the window, makes its GL context current and loads the GL entry
  // points. Events are delivered to `input` from then on.
  bool create(int width, int height, bool vsync, Input& input);
  void destroy();
  bool valid() const { return impl_ != nullptr; }

  void pollEvents();
  void swapBuffers();
  bool closeRequested() const;

  // Drawable size in pixels; cursor and touch positions are reported in the
  // same pixels.
  void framebufferSize(int& w, int& h) const;
  // Framebuffer pixels per layout unit (the CSS pixel in browsers, the
  // content scale on desktops): sizes touch targets.
  float pixelRatio() const;

  void setCursorCaptured(bool captured);
  // Whether the pointer really is captured (a browser may refuse or end it).
  bool cursorCaptured() const;
  void setFullscreen(bool on);
  bool fullscreen() const;
  void setVsync(bool on);

  // Platform traits.
  static bool canQuit();         // false in a browser tab
  static bool hasTouchScreen();  // the device has a touch screen
  static double now();           // seconds, monotonic

  // Asks for a line of text with a native dialog: on touch screens no key
  // events arrive for an on-screen keyboard. Returns false when cancelled or
  // unsupported.
  static bool promptText(const std::string& title, std::string& text);
  // Makes files written below the save directory durable (IndexedDB in a
  // browser; a no-op on desktops, where writes go straight to disk).
  static void persistStorage();

  struct Impl;  // per platform (public so event callbacks can reach it)

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace strata
