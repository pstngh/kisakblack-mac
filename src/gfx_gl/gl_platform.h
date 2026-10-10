#pragma once
// gl_platform.h — which GL the backend targets.
//
//   KB_GL_CORE  desktop core profile: macOS, where GL 4.1 core is all there is
//               (Apple's compatibility profile stops at 2.1)
//   otherwise   desktop compatibility context with #version 120 GLSL (Linux)
//
// The core profile has no fixed-function alpha test (shaders discard against
// uAlphaTestFunc/uAlphaRef instead) and no luminance texture formats, and takes the
// in/out GLSL dialect (KB_GL_MODERN_GLSL): the translator writes GLSL ES 3.00 and
// KB_GLSLForContext turns it into GLSL 4.10.

#include <cstring>
#include <string>

#if   defined(__APPLE__)
#define KB_GL_CORE 1
#endif

#if defined(KB_GL_CORE)
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
