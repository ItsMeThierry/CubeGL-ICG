#include "shaders.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

static std::string read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "[shader] nao foi possivel abrir '%s'\n", path);
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile_stage(GLenum type, const std::string& src, const char* path) {
    GLuint sh = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(sh, 1, &c, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 0 ? len : 1);
        glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] erro compilando '%s':\n%s\n", path, log.data());
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint shader_load(const char* vertex_path, const char* fragment_path) {
    std::string vsrc = read_file(vertex_path);
    std::string fsrc = read_file(fragment_path);
    if (vsrc.empty() || fsrc.empty()) return 0;

    GLuint vs = compile_stage(GL_VERTEX_SHADER, vsrc, vertex_path);
    if (!vs) return 0;
    GLuint fs = compile_stage(GL_FRAGMENT_SHADER, fsrc, fragment_path);
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 0 ? len : 1);
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] erro linkando:\n%s\n", log.data());
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void shader_destroy(GLuint program) {
    if (program) glDeleteProgram(program);
}