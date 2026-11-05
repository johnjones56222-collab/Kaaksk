// Writes every shader program, assembled for both GLSL dialects (desktop 3.30
// and ES 3.00), so the sources can be validated without a GPU. See
// tools/check_shaders.py.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "gl/shader.hpp"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <output directory>\n", argv[0]);
    return 2;
  }
  namespace fs = std::filesystem;
  const fs::path out = argv[1];
  std::error_code ec;
  fs::create_directories(out, ec);
  int written = 0;
  for (int i = 0; i < strata::shaders::kProgramCount; ++i) {
    const strata::shaders::Program& p = strata::shaders::kPrograms[i];
    std::string base = p.name;
    for (char& c : base)
      if (c == ' ') c = '_';
    for (const strata::GlslDialect d : {strata::GlslDialect::Desktop, strata::GlslDialect::Es}) {
      for (const bool fragment : {false, true}) {
        const fs::path file =
            out / (base + (d == strata::GlslDialect::Es ? ".es" : ".gl") + (fragment ? ".frag" : ".vert"));
        std::ofstream f(file, std::ios::binary);
        for (const std::string& part : strata::assembleShader(p, fragment, d)) f << part;
        if (!f) {
          std::fprintf(stderr, "could not write %s\n", file.string().c_str());
          return 1;
        }
        ++written;
      }
    }
  }
  std::printf("wrote %d shader files to %s\n", written, out.string().c_str());
  return 0;
}
