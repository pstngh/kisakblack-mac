#pragma once
// gl_platform.h — which GL the backend targets.
//
//   KB_GL_ES    WebGL2 / GLES 3 (Emscripten builds)
//   KB_GL_CORE  desktop core profile: macOS, where GL 4.1 core is all there is
//               (Apple's compatibility profile stops at 2.1)
//   neither     desktop compatibility context with #version 120 GLSL (Linux)
//
// ES and core share what matters to the D3D9 translation: no fixed-function
// alpha test (shaders discard against uAlphaTestFunc/uAlphaRef instead), no
// luminance texture formats, and the in/out GLSL dialect (KB_GL_MODERN_GLSL).
// Browser-specific code stays under __EMSCRIPTEN__.

#include <cstring>
#include <string>

#if defined(__EMSCRIPTEN__)
#define KB_GL_ES 1
#elif defined(__APPLE__)
#define KB_GL_CORE 1
#endif

#if defined(KB_GL_ES) || defined(KB_GL_CORE)
#define KB_GL_MODERN_GLSL 1
#endif

// Shader source as given to glShaderSource. The translator and the built-in
// programs write GLSL ES 3.00; a core context gets the same code as desktop GLSL
// 4.10: another #version line, and no precision statements (desktop GLSL ignores
// precision, and only accepts it on float/int).
inline std::string KB_GLSLForContext(const char *src)
{
#if defined(KB_GL_CORE)
    std::string out;
    const char *p = src;
    while (*p)
    {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) + 1 : strlen(p);
        if (!strncmp(p, "#version 300 es", 15))
            out += "#version 410 core\n";
        else if (strncmp(p, "precision ", 10))
            out.append(p, len);
        p += len;
    }
    return out;
#else
    return src;
#endif
}
