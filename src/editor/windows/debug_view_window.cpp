#include "windows/debug_view_window.hpp"
#include "editor_imgui_windows.hpp"
#include "scene/scene_root.hpp"
#include "graphics/gl_context_provider.hpp"
#include "renderers/forward_renderer.hpp"
#include "renderers/programs.hpp"
#include "renderers/shadow_renderer.hpp"

#include "erhe/graphics/debug.hpp"
#include "erhe/graphics/framebuffer.hpp"
#include "erhe/graphics/opengl_state_tracker.hpp"
#include "erhe/graphics/sampler.hpp"
#include "erhe/graphics/texture.hpp"

namespace editor
{

Debug_view_window::Debug_view_window()
    : erhe::components::Component{c_name}
    , Framebuffer_window         {c_name, c_title}
{
}

Debug_view_window::~Debug_view_window() = default;

void Debug_view_window::connect()
{
    m_forward_renderer       = get    <Forward_renderer                    >();
    m_pipeline_state_tracker = get    <erhe::graphics::OpenGL_state_tracker>();
    m_programs               = require<Programs                            >();
    m_scene_root             = get    <Scene_root                          >();
    m_shadow_renderer        = get    <Shadow_renderer                     >();

    require<Editor_imgui_windows>();
    require<Gl_context_provider >();
}

void Debug_view_window::initialize_component()
{
    Framebuffer_window::initialize(
        *get<Editor_imgui_windows>().get(),
        get<Gl_context_provider>(),
        get<Programs>()->visualize_depth.get()
    );

    m_empty_vertex_input = std::make_unique<erhe::graphics::Vertex_input_state>();

    m_renderpass.pipeline.data = {
        .name           = "Debug_view",
        .shader_stages  = m_programs->visualize_depth.get(),
        .vertex_input   = m_empty_vertex_input.get(),
        .input_assembly = erhe::graphics::Input_assembly_state::triangle_fan,
        .rasterization  = erhe::graphics::Rasterization_state::cull_mode_none,
        .depth_stencil  = erhe::graphics::Depth_stencil_state::depth_test_disabled_stencil_test_disabled,
        .color_blend    = erhe::graphics::Color_blend_state::color_blend_disabled
    };

}

auto Debug_view_window::get_size() const -> glm::vec2
{
    if (!m_shadow_renderer)
    {
        return {};
    }

    return glm::vec2{
        static_cast<float>(m_shadow_renderer->texture()->width()),
        static_cast<float>(m_shadow_renderer->texture()->height()),
    };
}

void Debug_view_window::render()
{
    if (
        (m_viewport.width < 1) ||
        (m_viewport.height < 1)
    )
    {
        return;
    }

    erhe::graphics::Scoped_debug_group pass_scope{m_debug_label};

    gl::bind_framebuffer(gl::Framebuffer_target::draw_framebuffer, m_framebuffer->gl_name());
    m_forward_renderer->render_fullscreen(
        Forward_renderer::Render_parameters{
            .viewport    = m_viewport,
            .mesh_layers = {},
            .light_layer = m_scene_root->light_layer(),
            .materials   = m_scene_root->materials(),
            .passes      = { &m_renderpass }
        }
    );

    gl::bind_framebuffer(gl::Framebuffer_target::draw_framebuffer, 0);
}

} // namespace editor
