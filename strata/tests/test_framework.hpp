// Minimal self-contained unit test framework.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct Case {
  const char* name;
  void (*fn)();
};

inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}

inline int& failures() {
  static int f = 0;
  return f;
}
inline int& checks() {
  static int c = 0;
  return c;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline int runAll(int argc, char** argv) {
  int ran = 0;
  for (const Case& c : registry()) {
    if (argc > 1 && std::strstr(c.name, argv[1]) == nullptr) continue;
    const int before = failures();
    std::printf("[ RUN  ] %s\n", c.name);
    std::fflush(stdout);
    c.fn();
    std::printf("[ %s ] %s\n", failures() == before ? " OK " : "FAIL", c.name);
    ++ran;
  }
  std::printf("\n%d tests, %d checks, %d failures\n", ran, checks(), failures());
  return failures() == 0 && ran > 0 ? 0 : 1;
}

}  // namespace testing

#define TEST(name)                                              \
  static void name();                                           \
  static ::testing::Registrar registrar_##name(#name, &name);   \
  static void name()

#define CHECK(cond)                                                                   \
  do {                                                                                \
    ++::testing::checks();                                                            \
    if (!(cond)) {                                                                    \
      ++::testing::failures();                                                        \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);   \
    }                                                                                 \
  } while (0)

#define CHECK_MSG(cond, ...)                                                          \
  do {                                                                                \
    ++::testing::checks();                                                            \
    if (!(cond)) {                                                                    \
      ++::testing::failures();                                                        \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s -- ", __FILE__, __LINE__, #cond); \
      std::fprintf(stderr, __VA_ARGS__);                                              \
      std::fprintf(stderr, "\n");                                                     \
    }                                                                                 \
  } while (0)

#define CHECK_NEAR(a, b, eps) CHECK_MSG(std::fabs((a) - (b)) <= (eps), "%g vs %g", (double)(a), (double)(b))
