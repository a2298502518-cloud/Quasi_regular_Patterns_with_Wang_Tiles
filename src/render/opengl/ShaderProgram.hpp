#pragma once

#include <glad/gl.h>

#include <filesystem>

namespace qrp::render::opengl {

class ShaderProgram final {
public:
    ShaderProgram(
        const std::filesystem::path& vertexPath,
        const std::filesystem::path& fragmentPath);
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    [[nodiscard]] GLuint id() const noexcept;
    void use() const noexcept;

private:
    GLuint id_ = 0;
};

} // namespace qrp::render::opengl
