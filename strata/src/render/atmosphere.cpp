#include "render/atmosphere.hpp"

#include <cmath>

namespace strata {

namespace {
// Scattering / extinction coefficients per km.
const vec3 kRayleigh(5.802e-3f, 13.558e-3f, 33.1e-3f);
constexpr float kMieScatter = 3.996e-3f;
constexpr float kMieExtinction = 4.440e-3f;
const vec3 kOzone(0.650e-3f, 1.881e-3f, 0.085e-3f);
constexpr float kRayleighHeight = 8.0f;
constexpr float kMieHeight = 1.2f;
constexpr float kMieG = 0.8f;

// Distance along a ray from `o` (km, planet centre at origin) to a sphere, or -1.
float raySphere(vec3 o, vec3 d, float radius, bool farHit) {
  const float b = dot(o, d);
  const float c = dot(o, o) - radius * radius;
  const float disc = b * b - c;
  if (disc < 0.0f) return -1.0f;
  const float s = std::sqrt(disc);
  const float t = farHit ? -b + s : -b - s;
  return t;
}

struct Densities {
  float rayleigh, mie, ozone;
};
Densities densityAt(float altitude) {
  Densities d;
  d.rayleigh = std::exp(-altitude / kRayleighHeight);
  d.mie = std::exp(-altitude / kMieHeight);
  d.ozone = std::max(0.0f, 1.0f - std::fabs(altitude - 25.0f) / 15.0f);
  return d;
}

vec3 extinction(const Densities& od) { return kRayleigh * od.rayleigh + vec3(kMieExtinction * od.mie) + kOzone * od.ozone; }

// Optical depth from point p (km) to the top of the atmosphere along d.
Densities opticalDepthToSpace(vec3 p, vec3 d, int steps) {
  Densities od{0, 0, 0};
  const float t = raySphere(p, d, AtmosphereModel::kTopRadius, true);
  if (t <= 0.0f) return od;
  const float ds = t / static_cast<float>(steps);
  for (int i = 0; i < steps; ++i) {
    const vec3 q = p + d * ((static_cast<float>(i) + 0.5f) * ds);
    const Densities den = densityAt(length(q) - AtmosphereModel::kGroundRadius);
    od.rayleigh += den.rayleigh * ds;
    od.mie += den.mie * ds;
    od.ozone += den.ozone * ds;
  }
  return od;
}

vec3 viewOrigin() { return vec3(0.0f, AtmosphereModel::kGroundRadius + AtmosphereModel::kViewAltitude, 0.0f); }
}  // namespace

vec3 AtmosphereModel::transmittance(vec3 dir) {
  const vec3 o = viewOrigin();
  const vec3 d = normalize(dir);
  if (raySphere(o, d, kGroundRadius, false) > 0.0f) return vec3(0.0f);
  const Densities od = opticalDepthToSpace(o, d, 24);
  return vexp(-extinction(od));
}

vec3 AtmosphereModel::skyRadiance(vec3 dir, vec3 lightDir, float intensity, vec3 ambient) {
  const vec3 o = viewOrigin();
  const vec3 d = normalize(dir);
  const vec3 l = normalize(lightDir);
  float tMax = raySphere(o, d, kTopRadius, true);
  const float tGround = raySphere(o, d, kGroundRadius, false);
  if (tGround > 0.0f) tMax = tGround;
  if (tMax <= 0.0f) return vec3(0.0f);
  // Samples are packed towards the viewer (quadratic spacing): near-horizon rays
  // are hundreds of km long but the dense air close by dominates the result.
  constexpr int kSteps = 24;
  const float n2 = static_cast<float>(kSteps * kSteps);
  vec3 sumR(0.0f), sumM(0.0f), sumMs(0.0f);
  Densities view{0, 0, 0};
  for (int i = 0; i < kSteps; ++i) {
    const float fi = static_cast<float>(i);
    const float t = tMax * (fi + 0.5f) * (fi + 0.5f) / n2;
    const float ds = tMax * (2.0f * fi + 1.0f) / n2;
    const vec3 p = o + d * t;
    const Densities den = densityAt(length(p) - kGroundRadius);
    view.rayleigh += den.rayleigh * ds;
    view.mie += den.mie * ds;
    view.ozone += den.ozone * ds;
    sumMs += vexp(-extinction(view)) * (kRayleigh * den.rayleigh + vec3(kMieScatter * den.mie)) * ds;
    if (raySphere(p, l, kGroundRadius, false) > 0.0f) continue;  // in the planet's shadow
    const Densities sun = opticalDepthToSpace(p, l, 8);
    const Densities total{view.rayleigh + sun.rayleigh, view.mie + sun.mie, view.ozone + sun.ozone};
    const vec3 att = vexp(-extinction(total));
    sumR += att * (den.rayleigh * ds);
    sumM += att * (den.mie * ds);
  }
  const float mu = dot(d, l);
  const float phaseR = 3.0f / (16.0f * kPi) * (1.0f + mu * mu);
  const float g2 = kMieG * kMieG;
  const float phaseM = 3.0f / (8.0f * kPi) * ((1.0f - g2) * (1.0f + mu * mu)) /
                       ((2.0f + g2) * std::pow(1.0f + g2 - 2.0f * kMieG * mu, 1.5f));
  return (sumR * kRayleigh * phaseR + sumM * (kMieScatter * phaseM)) * intensity + sumMs * ambient;
}

vec3 AtmosphereModel::skyAmbient(vec3 lightDir, float intensity, vec3 ambient) {
  // Cosine weighted integral over the upper hemisphere, divided by pi.
  constexpr int kAz = 8, kEl = 4;
  vec3 sum(0.0f);
  float weight = 0.0f;
  for (int e = 0; e < kEl; ++e) {
    const float el = (static_cast<float>(e) + 0.5f) / static_cast<float>(kEl) * kHalfPi;
    const float c = std::cos(el), s = std::sin(el);
    for (int a = 0; a < kAz; ++a) {
      const float az = (static_cast<float>(a) + 0.5f) / static_cast<float>(kAz) * kTwoPi;
      const vec3 d(std::cos(az) * c, s, std::sin(az) * c);
      // Solid angle element ~ cos(el); Lambert weight ~ sin(el).
      const float w = c * s;
      sum += skyRadiance(d, lightDir, intensity, ambient) * w;
      weight += w;
    }
  }
  // Average radiance weighted by cos(theta) over the hemisphere equals E / pi.
  return sum / weight;
}

DaylightState computeDaylight(float timeOfDay) {
  DaylightState s;
  const float angle = timeOfDay * kTwoPi;
  // The sun rises in the east (+X) and crosses the southern sky (+Z).
  constexpr float kTilt = 0.42f;  // ~24 degrees towards the south
  s.sunDir = normalize(vec3(std::cos(angle), std::sin(angle) * std::cos(kTilt), std::sin(angle) * std::sin(kTilt)));
  s.moonDir = -s.sunDir;
  s.sunHeight = s.sunDir.y;

  const vec3 sunT = AtmosphereModel::transmittance(s.sunDir);
  const vec3 moonT = AtmosphereModel::transmittance(s.moonDir);
  // Direct sun light; the extra fade hides the sun disc sinking below the horizon.
  const float sunFade = smoothstepf(-0.02f, 0.06f, s.sunDir.y);
  const vec3 sunLight = sunT * (AtmosphereModel::kSunIntensity / kPi) * sunFade;
  // Moonlight: dim and slightly blue (the eye's night vision shift).
  const float moonFade = smoothstepf(-0.02f, 0.1f, s.moonDir.y);
  const vec3 moonLight = moonT * vec3(0.55f, 0.68f, 1.0f) * (0.075f * moonFade);

  s.nightFactor = 1.0f - smoothstepf(-0.12f, 0.08f, s.sunDir.y);
  if (s.sunDir.y > -0.04f) {
    s.lightDir = s.sunDir;
    s.lightColor = sunLight;
  } else {
    s.lightDir = s.moonDir;
    s.lightColor = moonLight;
  }
  // One extra iteration feeds the single-scattering ambient back in as an
  // approximation of multiple scattering (brighter, bluer sky light).
  const vec3 daySingle = AtmosphereModel::skyAmbient(s.sunDir);
  const vec3 daySky = AtmosphereModel::skyAmbient(s.sunDir, AtmosphereModel::kSunIntensity, daySingle);
  const vec3 nightSky = AtmosphereModel::skyAmbient(s.moonDir, AtmosphereModel::kSunIntensity * 0.0025f) +
                        vec3(0.0024f, 0.0030f, 0.0048f);  // starlight and airglow
  s.ambientColor = daySky + nightSky;
  const vec3 horizon = normalize(vec3(s.sunDir.x, 0.05f, s.sunDir.z));
  s.fogColor = AtmosphereModel::skyRadiance(horizon, s.sunDir, AtmosphereModel::kSunIntensity, daySky) +
               AtmosphereModel::skyRadiance(normalize(vec3(-s.sunDir.x, 0.05f, -s.sunDir.z)), s.sunDir,
                                            AtmosphereModel::kSunIntensity, daySky) +
               vec3(0.003f, 0.004f, 0.007f);
  s.fogColor = s.fogColor * 0.5f;
  return s;
}

}  // namespace strata
