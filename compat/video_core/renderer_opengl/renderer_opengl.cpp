// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <span>
#include <glad/glad.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/telemetry.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/memory.h"
#include "core/perf_stats.h"
#include "common/settings.h"
#include "core/telemetry_session.h"
#include "video_core/morton.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

namespace OpenGL {

namespace {

constexpr char VERTEX_SHADER[] = R"(
#version 430 core

out gl_PerVertex {
    vec4 gl_Position;
};

layout (location = 0) in vec2 vert_position;
layout (location = 1) in vec2 vert_tex_coord;
layout (location = 0) out vec2 frag_tex_coord;

// This is a truncated 3x3 matrix for 2D transformations:
// The upper-left 2x2 submatrix performs scaling/rotation/mirroring.
// The third column performs translation.
// The third row could be used for projection, which we don't need in 2D. It hence is assumed to
// implicitly be [0, 0, 1]
layout (location = 0) uniform mat3x2 modelview_matrix;

void main() {
    // Multiply input position by the rotscale part of the matrix and then manually translate by
    // the last column. This is equivalent to using a full 3x3 matrix and expanding the vector
    // to `vec3(vert_position.xy, 1.0)`
    gl_Position = vec4(mat2(modelview_matrix) * vert_position + modelview_matrix[2], 0.0, 1.0);
    frag_tex_coord = vert_tex_coord;
}
)";

constexpr char FRAGMENT_SHADER[] = R"(
#version 430 core

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D color_texture;

void main() {
    color = vec4(texture(color_texture, frag_tex_coord).rgb, 1.0f);
}
)";

constexpr GLint PositionLocation = 0;
constexpr GLint TexCoordLocation = 1;
constexpr GLint ModelViewMatrixLocation = 0;

struct ScreenRectVertex {
    constexpr ScreenRectVertex(u32 x, u32 y, GLfloat u, GLfloat v)
        : position{{static_cast<GLfloat>(x), static_cast<GLfloat>(y)}}, tex_coord{{u, v}} {}

    std::array<GLfloat, 2> position;
    std::array<GLfloat, 2> tex_coord;
};

/**
 * Defines a 1:1 pixel ortographic projection matrix with (0,0) on the top-left
 * corner and (width, height) on the lower-bottom.
 *
 * The projection part of the matrix is trivial, hence these operations are represented
 * by a 3x2 matrix.
 */
std::array<GLfloat, 3 * 2> MakeOrthographicMatrix(float width, float height) {
    std::array<GLfloat, 3 * 2> matrix; // Laid out in column-major order

    // clang-format off
    matrix[0] = 2.f / width; matrix[2] =  0.f;          matrix[4] = -1.f;
    matrix[1] = 0.f;         matrix[3] = -2.f / height; matrix[5] =  1.f;
    // Last matrix row is implicitly assumed to be [0, 0, 1].
    // clang-format on

    return matrix;
}

const char* GetSource(GLenum source) {
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        return "WINDOW_SYSTEM";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        return "SHADER_COMPILER";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        return "THIRD_PARTY";
    case GL_DEBUG_SOURCE_APPLICATION:
        return "APPLICATION";
    case GL_DEBUG_SOURCE_OTHER:
        return "OTHER";
    default:
        UNREACHABLE();
        return "Unknown source";
    }
}

const char* GetType(GLenum type) {
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        return "ERROR";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        return "DEPRECATED_BEHAVIOR";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        return "UNDEFINED_BEHAVIOR";
    case GL_DEBUG_TYPE_PORTABILITY:
        return "PORTABILITY";
    case GL_DEBUG_TYPE_PERFORMANCE:
        return "PERFORMANCE";
    case GL_DEBUG_TYPE_OTHER:
        return "OTHER";
    case GL_DEBUG_TYPE_MARKER:
        return "MARKER";
    default:
        UNREACHABLE();
        return "Unknown type";
    }
}

void APIENTRY DebugHandler(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                           const GLchar* message, const void* user_param) {
    const char format[] = "{} {} {}: {}";
    const char* const str_source = GetSource(source);
    const char* const str_type = GetType(type);

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        LOG_CRITICAL(Render_OpenGL, format, str_source, str_type, id, message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        LOG_WARNING(Render_OpenGL, format, str_source, str_type, id, message);
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    case GL_DEBUG_SEVERITY_LOW:
        LOG_DEBUG(Render_OpenGL, format, str_source, str_type, id, message);
        break;
    }
}

} // Anonymous namespace

RendererOpenGL::RendererOpenGL(Tegra::GPU& gpu,
                               std::unique_ptr<Core::Frontend::GraphicsContext> context)
    : VideoCore::RendererBase{gpu.RenderWindow(), std::move(context)},
      gpu{gpu}, emu_window{gpu.RenderWindow()},
      state_tracker{gpu} {}

RendererOpenGL::~RendererOpenGL() = default;

MICROPROFILE_DEFINE(OpenGL_RenderFrame, "OpenGL", "Render Frame", MP_RGB(128, 128, 64));
MICROPROFILE_DEFINE(OpenGL_WaitPresent, "OpenGL", "Wait For Present", MP_RGB(128, 128, 128));

void RendererOpenGL::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    if (!framebuffer) {
        return;
    }

    PrepareRendertarget(framebuffer);
    RenderScreenshot();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    DrawScreen(render_window.GetFramebufferLayout());

    m_current_frame++;

    gpu.RendererFrameEndNotify();
    rasterizer->TickFrame();

    context->SwapBuffers();
    render_window.OnFrameDisplayed();
}

void RendererOpenGL::PrepareRendertarget(const Tegra::FramebufferConfig* framebuffer) {
    if (framebuffer) {
        // If framebuffer is provided, reload it from memory to a texture
        if (screen_info.texture.width != static_cast<GLsizei>(framebuffer->width) ||
            screen_info.texture.height != static_cast<GLsizei>(framebuffer->height) ||
            screen_info.texture.pixel_format != framebuffer->pixel_format ||
            gl_framebuffer_data.empty()) {
            // Reallocate texture if the framebuffer size has changed.
            // This is expected to not happen very often and hence should not be a
            // performance problem.
            ConfigureFramebufferTexture(screen_info.texture, *framebuffer);
        }

        // Load the framebuffer from memory, draw it to the screen, and swap buffers
        LoadFBToScreenInfo(*framebuffer);
    }
}

void RendererOpenGL::LoadFBToScreenInfo(const Tegra::FramebufferConfig& framebuffer) {
    // Framebuffer orientation handling
    framebuffer_transform_flags = framebuffer.transform_flags;
    framebuffer_crop_rect = framebuffer.crop_rect;

    const VAddr framebuffer_addr{framebuffer.address + framebuffer.offset};
    if (rasterizer->AccelerateDisplay(framebuffer, framebuffer_addr, framebuffer.stride)) {
        return;
    }

    // Reset the screen info's display texture to its own permanent texture
    screen_info.display_texture = screen_info.texture.resource.handle;

    constexpr u32 block_height_log2 = 4;
    const auto pixel_format{
        VideoCore::Surface::PixelFormatFromGPUPixelFormat(framebuffer.pixel_format)};
    const u32 bytes_per_pixel{VideoCore::Surface::GetBytesPerPixel(pixel_format)};
    const u64 size_in_bytes{Tegra::Texture::CalculateSize(
        true, bytes_per_pixel, framebuffer.stride, framebuffer.height, 1, block_height_log2, 0)};
    std::unique_ptr<u8[]> host_data(new u8[size_in_bytes]);
    horizon_servctl_read_buffer_from(framebuffer_addr, (u8 *)host_data.get(), size_in_bytes,
                                     framebuffer.session_pid);
    rasterizer->FlushTextureRegion(framebuffer_addr, size_in_bytes);

    // TODO(Rodrigo): Read this from HLE
    VideoCore::MortonSwizzle(VideoCore::MortonSwizzleMode::MortonToLinear, pixel_format,
                             framebuffer.stride, block_height_log2, framebuffer.height, 0, 1, 1,
                             gl_framebuffer_data.data(), host_data.get());

    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(framebuffer.stride));

    // Update existing texture
    // TODO: Test what happens on hardware when you change the framebuffer dimensions so that
    //       they differ from the LCD resolution.
    // TODO: Applications could theoretically crash yuzu here by specifying too large
    //       framebuffer sizes. We should make sure that this cannot happen.
    glTextureSubImage2D(screen_info.texture.resource.handle, 0, 0, 0, framebuffer.width,
                        framebuffer.height, screen_info.texture.gl_format,
                        screen_info.texture.gl_type, gl_framebuffer_data.data());

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void RendererOpenGL::LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, u8 color_a,
                                                const TextureInfo& texture) {
    const u8 framebuffer_data[4] = {color_a, color_b, color_g, color_r};
    glClearTexImage(texture.resource.handle, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer_data);
}

void RendererOpenGL::InitOpenGLObjects() {
    glClearColor(Settings::values.bg_red.GetValue(),
                 Settings::values.bg_green.GetValue(),
                 Settings::values.bg_blue.GetValue(),
                 0.0f);

    // Create shader programs
    OGLShader vertex_shader;
    vertex_shader.Create(VERTEX_SHADER, GL_VERTEX_SHADER);

    OGLShader fragment_shader;
    fragment_shader.Create(FRAGMENT_SHADER, GL_FRAGMENT_SHADER);

    vertex_program.Create(true, false, vertex_shader.handle);
    fragment_program.Create(true, false, fragment_shader.handle);

    // Create program pipeline
    program_manager.Create();

    // Generate VBO handle for drawing
    vertex_buffer.Create();

    // Attach vertex data to VAO
    glNamedBufferData(vertex_buffer.handle, sizeof(ScreenRectVertex) * 4, nullptr, GL_STREAM_DRAW);

    // Allocate textures for the screen
    screen_info.texture.resource.Create(GL_TEXTURE_2D);

    const GLuint texture = screen_info.texture.resource.handle;
    glTextureStorage2D(texture, 1, GL_RGBA8, 1, 1);

    screen_info.display_texture = screen_info.texture.resource.handle;

    // Clear screen to black
    LoadColorToActiveGLTexture(0, 0, 0, 0, screen_info.texture);
}

void RendererOpenGL::AddTelemetryFields() {
    const char* const gl_version{reinterpret_cast<char const*>(glGetString(GL_VERSION))};
    const char* const gpu_vendor{reinterpret_cast<char const*>(glGetString(GL_VENDOR))};
    const char* const gpu_model{reinterpret_cast<char const*>(glGetString(GL_RENDERER))};

    LOG_INFO(Render_OpenGL, "GL_VERSION: {}", gl_version);
    LOG_INFO(Render_OpenGL, "GL_VENDOR: {}", gpu_vendor);
    LOG_INFO(Render_OpenGL, "GL_RENDERER: {}", gpu_model);

    auto& telemetry_session = gpu.TelemetrySession();
    telemetry_session.AddField(Common::Telemetry::FieldType::UserSystem, "GPU_Vendor", gpu_vendor);
    telemetry_session.AddField(Common::Telemetry::FieldType::UserSystem, "GPU_Model", gpu_model);
    telemetry_session.AddField(Common::Telemetry::FieldType::UserSystem, "GPU_OpenGL_Version", gl_version);
}

void RendererOpenGL::CreateRasterizer() {
    if (rasterizer) {
        return;
    }
    rasterizer = std::make_unique<RasterizerOpenGL>(emu_window, screen_info,
                                                    program_manager, state_tracker);
}

void RendererOpenGL::ConfigureFramebufferTexture(TextureInfo& texture,
                                                 const Tegra::FramebufferConfig& framebuffer) {
    texture.width = framebuffer.width;
    texture.height = framebuffer.height;
    texture.pixel_format = framebuffer.pixel_format;

    const auto pixel_format{
        VideoCore::Surface::PixelFormatFromGPUPixelFormat(framebuffer.pixel_format)};
    const u32 bytes_per_pixel{VideoCore::Surface::GetBytesPerPixel(pixel_format)};
    gl_framebuffer_data.resize(texture.width * texture.height * bytes_per_pixel);

    GLint internal_format;
    switch (framebuffer.pixel_format) {
    case Tegra::FramebufferConfig::PixelFormat::ABGR8:
        internal_format = GL_RGBA8;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
        break;
    case Tegra::FramebufferConfig::PixelFormat::RGB565:
        internal_format = GL_RGB565;
        texture.gl_format = GL_RGB;
        texture.gl_type = GL_UNSIGNED_SHORT_5_6_5;
        break;
    default:
        internal_format = GL_RGBA8;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
        UNIMPLEMENTED_MSG("Unknown framebuffer pixel format: {}",
                          static_cast<u32>(framebuffer.pixel_format));
    }

    texture.resource.Release();
    texture.resource.Create(GL_TEXTURE_2D);
    glTextureStorage2D(texture.resource.handle, 1, internal_format, texture.width, texture.height);
}

void RendererOpenGL::DrawScreen(const Layout::FramebufferLayout& layout) {
    if (renderer_settings.set_background_color) {
        // Update background color before drawing
        glClearColor(Settings::values.bg_red.GetValue(),
                     Settings::values.bg_green.GetValue(),
                     Settings::values.bg_blue.GetValue(),
                     0.0f);
    }

    // Set projection matrix
    const std::array ortho_matrix =
        MakeOrthographicMatrix(static_cast<float>(layout.width), static_cast<float>(layout.height));
    glProgramUniformMatrix3x2fv(vertex_program.handle, ModelViewMatrixLocation, 1, GL_FALSE,
                                std::data(ortho_matrix));

    const auto& texcoords = screen_info.display_texcoords;
    auto left = texcoords.left;
    auto right = texcoords.right;
    if (framebuffer_transform_flags != Tegra::FramebufferConfig::TransformFlags::Unset) {
        if (framebuffer_transform_flags == Tegra::FramebufferConfig::TransformFlags::FlipV) {
            // Flip the framebuffer vertically
            left = texcoords.right;
            right = texcoords.left;
        } else {
            // Other transformations are unsupported
            LOG_CRITICAL(Render_OpenGL, "Unsupported framebuffer_transform_flags={}",
                         static_cast<u32>(framebuffer_transform_flags));
            UNIMPLEMENTED();
        }
    }

    ASSERT_MSG(framebuffer_crop_rect.top == 0, "Unimplemented");
    ASSERT_MSG(framebuffer_crop_rect.left == 0, "Unimplemented");

    // Scale the output by the crop width/height. This is commonly used with 1280x720 rendering
    // (e.g. handheld mode) on a 1920x1080 framebuffer.
    f32 scale_u = 1.f, scale_v = 1.f;
    if (framebuffer_crop_rect.GetWidth() > 0) {
        scale_u = static_cast<f32>(framebuffer_crop_rect.GetWidth()) /
                  static_cast<f32>(screen_info.texture.width);
    }
    if (framebuffer_crop_rect.GetHeight() > 0) {
        scale_v = static_cast<f32>(framebuffer_crop_rect.GetHeight()) /
                  static_cast<f32>(screen_info.texture.height);
    }

    const auto& screen = layout.screen;
    const std::array vertices = {
        ScreenRectVertex(screen.left, screen.top, texcoords.top * scale_u, left * scale_v),
        ScreenRectVertex(screen.right, screen.top, texcoords.bottom * scale_u, left * scale_v),
        ScreenRectVertex(screen.left, screen.bottom, texcoords.top * scale_u, right * scale_v),
        ScreenRectVertex(screen.right, screen.bottom, texcoords.bottom * scale_u, right * scale_v),
    };
    glNamedBufferSubData(vertex_buffer.handle, 0, sizeof(vertices), std::data(vertices));

    // TODO: Signal state tracker about these changes
    state_tracker.NotifyScreenDrawVertexArray();
    state_tracker.NotifyPolygonModes();
    state_tracker.NotifyViewport0();
    state_tracker.NotifyScissor0();
    state_tracker.NotifyColorMask0();
    state_tracker.NotifyBlend0();
    state_tracker.NotifyFramebuffer();
    state_tracker.NotifyFrontFace();
    state_tracker.NotifyCullTest();
    state_tracker.NotifyDepthTest();
    state_tracker.NotifyStencilTest();
    state_tracker.NotifyPolygonOffset();
    state_tracker.NotifyRasterizeEnable();
    state_tracker.NotifyFramebufferSRGB();
    state_tracker.NotifyLogicOp();
    state_tracker.NotifyClipControl();
    state_tracker.NotifyAlphaTest();

    program_manager.UseVertexShader(vertex_program.handle);
    program_manager.UseGeometryShader(0);
    program_manager.UseFragmentShader(fragment_program.handle);
    program_manager.BindGraphicsPipeline();

    glEnable(GL_CULL_FACE);
    if (screen_info.display_srgb) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    } else {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_ALPHA_TEST);
    glDisablei(GL_BLEND, 0);
    glDisablei(GL_SCISSOR_TEST, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glViewportIndexedf(0, 0.0f, 0.0f, static_cast<GLfloat>(layout.width),
                       static_cast<GLfloat>(layout.height));
    glDepthRangeIndexed(0, 0.0, 0.0);

    glEnableVertexAttribArray(PositionLocation);
    glEnableVertexAttribArray(TexCoordLocation);
    glVertexAttribDivisor(PositionLocation, 0);
    glVertexAttribDivisor(TexCoordLocation, 0);
    glVertexAttribFormat(PositionLocation, 2, GL_FLOAT, GL_FALSE,
                         offsetof(ScreenRectVertex, position));
    glVertexAttribFormat(TexCoordLocation, 2, GL_FLOAT, GL_FALSE,
                         offsetof(ScreenRectVertex, tex_coord));
    glVertexAttribBinding(PositionLocation, 0);
    glVertexAttribBinding(TexCoordLocation, 0);
    glBindVertexBuffer(0, vertex_buffer.handle, 0, sizeof(ScreenRectVertex));

    glBindTextureUnit(0, screen_info.display_texture);
    glBindSampler(0, 0);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void RendererOpenGL::RenderScreenshot() {
    if (!renderer_settings.screenshot_requested) {
        return;
    }

    GLint old_read_fb;
    GLint old_draw_fb;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read_fb);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw_fb);

    // Draw the current frame to the screenshot framebuffer
    screenshot_framebuffer.Create();
    glBindFramebuffer(GL_FRAMEBUFFER, screenshot_framebuffer.handle);

    Layout::FramebufferLayout layout{renderer_settings.screenshot_framebuffer_layout};

    GLuint renderbuffer;
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, screen_info.display_srgb ? GL_SRGB8 : GL_RGB8,
                          layout.width, layout.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);

    DrawScreen(layout);

    glReadPixels(0, 0, layout.width, layout.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                 renderer_settings.screenshot_bits);

    screenshot_framebuffer.Release();
    glDeleteRenderbuffers(1, &renderbuffer);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, old_read_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_draw_fb);

    renderer_settings.screenshot_complete_callback(true);
    renderer_settings.screenshot_requested = false;
}

bool RendererOpenGL::Init() {
    if (GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(DebugHandler, nullptr);
    }

    AddTelemetryFields();

    if (!GLAD_GL_VERSION_4_3) {
        return false;
    }

    InitOpenGLObjects();
    CreateRasterizer();

    return true;
}

void RendererOpenGL::ShutDown() {}

} // namespace OpenGL
