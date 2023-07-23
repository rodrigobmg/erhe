#include "example.hpp"

#include "example_log.hpp"
#include "frame_controller.hpp"
#include "gltf.hpp"
#include "image_transfer.hpp"
#include "mesh_memory.hpp"
#include "programs.hpp"

#include "erhe/gl/enum_bit_mask_operators.hpp"
#include "erhe/gl/gl_log.hpp"
#include "erhe/gl/wrapper_functions.hpp"
#include "erhe/graphics/buffer_transfer_queue.hpp"
#include "erhe/graphics/graphics_log.hpp"
#include "erhe/graphics/instance.hpp"
#include "erhe/graphics/pipeline.hpp"
#include "erhe/log/log.hpp"
#include "erhe/primitive/primitive_log.hpp"
#include "erhe/renderer/pipeline_renderpass.hpp"
#include "erhe/renderer/renderer_log.hpp"
#include "erhe/scene/mesh.hpp"
#include "erhe/scene/node.hpp"
#include "erhe/scene/scene.hpp"
#include "erhe/scene/scene_log.hpp"
#include "erhe/scene/scene_message_bus.hpp"
#include "erhe/scene_renderer/forward_renderer.hpp"
#include "erhe/scene_renderer/program_interface.hpp"
#include "erhe/scene_renderer/scene_renderer_log.hpp"
#include "erhe/toolkit/renderdoc_capture.hpp"
#include "erhe/toolkit/toolkit_log.hpp"
#include "erhe/toolkit/verify.hpp"
#include "erhe/toolkit/window.hpp"
#include "erhe/toolkit/window.hpp"
#include "erhe/toolkit/window_event_handler.hpp"
#include "erhe/ui/ui_log.hpp"

namespace example {

class Example
    : public erhe::toolkit::Window_event_handler
{
public:
    virtual auto get_name() const -> const char* { return "Example"; }

    Example(
        erhe::toolkit::Context_window&          window,
        erhe::scene::Scene&                     scene,
        erhe::graphics::Instance&               graphics_instance,
        erhe::scene_renderer::Forward_renderer& forward_renderer,
        Parse_context&                          parse_context,
        Mesh_memory&                            mesh_memory,
        Programs&                               programs
    )
        : m_window           {window}
        , m_scene            {scene}
        , m_graphics_instance{graphics_instance}
        , m_forward_renderer {forward_renderer}
        , m_parse_context    {parse_context}
        , m_mesh_memory      {mesh_memory}
        , m_programs         {programs}
    {
        m_camera = make_camera(
            "Camera",
            glm::vec3{0.0f, 0.0f, 10.0f},
            glm::vec3{0.0f, 0.0f, -1.0f}
        );
        m_light = make_point_light(
            "Light",
            glm::vec3{0.0f, 0.0f, 4.0f}, // position
            glm::vec3{1.0f, 1.0f, 1.0f}, // color
            40.0f
        );

        m_camera_controller = std::make_shared<Frame_controller>();
        m_camera_controller->set_node(m_camera->get_node());

        auto& root_event_handler = m_window.get_root_window_event_handler();
        root_event_handler.attach(this, 3);
    }

    void run()
    {
        m_current_time = std::chrono::steady_clock::now();
        while (!m_close_requested) {
            m_window.poll_events();
            update();
            m_window.swap_buffers();
        }
    }

    void update()
    {
        // Update fixed steps
        {
            const auto new_time   = std::chrono::steady_clock::now();
            const auto duration   = new_time - m_current_time;
            double     frame_time = std::chrono::duration<double, std::ratio<1>>(duration).count();

            if (frame_time > 0.25) {
                frame_time = 0.25;
            }

            m_current_time = new_time;
            m_time_accumulator += frame_time;
            const double dt = 1.0 / 100.0;
            while (m_time_accumulator >= dt) {
                update_fixed_step();
                m_time_accumulator -= dt;
                m_time += dt;
            }
        }

        m_camera_controller->update();
        m_scene.update_node_transforms();

        gl::enable(gl::Enable_cap::framebuffer_srgb);
        gl::clear_color(0.1f, 0.1f, 0.1f, 1.0f);
        gl::clear_stencil(0);
        gl::clear_depth_f(*m_graphics_instance.depth_clear_value_pointer());
        gl::clear(
            gl::Clear_buffer_mask::color_buffer_bit |
            gl::Clear_buffer_mask::depth_buffer_bit |
            gl::Clear_buffer_mask::stencil_buffer_bit
        );

        std::vector<erhe::renderer::Pipeline_renderpass*> passes;


        const bool reverse_depth = m_graphics_instance.configuration.reverse_depth;
        erhe::renderer::Pipeline_renderpass standard_pipeline_renderpass{ 
            erhe::graphics::Pipeline{
                erhe::graphics::Pipeline_data{
                    .name           = "Standard Renderpass",
                    .shader_stages  = &m_programs.standard,
                    .vertex_input   = &m_mesh_memory.vertex_input,
                    .input_assembly = erhe::graphics::Input_assembly_state::triangles,
                    .rasterization  = erhe::graphics::Rasterization_state::cull_mode_back_cw(reverse_depth),
                    .depth_stencil  = erhe::graphics::Depth_stencil_state::depth_test_enabled_stencil_test_disabled(reverse_depth),
                    .color_blend    = erhe::graphics::Color_blend_state::color_blend_disabled
                }
            }
        };

        passes.push_back(&standard_pipeline_renderpass);

        erhe::toolkit::Viewport viewport{
            .x             = 0,
            .y             = 0,
            .width         = m_window.get_width(),
            .height        = m_window.get_height(),
            .reverse_depth = m_graphics_instance.configuration.reverse_depth
        };

        std::vector<std::shared_ptr<erhe::scene::Light>> lights;
        for (const auto& light : m_parse_context.lights) {
            lights.push_back(light);
        }
        lights.push_back(m_light);

        erhe::scene_renderer::Light_projections light_projections{
            lights,
            m_camera.get(),
            erhe::toolkit::Viewport{},
            std::shared_ptr<erhe::graphics::Texture>{},
            0
        };

        m_forward_renderer.render(
            erhe::scene_renderer::Forward_renderer::Render_parameters{
                .index_type             = gl::Draw_elements_type::unsigned_int,
                .ambient_light          = glm::vec3{0.1f, 0.1f, 0.1f},
                .camera                 = m_camera.get(),
                .light_projections      = &light_projections,
                .lights                 = lights,
                .skins                  = m_parse_context.skins,
                .materials              = m_parse_context.materials,
                .mesh_spans             = { m_parse_context.meshes },
                .passes                 = passes,
                .primitive_mode         = erhe::primitive::Primitive_mode::polygon_fill,
                .primitive_settings     = erhe::scene_renderer::Primitive_interface_settings{},
                .shadow_texture         = nullptr,
                .viewport               = viewport,
                .filter                 = erhe::Item_filter{},
                .override_shader_stages = nullptr
            }
        );

        m_forward_renderer.next_frame();
    }
    void update_fixed_step()
    {
        m_camera_controller->update_fixed_step();
    }

    // Implements erhe::toolkit::Window_event_handler
    auto on_close() -> bool override
    {
        m_close_requested = true;
        return true;
    }

    auto on_key(erhe::toolkit::Keycode code, uint32_t modifier_mask, bool pressed) -> bool override
    {
        static_cast<void>(modifier_mask);
        switch (code) {
            case erhe::toolkit::Key_w: m_camera_controller->translate_z.set_less(pressed); return true;
            case erhe::toolkit::Key_s: m_camera_controller->translate_z.set_more(pressed); return true;
            case erhe::toolkit::Key_a: m_camera_controller->translate_x.set_less(pressed); return true;
            case erhe::toolkit::Key_d: m_camera_controller->translate_x.set_more(pressed); return true;
            default: return false;
        }
    }

    auto on_mouse_move(float x, float y) -> bool override
    {
        if (!m_mouse_pressed) {
            m_mouse_last_x = x;
            m_mouse_last_y = y;
            return false;
        }
        float dx = x - m_mouse_last_x;
        float dy = y - m_mouse_last_y;
        m_mouse_last_x = x;
        m_mouse_last_y = y;
        if (dx != 0.0f) {
            m_camera_controller->rotate_y.adjust(dx * (-1.0f / 1024.0f));
        }

        if (dy != 0.0f) {
            m_camera_controller->rotate_x.adjust(dy * (-1.0f / 1024.0f));
        }
        return true;
    }

    auto on_mouse_button(erhe::toolkit::Mouse_button button, bool pressed) -> bool override
    {
        if (button != erhe::toolkit::Mouse_button_left) {
            return false;
        }
        m_mouse_pressed = pressed;
        return true;
    }

    auto on_mouse_wheel(float, float y) -> bool override
    {
        glm::vec3 position = m_camera_controller->get_position();
        const float l = glm::length(position);
        const float k = (-1.0f / 16.0f) * l * l * y;
        m_camera_controller->get_controller(Control::translate_z).adjust(k);
        return true;
    }

private:
    auto make_camera(
        const std::string_view name,
        const glm::vec3        position,
        const glm::vec3        look_at
    ) -> std::shared_ptr<erhe::scene::Camera>
    {
        using Item_flags = erhe::Item_flags;

        auto node   = std::make_shared<erhe::scene::Node>(fmt::format("{} node", name));
        auto camera = std::make_shared<erhe::scene::Camera>(name);
        camera->projection()->fov_y           = glm::radians(45.0f);
        camera->projection()->projection_type = erhe::scene::Projection::Type::perspective_vertical;
        camera->projection()->z_near          = 1.0f / 128.0f;
        camera->projection()->z_far           = 512.0f;
        camera->enable_flag_bits(Item_flags::content | Item_flags::show_in_ui);
        node->attach(camera);
        node->set_parent(m_scene.get_root_node());

        const glm::mat4 m = erhe::toolkit::create_look_at(
            position, // eye
            look_at,  // center
            glm::vec3{0.0f, 1.0f, 0.0f}  // up
        );
        node->set_parent_from_node(m);
        node->enable_flag_bits(Item_flags::content | Item_flags::show_in_ui);

        return camera;
    }

    auto make_directional_light(
        const std::string_view name,
        const glm::vec3        position,
        const glm::vec3        color,
        const float            intensity
    ) -> std::shared_ptr<erhe::scene::Light>
    {
        using Item_flags = erhe::Item_flags;

        auto node  = std::make_shared<erhe::scene::Node>(fmt::format("{} node", name));
        auto light = std::make_shared<erhe::scene::Light>(name);
        light->type      = erhe::scene::Light::Type::directional;
        light->color     = color;
        light->intensity = intensity;
        light->range     = 0.0f;
        light->layer_id  = 0;
        light->enable_flag_bits(Item_flags::content | Item_flags::visible | Item_flags::show_in_ui);
        node->attach          (light);
        node->set_parent      (m_scene.get_root_node());
        node->enable_flag_bits(Item_flags::content | Item_flags::visible | Item_flags::show_in_ui);

        const glm::mat4 m = erhe::toolkit::create_look_at(
            position,                     // eye
            glm::vec3{0.0f, 0.0f, 0.0f},  // center
            glm::vec3{0.0f, 1.0f, 0.0f}   // up
        );
        node->set_parent_from_node(m);

        return light;
    }

    auto make_point_light(
        const std::string_view name,
        const glm::vec3        position,
        const glm::vec3        color,
        const float            intensity
    ) -> std::shared_ptr<erhe::scene::Light>
    {
        using Item_flags = erhe::Item_flags;

        auto node  = std::make_shared<erhe::scene::Node>(fmt::format("{} node", name));
        auto light = std::make_shared<erhe::scene::Light>(name);
        light->type      = erhe::scene::Light::Type::point;
        light->color     = color;
        light->intensity = intensity;
        light->range     = 25.0f;
        light->layer_id  = 0;
        light->enable_flag_bits(Item_flags::content | Item_flags::visible | Item_flags::show_in_ui);
        node->attach          (light);
        node->set_parent      (m_scene.get_root_node());
        node->enable_flag_bits(Item_flags::content | Item_flags::visible | Item_flags::show_in_ui);

        const glm::mat4 m = erhe::toolkit::create_translation<float>(position);
        node->set_parent_from_node(m);

        return light;
    }

    erhe::toolkit::Context_window&          m_window;
    erhe::scene::Scene&                     m_scene;
    erhe::graphics::Instance&               m_graphics_instance;
    erhe::scene_renderer::Forward_renderer& m_forward_renderer;
    Parse_context&                          m_parse_context;
    Mesh_memory&                            m_mesh_memory;
    Programs&                               m_programs;

    bool                                    m_close_requested{false};
    std::shared_ptr<erhe::scene::Camera>    m_camera;
    std::shared_ptr<erhe::scene::Light>     m_light;
    std::shared_ptr<Frame_controller>       m_camera_controller;
    std::chrono::steady_clock::time_point   m_current_time;
    double                                  m_time_accumulator{0.0};
    double                                  m_time            {0.0};
    uint64_t                                m_frame_number{0};

    bool  m_mouse_pressed{false};
    float m_mouse_last_x{0.0f};
    float m_mouse_last_y{0.0f};
};

void run_example()
{
    erhe::log::initialize_log_sinks();
    gl::initialize_logging();
    erhe::graphics::initialize_logging();
    erhe::primitive::initialize_logging();
    erhe::renderer::initialize_logging();
    erhe::scene::initialize_logging();
    erhe::scene_renderer::initialize_logging();
    erhe::toolkit::initialize_logging();
    erhe::ui::initialize_logging();
    example::initialize_logging();
    erhe::toolkit::initialize_frame_capture();

    erhe::toolkit::Context_window window{
        erhe::toolkit::Window_configuration{
            .gl_major          = 4,
            .gl_minor          = 6,
            .width             = 1920,
            .height            = 1080,
            .msaa_sample_count = 0,
            .title             = "erhe example"
        }
    };

    erhe::graphics::Instance                graphics_instance{window};
    erhe::scene_renderer::Program_interface program_interface{graphics_instance};
    erhe::scene_renderer::Forward_renderer  forward_renderer {graphics_instance, program_interface};
    erhe::scene::Scene_message_bus    scene_message_bus;
    erhe::scene::Scene                scene         {scene_message_bus, "example scene", nullptr};
    Mesh_memory                       mesh_memory   {graphics_instance, program_interface};
    Programs                          programs      {graphics_instance, program_interface};
    Image_transfer                    image_transfer{graphics_instance};
    Parse_context parse_context{
        .graphics_instance = graphics_instance,
        .buffer_sink       = mesh_memory.gl_buffer_sink,
        .vertex_format     = mesh_memory.vertex_format,
        .scene             = scene,
        .image_transfer    = image_transfer,
        .path              = "res/models/BoxTextured.gltf"
    };
    parse_gltf(parse_context);
    mesh_memory.gl_buffer_transfer_queue.flush();
    gl::clip_control(gl::Clip_control_origin::lower_left, gl::Clip_control_depth::zero_to_one);
    gl::enable      (gl::Enable_cap::framebuffer_srgb);

    Example example{window, scene, graphics_instance, forward_renderer, parse_context, mesh_memory, programs};
    example.run();
}

} // namespace example

