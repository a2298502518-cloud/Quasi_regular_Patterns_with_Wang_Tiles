#include "render/opengl/ShaderProgram.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace qrp::render::opengl {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open shader: " + path.string());
    }
    std::ostringstream text;
    text << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        throw std::runtime_error("Failed while reading shader: " + path.string());
    }
    return text.str();
}

[[nodiscard]] GLuint compileShader(
    const GLenum type,
    const std::string& source,
    const std::filesystem::path& path) {
    const GLuint shader = glCreateShader(type);
    const char* data = source.c_str();
    glShaderSource(shader, 1, &data, nullptr);
    glCompileShader(shader);

    GLint succeeded = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &succeeded);
    if (succeeded == GL_TRUE) {
        return shader;
    }

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("Shader compilation failed for " + path.string() + ":\n" + log);
}

} // namespace

ShaderProgram::ShaderProgram(
    const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath) {
    const std::string vertexSource = readTextFile(vertexPath);
    const std::string fragmentSource = readTextFile(fragmentPath);
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource, vertexPath);
    GLuint fragment = 0;
    try {
        fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
        id_ = glCreateProgram();
        glAttachShader(id_, vertex);
        glAttachShader(id_, fragment);
        glLinkProgram(id_);

        GLint succeeded = GL_FALSE;
        glGetProgramiv(id_, GL_LINK_STATUS, &succeeded);
        if (succeeded != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &length);
            std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
            glGetProgramInfoLog(id_, length, nullptr, log.data());
            throw std::runtime_error("Shader program link failed:\n" + log);
        }
    } catch (...) {
        glDeleteShader(vertex);
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        if (id_ != 0) {
            glDeleteProgram(id_);
            id_ = 0;
        }
        throw;
    }
    glDetachShader(id_, vertex);
    glDetachShader(id_, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

ShaderProgram::~ShaderProgram() {
    if (id_ != 0) {
        glDeleteProgram(id_);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : id_(std::exchange(other.id_, 0)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        if (id_ != 0) {
            glDeleteProgram(id_);
        }
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}

GLuint ShaderProgram::id() const noexcept {
    return id_;
}

void ShaderProgram::use() const noexcept {
    glUseProgram(id_);
}

} // namespace qrp::render::opengl
