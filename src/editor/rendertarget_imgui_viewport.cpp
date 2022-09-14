﻿// #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "rendertarget_imgui_viewport.hpp"
#include "editor_log.hpp"
#include "rendertarget_node.hpp"
#include "scene/viewport_window.hpp"

#if defined(ERHE_XR_LIBRARY_OPENXR)
#   include "xr/hand_tracker.hpp"
#   include "xr/headset_renderer.hpp"
#endif

#include "erhe/application/configuration.hpp"
#include "erhe/application/imgui/imgui_renderer.hpp"
#include "erhe/application/imgui/imgui_window.hpp"
#include "erhe/application/imgui/imgui_windows.hpp"
#include "erhe/application/imgui/scoped_imgui_context.hpp"
#include "erhe/application/view.hpp"
#include "erhe/graphics/framebuffer.hpp"
#include "erhe/graphics/texture.hpp"

#include <GLFW/glfw3.h> // TODO Fix dependency ?

#include <imgui.h>
#include <imgui_internal.h>

namespace editor
{

Rendertarget_imgui_viewport::Rendertarget_imgui_viewport(
    Rendertarget_node*                  rendertarget_node,
    const std::string_view              name,
    const erhe::components::Components& components,
    const bool                          imgui_ini
)
    : erhe::application::Imgui_viewport{
        name,
        components.get<erhe::application::Imgui_renderer>()->get_font_atlas()
    }
    , m_rendertarget_node{rendertarget_node}
    , m_configuration    {components.get<erhe::application::Configuration >()}
    , m_imgui_renderer   {components.get<erhe::application::Imgui_renderer>()}
    , m_imgui_windows    {components.get<erhe::application::Imgui_windows >()}
    , m_view             {components.get<erhe::application::View          >()}
#if defined(ERHE_XR_LIBRARY_OPENXR)
    , m_hand_tracker     {components.get<Hand_tracker    >()}
    , m_headset_renderer {components.get<Headset_renderer>()}
#endif
    , m_name             {name}
    , m_imgui_ini_path   {imgui_ini ? fmt::format("imgui_{}.ini", name) : ""}
{
    m_imgui_renderer->use_as_backend_renderer_on_context(m_imgui_context);

    ImGuiIO& io = m_imgui_context->IO;
    io.MouseDrawCursor = true;
    io.IniFilename = imgui_ini ? m_imgui_ini_path.c_str() : nullptr;
    io.FontDefault = m_imgui_renderer->vr_primary_font();

    IM_ASSERT(io.BackendPlatformUserData == NULL && "Already initialized a platform backend!");

    io.BackendPlatformUserData = this;
    io.BackendPlatformName     = "erhe rendertarget";
    io.DisplaySize             = ImVec2{static_cast<float>(m_rendertarget_node->width()), static_cast<float>(m_rendertarget_node->height())};
    io.DisplayFramebufferScale = ImVec2{1.0f, 1.0f};

    io.MousePos                = ImVec2{-FLT_MAX, -FLT_MAX};
    io.MouseHoveredViewport    = 0;

    m_last_mouse_x = -FLT_MAX;
    m_last_mouse_y = -FLT_MAX;

    ImGui::SetCurrentContext(nullptr);
}

Rendertarget_imgui_viewport::~Rendertarget_imgui_viewport() noexcept
{
    ImGui::DestroyContext(m_imgui_context);
}

template <typename T>
[[nodiscard]] inline auto as_span(const T& value) -> gsl::span<const T>
{
    return gsl::span<const T>(&value, 1);
}

[[nodiscard]] auto Rendertarget_imgui_viewport::rendertarget_node() -> Rendertarget_node*
{
    return m_rendertarget_node;
}

[[nodiscard]] auto Rendertarget_imgui_viewport::begin_imgui_frame() -> bool
{
    SPDLOG_LOGGER_TRACE(
        log_rendertarget_imgui_windows,
        "Rendertarget_imgui_viewport::begin_imgui_frame()"
    );

    if (m_rendertarget_node == nullptr)
    {
        return false;
    }

#if defined(ERHE_XR_LIBRARY_OPENXR)
    m_rendertarget_node->update_headset(*m_headset_renderer.get());
#endif

    const auto pointer = m_rendertarget_node->get_pointer();

#if defined(ERHE_XR_LIBRARY_OPENXR)
    if (!m_configuration->headset.openxr) // TODO Figure out better way to combine different input methods
#endif
    {
        if (pointer.has_value())
        {
            if (!has_cursor())
            {
                on_cursor_enter(1);
            }
            const auto position = pointer.value();
            if (
                (m_last_mouse_x != position.x) ||
                (m_last_mouse_y != position.y)
            )
            {
                m_last_mouse_x = position.x;
                m_last_mouse_y = position.y;
                on_mouse_move(position.x, position.y);
            }
        }
        else
        {
            if (has_cursor())
            {
                on_cursor_enter(0);
                m_last_mouse_x = -FLT_MAX;
                m_last_mouse_y = -FLT_MAX;
                on_mouse_move(-FLT_MAX, -FLT_MAX);
            }
        }
    }

#if 0
    if (m_hand_tracker)
    {
        m_rendertarget_node->update_headset(*m_headset_renderer.get());
        const auto& pointer_finger_opt = m_rendertarget_node->get_pointer_finger();
        if (pointer_finger_opt.has_value())
        {
            const auto& pointer_finger           = pointer_finger_opt.value();
            m_headset_renderer->add_finger_input(pointer_finger);
            const auto  finger_world_position    = pointer_finger.finger_point;
            const auto  viewport_world_position  = pointer_finger.point;
            //const float distance                 = glm::distance(finger_world_position, viewport_world_position);
            const auto  window_position_opt      = m_rendertarget_node->world_to_window(viewport_world_position);

            if (window_position_opt.has_value())
            {
                if (!has_cursor())
                {
                    on_cursor_enter(1);
                }
                const auto position = window_position_opt.value();
                if (
                    (m_last_mouse_x != position.x) ||
                    (m_last_mouse_y != position.y)
                )
                {
                    m_last_mouse_x = position.x;
                    m_last_mouse_y = position.y;
                    on_mouse_move(position.x, position.y);
                }
            }
            else
            {
                if (has_cursor())
                {
                    on_cursor_enter(0);
                    m_last_mouse_x = -FLT_MAX;
                    m_last_mouse_y = -FLT_MAX;
                    on_mouse_move(-FLT_MAX, -FLT_MAX);
                }
            }

            ////const float finger_press_threshold = m_headset_renderer->finger_to_viewport_distance_threshold();
            ////if ((distance < finger_press_threshold * 0.98f) && (!m_last_mouse_finger))
            ////{
            ////    m_last_mouse_finger = true;
            ////    ImGuiIO& io = m_imgui_context->IO;
            ////    io.AddMouseButtonEvent(0, true);
            ////}
            ////if ((distance > finger_press_threshold * 1.02f) && (m_last_mouse_finger))
            ////{
            ////    m_last_mouse_finger = false;
            ////    ImGuiIO& io = m_imgui_context->IO;
            ////    io.AddMouseButtonEvent(0, false);
            ////}
        }
        const bool finger_trigger = m_rendertarget_node->get_finger_trigger();

        if (finger_trigger && (!m_last_mouse_finger))
        {
            m_last_mouse_finger = true;
            ImGuiIO& io = m_imgui_context->IO;
            io.AddMouseButtonEvent(0, true);
        }
        if (!finger_trigger && (m_last_mouse_finger))
        {
            m_last_mouse_finger = false;
            ImGuiIO& io = m_imgui_context->IO;
            io.AddMouseButtonEvent(0, false);
        }
    }
#endif

#if defined(ERHE_XR_LIBRARY_OPENXR)
    if (m_configuration->headset.openxr) // TODO Figure out better way to combine different input methods
    {
        const auto& pose                   = m_rendertarget_node->get_controller_pose();
        const float trigger_value          = m_rendertarget_node->get_controller_trigger();
        const auto  controller_orientation = glm::mat4_cast(pose.orientation);
        const auto  controller_direction   = glm::vec3{controller_orientation * glm::vec4{0.0f, 0.0f, -1.0f, 0.0f}};
        const bool  trigger                = trigger_value > 0.5f;

        const auto intersection = erhe::toolkit::intersect_plane<float>(
            glm::vec3{m_rendertarget_node->direction_in_world()},
            glm::vec3{m_rendertarget_node->position_in_world()},
            pose.position,
            controller_direction
        );
        bool mouse_has_position{false};
        if (intersection.has_value())
        {
            const auto world_position      = pose.position + intersection.value() * controller_direction;
            const auto window_position_opt = m_rendertarget_node->world_to_window(world_position);
            if (window_position_opt.has_value())
            {
                if (!has_cursor())
                {
                    on_cursor_enter(1);
                }
                mouse_has_position = true;
                const auto position = window_position_opt.value();
                if (
                    (m_last_mouse_x != position.x) ||
                    (m_last_mouse_y != position.y)
                )
                {
                    m_last_mouse_x = position.x;
                    m_last_mouse_y = position.y;
                    on_mouse_move(position.x, position.y);
                }
            }
        }
        if (!mouse_has_position)
        {
            if (has_cursor())
            {
                on_cursor_enter(0);
                m_last_mouse_x = -FLT_MAX;
                m_last_mouse_y = -FLT_MAX;
                on_mouse_move(-FLT_MAX, -FLT_MAX);
            }
        }

        m_headset_renderer->add_controller_input(
            Controller_input{
                .position      = pose.position,
                .direction     = controller_direction,
                .trigger_value = trigger_value
            }
        );
        if (trigger && (!m_last_mouse_finger))
        {
            m_last_mouse_finger = true;
            ImGuiIO& io = m_imgui_context->IO;
            io.AddMouseButtonEvent(0, true);
        }
        if (!trigger && (m_last_mouse_finger))
        {
            m_last_mouse_finger = false;
            ImGuiIO& io = m_imgui_context->IO;
            io.AddMouseButtonEvent(0, false);
        }
    }
#endif

    const auto current_time = glfwGetTime();
    ImGuiIO& io = m_imgui_context->IO;
    io.DeltaTime = m_time > 0.0
        ? static_cast<float>(current_time - m_time)
        : static_cast<float>(1.0 / 60.0);
    m_time = current_time;

    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

    return true;
}

void Rendertarget_imgui_viewport::end_imgui_frame()
{
    SPDLOG_LOGGER_TRACE(
        log_rendertarget_imgui_windows,
        "Rendertarget_imgui_viewport::end_imgui_frame()"
    );

    ImGui::EndFrame();
    ImGui::Render();
}

void Rendertarget_imgui_viewport::execute_rendergraph_node()
{
    SPDLOG_LOGGER_TRACE(log_rendertarget_imgui_windows, "Rendertarget_imgui_viewport::execute_rendergraph_node()");

    erhe::application::Imgui_windows&  imgui_windows  = *m_imgui_windows.get();
    erhe::application::Imgui_viewport& imgui_viewport = *this;

    erhe::application::Scoped_imgui_context imgui_context(imgui_windows, imgui_viewport);

    m_rendertarget_node->bind();
    m_rendertarget_node->clear(glm::vec4{0.0f, 0.0f, 0.0f, 0.5f});
    m_imgui_renderer->render_draw_data();
    m_rendertarget_node->render_done();
}

auto Rendertarget_imgui_viewport::get_consumer_input_texture(
    const erhe::application::Resource_routing resource_routing,
    const int                                 key,
    const int                                 depth
) const -> std::shared_ptr<erhe::graphics::Texture>
{
    static_cast<void>(resource_routing); // TODO Validate
    static_cast<void>(key); // TODO Validate
    static_cast<void>(depth);
    return m_rendertarget_node->texture();
}

auto Rendertarget_imgui_viewport::get_consumer_input_framebuffer(
    const erhe::application::Resource_routing resource_routing,
    const int                                 key,
    const int                                 depth
) const -> std::shared_ptr<erhe::graphics::Framebuffer>
{
    static_cast<void>(resource_routing); // TODO Validate
    static_cast<void>(key); // TODO Validate
    static_cast<void>(depth);
    return m_rendertarget_node->framebuffer();
}

auto Rendertarget_imgui_viewport::get_consumer_input_viewport(
    const erhe::application::Resource_routing resource_routing,
    const int                                 key,
    const int                                 depth
) const -> erhe::scene::Viewport
{
    static_cast<void>(resource_routing); // TODO Validate
    static_cast<void>(key); // TODO Validate
    static_cast<void>(depth);
    return erhe::scene::Viewport{
        .x      = 0,
        .y      = 0,
        .width  = static_cast<int>(m_rendertarget_node->width()),
        .height = static_cast<int>(m_rendertarget_node->height())
        // .reverse_depth = false // unused
    };
}

auto Rendertarget_imgui_viewport::get_producer_output_viewport(
    const erhe::application::Resource_routing resource_routing,
    const int                                 key,
    const int                                 depth
) const -> erhe::scene::Viewport
{
    static_cast<void>(resource_routing); // TODO Validate
    static_cast<void>(key); // TODO Validate
    static_cast<void>(depth);
    return erhe::scene::Viewport{
        .x      = 0,
        .y      = 0,
        .width  = static_cast<int>(m_rendertarget_node->width()),
        .height = static_cast<int>(m_rendertarget_node->height())
        // .reverse_depth = false // unused
    };
}

}  // namespace editor
