#include "gl/gl.hpp"

#if defined(STRATA_GLES)

namespace strata::gl {
// ES entry points are linked directly; there is nothing to load.
bool loadFunctions(ProcLoader, const char**) { return true; }
bool hasDebugOutput() { return false; }
}  // namespace strata::gl

#else

#define STRATA_GL_DEFINE(ret, name, params, args) PFN_##name strata_ptr_##name = nullptr;
STRATA_GL_FUNCTIONS(STRATA_GL_DEFINE)
STRATA_GL_OPTIONAL_FUNCTIONS(STRATA_GL_DEFINE)
#undef STRATA_GL_DEFINE

namespace strata::gl {

namespace {
template <class Fn>
bool loadOne(ProcLoader loader, const char* name, Fn& out) {
  GenericProc p = loader(name);
  // Function pointer to function pointer casts are well defined; the pointer
  // is only ever called through its real type.
  out = reinterpret_cast<Fn>(p);
  return p != nullptr;
}
}  // namespace

bool loadFunctions(ProcLoader loader, const char** missing) {
  bool ok = true;
#define STRATA_GL_LOAD(ret, name, params, args)                       \
  if (!loadOne(loader, #name, strata_ptr_##name) && ok) {             \
    ok = false;                                                       \
    if (missing) *missing = #name;                                    \
  }
  STRATA_GL_FUNCTIONS(STRATA_GL_LOAD)
#undef STRATA_GL_LOAD
#define STRATA_GL_LOAD_OPTIONAL(ret, name, params, args) loadOne(loader, #name, strata_ptr_##name);
  STRATA_GL_OPTIONAL_FUNCTIONS(STRATA_GL_LOAD_OPTIONAL)
#undef STRATA_GL_LOAD_OPTIONAL
  return ok;
}

bool hasDebugOutput() { return strata_ptr_glDebugMessageCallback != nullptr; }

}  // namespace strata::gl

#endif  // STRATA_GLES
