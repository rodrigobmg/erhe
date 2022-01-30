#include "renderers/forward_renderer.hpp"
#include "configuration.hpp"
#include "graphics/gl_context_provider.hpp"

#include "renderers/mesh_memory.hpp"
#include "renderers/program_interface.hpp"
#include "renderers/shadow_renderer.hpp"

#include "erhe/graphics/buffer.hpp"
#include "erhe/graphics/configuration.hpp"
#include "erhe/graphics/debug.hpp"
#include "erhe/graphics/opengl_state_tracker.hpp"
#include "erhe/graphics/shader_stages.hpp"
#include "erhe/graphics/shader_resource.hpp"
#include "erhe/graphics/state/vertex_input_state.hpp"
#include "erhe/graphics/vertex_format.hpp"
#include "erhe/primitive/primitive.hpp"
#include "erhe/scene/camera.hpp"
#include "erhe/scene/light.hpp"
#include "erhe/scene/scene.hpp"
#include "erhe/gl/gl.hpp"
#include "erhe/gl/strong_gl_enums.hpp"
#include "erhe/toolkit/math_util.hpp"
#include "erhe/toolkit/profile.hpp"
#include "erhe/toolkit/verify.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <functional>

namespace editor
{

using erhe::graphics::Vertex_input_state;
using erhe::graphics::Input_assembly_state;
using erhe::graphics::Rasterization_state;
using erhe::graphics::Depth_stencil_state;
using erhe::graphics::Color_blend_state;

Forward_renderer::Forward_renderer()
    : Component    {c_name}
    , Base_renderer{std::string{c_name}}
{
}

Forward_renderer::~Forward_renderer()
{
}

void Forward_renderer::connect()
{
    base_connect(this);

    require<Gl_context_provider>();
    require<Program_interface  >();

    m_configuration          = require<Configuration>();
    m_mesh_memory            = require<Mesh_memory>();
    m_programs               = require<Programs   >();
    m_pipeline_state_tracker = get<erhe::graphics::OpenGL_state_tracker>();
    m_shadow_renderer        = get<Shadow_renderer>();
}

static constexpr std::string_view c_forward_renderer_initialize_component{"Forward_renderer::initialize_component()"};
void Forward_renderer::initialize_component()
{
    ERHE_PROFILE_FUNCTION

    const Scoped_gl_context gl_context{Component::get<Gl_context_provider>()};

    erhe::graphics::Scoped_debug_group forward_renderer_initialization{c_forward_renderer_initialize_component};

    create_frame_resources(256, 256, 256, 8000, 8000);
}


static constexpr std::string_view c_forward_renderer_render{"Forward_renderer::render()"};

void Forward_renderer::render(const Render_parameters& parameters)
{
    ERHE_PROFILE_FUNCTION

    const auto&    viewport              = parameters.viewport;
    const auto*    camera                = parameters.camera;
    const auto&    mesh_layers           = parameters.mesh_layers;
    const auto*    light_layer           = parameters.light_layer;
    const auto&    materials             = parameters.materials;
    const auto&    passes                = parameters.passes;
    const auto&    visibility_filter     = parameters.visibility_filter;
    const bool     enable_shadows        = m_shadow_renderer && (light_layer != nullptr);
    const uint64_t shadow_texture_handle = enable_shadows
        ?
            erhe::graphics::get_handle(
                *m_shadow_renderer->texture(),
                *m_programs->nearest_sampler.get()
            )
        : 0;

    erhe::graphics::Scoped_debug_group forward_renderer_render{c_forward_renderer_render};

    gl::viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    if (camera != nullptr)
    {
        update_camera_buffer(*camera, viewport);
        bind_camera_buffer  ();
    }
    update_material_buffer(materials);
    bind_material_buffer();
    if (light_layer != nullptr)
    {
        update_light_buffer(
            *light_layer,
            m_shadow_renderer->viewport(),
            shadow_texture_handle
        );
        bind_light_buffer();
    }
    if (enable_shadows)
    {
        gl::make_texture_handle_resident_arb(shadow_texture_handle);
    }
    for (auto& pass : passes)
    {
        const auto& pipeline = pass->pipeline;
        if (!pipeline.data.shader_stages)
        {
            return;
        }

        const auto primitive_mode = pass->primitive_mode; //select_primitive_mode(pass);

        if (pass->begin)
        {
            pass->begin();
        }

        erhe::graphics::Scoped_debug_group pass_scope{pass->pipeline.data.name};

        m_pipeline_state_tracker->execute(pipeline);
        for (auto mesh_layer : mesh_layers)
        {
            ERHE_PROFILE_GPU_SCOPE(c_forward_renderer_render);

            update_primitive_buffer(*mesh_layer, visibility_filter);
            const auto draw_indirect_buffer_range = update_draw_indirect_buffer(*mesh_layer, primitive_mode, visibility_filter);
            if (draw_indirect_buffer_range.draw_indirect_count == 0)
            {
                continue;
            }
            bind_primitive_buffer();
            bind_draw_indirect_buffer();

            gl::multi_draw_elements_indirect(
                pipeline.data.input_assembly.primitive_topology,
                m_mesh_memory->gl_index_type(),
                reinterpret_cast<const void *>(draw_indirect_buffer_range.range.first_byte_offset),
                static_cast<GLsizei>(draw_indirect_buffer_range.draw_indirect_count),
                static_cast<GLsizei>(sizeof(gl::Draw_elements_indirect_command))
            );
        }

        if (pass->end)
        {
            pass->end();
        }
    }
    if (enable_shadows)
    {
        gl::make_texture_handle_non_resident_arb(shadow_texture_handle);
    }
}

void Forward_renderer::render_fullscreen(
    const Render_parameters& parameters
)
{
    ERHE_PROFILE_FUNCTION

    const auto&    viewport              = parameters.viewport;
    const auto*    camera                = parameters.camera;
    const auto*    light_layer           = parameters.light_layer;
    const auto&    passes                = parameters.passes;
    const bool     enable_shadows        = m_shadow_renderer && (light_layer != nullptr);
    const uint64_t shadow_texture_handle = enable_shadows
        ?
            erhe::graphics::get_handle(
                *m_shadow_renderer->texture(),
                *m_programs->nearest_sampler.get()
            )
        : 0;

    erhe::graphics::Scoped_debug_group forward_renderer_render{c_forward_renderer_render};

    gl::viewport(viewport.x, viewport.y, viewport.width, viewport.height);
    if (camera != nullptr)
    {
        update_camera_buffer(*camera, viewport);
        bind_camera_buffer  ();
    }
    if (light_layer != nullptr)
    {
        update_light_buffer(
            *light_layer,
            m_shadow_renderer->viewport(),
            shadow_texture_handle
        );
        bind_light_buffer();
    }
    if (enable_shadows)
    {
        gl::make_texture_handle_resident_arb(shadow_texture_handle);
    }
    for (auto& pass : passes)
    {
        const auto& pipeline = pass->pipeline;
        if (!pipeline.data.shader_stages)
        {
            return;
        }

        if (pass->begin)
        {
            pass->begin();
        }

        erhe::graphics::Scoped_debug_group pass_scope{pass->pipeline.data.name};

        m_pipeline_state_tracker->execute(pipeline);
        gl::draw_arrays(pipeline.data.input_assembly.primitive_topology, 0, 4);

        if (pass->end)
        {
            pass->end();
        }
    }
    if (enable_shadows)
    {
        gl::make_texture_handle_non_resident_arb(shadow_texture_handle);
    }
}

} // namespace editor
