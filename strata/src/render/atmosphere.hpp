// Physically based single-scattering atmosphere (Rayleigh + Mie + ozone).
// The same model runs on the GPU for the sky; this CPU copy provides the
// light colours used to shade the world so that both always agree.
#pragma once

#include "core/math.hpp"

namespace strata {

struct AtmosphereModel {
  static constexpr float kGroundRadius = 6360.0f;  // km
  static constexpr float kTopRadius = 6460.0f;     // km
  static constexpr float kViewAltitude = 0.25f;    // km above the ground
  static constexpr float kSunIntensity = 20.0f;

  // Fraction of light that survives travelling from the viewer towards `dir`
  // to space (0 when the ray hits the planet).
  static vec3 transmittance(vec3 dir);
  // Scattered sky radiance seen in direction `dir` for a light from `lightDir`.
  // `ambient` approximates light scattered more than once: every sample point
  // also scatters this (isotropic) sky radiance towards the viewer.
  static vec3 skyRadiance(vec3 dir, vec3 lightDir, float intensity = kSunIntensity, vec3 ambient = vec3(0.0f));
  // Irradiance from the whole sky on an upward facing surface, divided by pi
  // (i.e. what multiplies a Lambertian albedo).
  static vec3 skyAmbient(vec3 lightDir, float intensity = kSunIntensity, vec3 ambient = vec3(0.0f));
};

// Sun and moon positions and resulting light for a time of day.
struct DaylightState {
  vec3 sunDir;        // unit vector towards the sun
  vec3 moonDir;
  vec3 lightDir;      // the dominant shadow casting light (sun by day, moon by night)
  vec3 lightColor;    // radiance multiplier for direct light (already divided by pi)
  vec3 ambientColor;  // sky ambient (already divided by pi)
  vec3 fogColor;      // horizon colour used for distant haze when the sky LUT is unavailable
  float sunHeight;    // sin(elevation) of the sun
  float nightFactor;  // 0 by day .. 1 at night
};

// timeOfDay in [0, 1): 0 = sunrise, 0.25 = noon, 0.5 = sunset, 0.75 = midnight.
DaylightState computeDaylight(float timeOfDay);

}  // namespace strata
