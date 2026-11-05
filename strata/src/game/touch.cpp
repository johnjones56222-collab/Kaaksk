#include "game/touch.hpp"

#include <algorithm>
#include <cmath>

#include "render/ui.hpp"

namespace strata {

namespace {
// Pushing the stick this far (in stick radii) sprints; beyond it the stick's
// centre follows the thumb so that changing direction stays immediate.
constexpr float kSprintReach = 1.35f;
constexpr float kFollowReach = 1.6f;
constexpr float kDeadZone = 0.15f;
}  // namespace

TouchControls::Finger* TouchControls::find(int id) {
  for (Finger& f : fingers_)
    if (f.id == id) return &f;
  return nullptr;
}

bool TouchControls::owns(int id) const {
  for (const Finger& f : fingers_)
    if (f.id == id) return true;
  return false;
}

void TouchControls::releaseAll() {
  fingers_.clear();
  out_ = Output{};
  out_.sneak = sneakToggle_;  // the sneak toggle outlives a menu
}

TouchControls::Circle TouchControls::button(int b, const Layout& l) const {
  const float u = l.unit;
  const float hotbarRight = l.hotbarX + l.hotbarSlot * 9.0f;
  // The jump button sits in the bottom right corner, raised above the hotbar
  // when the screen is too narrow for them to sit side by side.
  const float jr = 38.0f * u;
  vec2 jump(l.width - 30.0f * u - jr, l.height - 34.0f * u - jr);
  if (jump.x - jr * 2.6f < hotbarRight + 8.0f * u) jump.y = std::min(jump.y, l.hotbarY - 18.0f * u - jr);
  Circle c;
  c.visible = true;
  switch (b) {
    case kJump:
      c.c = jump;
      c.r = jr;
      break;
    case kSneak:
      c.r = 30.0f * u;
      c.c = vec2(jump.x - jr - 22.0f * u - c.r, jump.y + jr - c.r);
      break;
    case kFly:
      c.r = 25.0f * u;
      c.c = vec2(jump.x, jump.y - jr - 20.0f * u - c.r);
      c.visible = l.creative;
      break;
    default: {
      // Inventory, chat and pause along the top right edge.
      c.r = 22.0f * u;
      const int slotFromRight = kPause - b;  // pause rightmost
      c.c = vec2(l.width - 14.0f * u - c.r - static_cast<float>(slotFromRight) * (2.0f * c.r + 12.0f * u),
                 14.0f * u + c.r);
      break;
    }
  }
  return c;
}

int TouchControls::buttonAt(vec2 p, const Layout& l) const {
  for (int b = 0; b < kButtonCount; ++b) {
    const Circle c = button(b, l);
    // Fingers are blunt: accept touches a little outside the drawn circle.
    if (c.visible && length(p - c.c) <= c.r + 8.0f * l.unit) return b;
  }
  return -1;
}

int TouchControls::hotbarSlotAt(vec2 p, const Layout& l) const {
  const float s = l.hotbarSlot;
  if (s <= 0.0f || p.x < l.hotbarX || p.x >= l.hotbarX + s * 9.0f) return -1;
  if (p.y < l.hotbarY - s * 0.25f || p.y > l.hotbarY + s + 6.0f * l.unit) return -1;
  return clampv(static_cast<int>((p.x - l.hotbarX) / s), 0, 8);
}

vec2 TouchControls::stickRest(const Layout& l) const {
  const float u = l.unit;
  const float r = kStickRadius * u;
  vec2 c(30.0f * u + r, l.height - 34.0f * u - r);
  if (c.x + r * 1.2f > l.hotbarX - 8.0f * u) c.y = std::min(c.y, l.hotbarY - 18.0f * u - r);
  return c;
}

void TouchControls::press(int b, const Layout& l) {
  switch (b) {
    case kJump: out_.jumpPressed = true; break;
    case kSneak:
      if (!l.flying) sneakToggle_ = !sneakToggle_;
      break;
    case kFly: out_.toggleFly = true; break;
    case kInventory: out_.inventory = true; break;
    case kChat: out_.chat = true; break;
    case kPause: out_.pause = true; break;
    default: break;
  }
}

void TouchControls::update(const std::vector<Input::TouchEvent>& events, const Layout& l, double now) {
  // One-frame outputs.
  out_.jumpPressed = out_.use = out_.pause = out_.inventory = out_.chat = out_.toggleFly = false;
  out_.hotbarSlot = -1;
  out_.look = vec2(0.0f, 0.0f);
  const float u = std::max(l.unit, 0.01f);

  for (const Input::TouchEvent& e : events) {
    if (e.phase == Input::TouchPhase::Began) {
      if (find(e.id)) continue;
      Finger f;
      f.id = e.id;
      f.start = f.pos = e.pos;
      f.startTime = now;
      const int slot = hotbarSlotAt(e.pos, l);
      const int b = slot < 0 ? buttonAt(e.pos, l) : -1;
      const bool stickTaken = std::any_of(fingers_.begin(), fingers_.end(),
                                          [](const Finger& o) { return o.role == Role::Stick; });
      if (slot >= 0) {
        f.role = Role::Hotbar;
        out_.hotbarSlot = slot;
      } else if (b >= 0) {
        f.role = Role::Button;
        f.button = b;
        press(b, l);
      } else if (!stickTaken && e.pos.x < l.width * 0.42f && e.pos.y > l.height * 0.3f) {
        f.role = Role::Stick;
      } else {
        f.role = Role::Look;
      }
      fingers_.push_back(f);
      continue;
    }
    Finger* f = find(e.id);
    if (!f) continue;  // began before the controls were active
    const vec2 d = (e.pos - f->pos) * (1.0f / u);
    f->travel += length(d);
    if (f->role == Role::Look) out_.look += d;
    f->pos = e.pos;
    if (e.phase == Input::TouchPhase::Ended || e.phase == Input::TouchPhase::Cancelled) {
      if (e.phase == Input::TouchPhase::Ended && f->role == Role::Look && !f->holding && f->travel <= kTapSlop &&
          now - f->startTime < kHoldTime) {
        out_.use = true;
        out_.aim = f->pos;
      }
      const int id = e.id;
      fingers_.erase(std::remove_if(fingers_.begin(), fingers_.end(), [id](const Finger& o) { return o.id == id; }),
                     fingers_.end());
    }
  }

  // Continuous state from the fingers that are down.
  out_.forward = out_.strafe = 0.0f;
  out_.sprint = out_.jump = out_.aiming = out_.breaking = false;
  bool sneakHeld = false;
  for (Finger& f : fingers_) {
    switch (f.role) {
      case Role::Stick: {
        vec2 off = (f.pos - f.start) * (1.0f / u);
        float dist = length(off);
        if (dist > kStickRadius * kFollowReach) {
          const vec2 dir = off * (1.0f / dist);
          f.start = f.start + dir * ((dist - kStickRadius * kFollowReach) * u);
          off = (f.pos - f.start) * (1.0f / u);
          dist = length(off);
        }
        vec2 v = off * (1.0f / kStickRadius);
        const float m = length(v);
        if (m > 1.0f) v = v * (1.0f / m);
        if (m >= kDeadZone) {
          out_.forward = -v.y;
          out_.strafe = v.x;
        }
        out_.sprint = dist > kStickRadius * kSprintReach && out_.forward > 0.6f;
        break;
      }
      case Role::Look:
        if (!f.holding && f.travel <= kTapSlop && now - f.startTime >= kHoldTime) f.holding = true;
        if (f.holding) {
          out_.breaking = true;
          out_.aiming = true;
          out_.aim = f.pos;
        }
        break;
      case Role::Button:
        if (f.button == kJump) out_.jump = true;
        if (f.button == kSneak) sneakHeld = true;
        break;
      case Role::Hotbar: break;
    }
  }
  if (l.flying) sneakToggle_ = false;
  out_.sneak = l.flying ? sneakHeld : sneakToggle_;
}

void TouchControls::draw(UiRenderer& ui, const Layout& l) const {
  const float u = l.unit;
  const uint32_t fill = rgba(16, 20, 26, 105);
  const uint32_t fillOn = rgba(235, 240, 250, 95);
  const uint32_t edge = rgba(255, 255, 255, 120);
  const uint32_t glyph = rgba(245, 248, 255, 215);
  auto held = [this](int b) {
    return std::any_of(fingers_.begin(), fingers_.end(),
                       [b](const Finger& f) { return f.role == Role::Button && f.button == b; });
  };

  // Movement stick.
  const Finger* stick = nullptr;
  for (const Finger& f : fingers_)
    if (f.role == Role::Stick) stick = &f;
  const float r = kStickRadius * u;
  if (stick) {
    vec2 off = stick->pos - stick->start;
    const float d = length(off);
    if (d > r) off = off * (r / d);
    ui.disc(stick->start.x, stick->start.y, r, fill);
    ui.ring(stick->start.x, stick->start.y, r, 2.0f * u, out_.sprint ? rgba(255, 214, 120, 200) : edge);
    ui.disc(stick->start.x + off.x, stick->start.y + off.y, r * 0.42f, rgba(235, 240, 250, 150));
  } else {
    const vec2 c = stickRest(l);
    ui.ring(c.x, c.y, r, 2.0f * u, rgba(255, 255, 255, 60));
    ui.disc(c.x, c.y, r * 0.42f, rgba(235, 240, 250, 55));
  }

  for (int b = 0; b < kButtonCount; ++b) {
    const Circle c = button(b, l);
    if (!c.visible) continue;
    const bool on = held(b) || (b == kSneak && sneakToggle_) || (b == kFly && l.flying);
    ui.disc(c.c.x, c.c.y, c.r, on ? fillOn : fill);
    ui.ring(c.c.x, c.c.y, c.r, 2.0f * u, edge);
    const float s = c.r * 0.42f;
    const float x = c.c.x, y = c.c.y;
    switch (b) {
      case kJump: ui.triangle(x - s, y + s * 0.6f, x + s, y + s * 0.6f, x, y - s * 0.8f, glyph); break;
      case kSneak: ui.triangle(x - s, y - s * 0.6f, x, y + s * 0.8f, x + s, y - s * 0.6f, glyph); break;
      case kFly: {
        // Two chevrons: up and away.
        ui.triangle(x - s, y, x + s, y, x, y - s, glyph);
        ui.triangle(x - s, y + s * 0.9f, x + s, y + s * 0.9f, x, y - s * 0.1f, glyph);
        break;
      }
      case kInventory: {
        const float q = s * 0.8f, gap = s * 0.25f;
        for (int iy = 0; iy < 2; ++iy)
          for (int ix = 0; ix < 2; ++ix)
            ui.rect(x - q - gap * 0.5f + static_cast<float>(ix) * (q + gap),
                    y - q - gap * 0.5f + static_cast<float>(iy) * (q + gap), q, q, glyph);
        break;
      }
      case kChat: {
        // Speech bubble.
        ui.rect(x - s * 1.1f, y - s * 0.8f, s * 2.2f, s * 1.3f, glyph);
        ui.triangle(x - s * 0.6f, y + s * 0.5f, x - s * 0.1f, y + s * 0.5f, x - s * 0.7f, y + s * 1.1f, glyph);
        break;
      }
      case kPause: {
        ui.rect(x - s * 0.75f, y - s * 0.85f, s * 0.5f, s * 1.7f, glyph);
        ui.rect(x + s * 0.25f, y - s * 0.85f, s * 0.5f, s * 1.7f, glyph);
        break;
      }
      default: break;
    }
  }

  // A ring under a finger that is breaking.
  if (out_.breaking) ui.ring(out_.aim.x, out_.aim.y, 26.0f * u, 3.0f * u, rgba(255, 255, 255, 150));
}

}  // namespace strata
