#include "scene/scene_commands.hpp"

#include "editor_context.hpp"
#include "operations/compound_operation.hpp"
#include "operations/insert_operation.hpp"
#include "operations/node_operation.hpp"
#include "operations/operation_stack.hpp"
#include "rendertarget_mesh.hpp"
#include "rendertarget_imgui_viewport.hpp"
#include "scene/node_raytrace.hpp"
#include "scene/scene_root.hpp"
#include "scene/viewport_window.hpp"
#include "scene/viewport_windows.hpp"
#include "tools/selection_tool.hpp"

#include "erhe/commands/commands.hpp"
#include "erhe/imgui/imgui_windows.hpp"
#include "erhe/scene/camera.hpp"
#include "erhe/scene/light.hpp"

namespace editor
{

using erhe::scene::Item_flags;

#pragma region Command

Create_new_camera_command::Create_new_camera_command(
    erhe::commands::Commands& commands,
    Editor_context&           context
)
    : Command  {commands, "scene.create_new_camera"}
    , m_context{context}
{
}

auto Create_new_camera_command::try_call() -> bool
{
    return m_context.scene_commands->create_new_camera().operator bool();
}

Create_new_empty_node_command::Create_new_empty_node_command(
    erhe::commands::Commands& commands,
    Editor_context&           context
)
    : Command  {commands, "scene.create_new_empty_node"}
    , m_context{context}
{
}

auto Create_new_empty_node_command::try_call() -> bool
{
    return m_context.scene_commands->create_new_empty_node().operator bool();
}

Create_new_light_command::Create_new_light_command(
    erhe::commands::Commands& commands,
    Editor_context&           context
)
    : Command  {commands, "scene.create_new_light"}
    , m_context{context}
{
}

auto Create_new_light_command::try_call() -> bool
{
    return m_context.scene_commands->create_new_light().operator bool();
}
#pragma endregion Command

Scene_commands::Scene_commands(
    erhe::commands::Commands& commands,
    Editor_context&           editor_context
)
    : m_context                      {editor_context}
    , m_create_new_camera_command    {commands, editor_context}
    , m_create_new_empty_node_command{commands, editor_context}
    , m_create_new_light_command     {commands, editor_context}

{
    commands.register_command   (&m_create_new_camera_command);
    commands.register_command   (&m_create_new_empty_node_command);
    commands.register_command   (&m_create_new_light_command);
    commands.bind_command_to_key(&m_create_new_camera_command,     erhe::toolkit::Key_f2, true);
    commands.bind_command_to_key(&m_create_new_empty_node_command, erhe::toolkit::Key_f3, true);
    commands.bind_command_to_key(&m_create_new_light_command,      erhe::toolkit::Key_f4, true);
}

auto Scene_commands::get_scene_root(
    erhe::scene::Node* parent
) const -> Scene_root*
{
    if (parent != nullptr) {
        return static_cast<Scene_root*>(parent->get_item_host());
    }

    const auto  first_selected_node  = m_context.selection->get<erhe::scene::Node>();
    const auto  first_selected_scene = m_context.selection->get<erhe::scene::Scene>();
    const auto& viewport_window      = m_context.viewport_windows->last_window();

    erhe::scene::Item_host* scene_host = first_selected_node
        ? first_selected_node->get_item_host()
        : first_selected_scene
            ? first_selected_scene->get_root_node()->get_item_host()
            : viewport_window
                ? viewport_window->get_scene_root().get()
                : nullptr;
    if (scene_host == nullptr) {
        return nullptr;
    }

    Scene_root* scene_root = static_cast<Scene_root*>(scene_host);
    return scene_root;
}

auto Scene_commands::create_new_camera(
    erhe::scene::Node* parent
) -> std::shared_ptr<erhe::scene::Camera>
{
    Scene_root* scene_root = get_scene_root(parent);
    if (scene_root == nullptr) {
        return {};
    }

    auto new_node   = std::make_shared<erhe::scene::Node>("new camera node");
    auto new_camera = std::make_shared<erhe::scene::Camera>("new camera");
    new_node  ->enable_flag_bits(Item_flags::content | Item_flags::show_in_ui);
    new_camera->enable_flag_bits(erhe::scene::Item_flags::content | Item_flags::show_in_ui);
    m_context.operation_stack->push(
        std::make_shared<Compound_operation>(
            Compound_operation::Parameters{
                .operations = {
                    std::make_shared<Item_insert_remove_operation>(
                        Item_insert_remove_operation::Parameters{
                            .context = m_context,
                            .item    = new_node,
                            .parent  = (parent != nullptr)
                                ? std::static_pointer_cast<erhe::scene::Node>(parent->shared_from_this())
                                : scene_root->get_hosted_scene()->get_root_node(),
                            .mode    = Scene_item_operation::Mode::insert
                        }
                    ),
                    std::make_shared<Attach_operation>(new_camera, new_node)
                }
            }
        )
    );

    return new_camera;
}

auto Scene_commands::create_new_empty_node(
    erhe::scene::Node* parent
) -> std::shared_ptr<erhe::scene::Node>
{
    Scene_root* scene_root = get_scene_root(parent);
    if (scene_root == nullptr) {
        return {};
    }

    auto new_empty_node = std::make_shared<erhe::scene::Node>("new empty node");
    new_empty_node->enable_flag_bits(Item_flags::content | Item_flags::show_in_ui);
    m_context.operation_stack->push(
        std::make_shared<Item_insert_remove_operation>(
            Item_insert_remove_operation::Parameters{
                .context = m_context,
                .item    = new_empty_node,
                .parent  = (parent != nullptr)
                    ? std::static_pointer_cast<erhe::scene::Node>(parent->shared_from_this())
                    : scene_root->get_hosted_scene()->get_root_node(),
                .mode    = Scene_item_operation::Mode::insert
            }
        )
    );

    return new_empty_node;
}

auto Scene_commands::create_new_light(
    erhe::scene::Node* parent
) -> std::shared_ptr<erhe::scene::Light>
{
    Scene_root* scene_root = get_scene_root(parent);
    if (scene_root == nullptr) {
        return {};
    }

    auto new_node  = std::make_shared<erhe::scene::Node>("new light node");
    auto new_light = std::make_shared<erhe::scene::Light>("new light");
    new_node ->enable_flag_bits(erhe::scene::Item_flags::content | Item_flags::show_in_ui);
    new_light->enable_flag_bits(erhe::scene::Item_flags::content | Item_flags::show_in_ui);
    new_light->layer_id = scene_root->layers().light()->id;
    m_context.operation_stack->push(
        std::make_shared<Compound_operation>(
            Compound_operation::Parameters{
                .operations = {
                    std::make_shared<Item_insert_remove_operation>(
                        Item_insert_remove_operation::Parameters{
                            .context = m_context,
                            .item    = new_node,
                            .parent  = (parent != nullptr)
                                ? std::static_pointer_cast<erhe::scene::Node>(parent->shared_from_this())
                                : scene_root->get_hosted_scene()->get_root_node(),
                            .mode    = Scene_item_operation::Mode::insert
                        }
                    ),
                    std::make_shared<Attach_operation>(new_light, new_node)
                }
            }
        )
    );

    return new_light;
}

auto Scene_commands::create_new_rendertarget(
    erhe::scene::Node* parent
) -> std::shared_ptr<Rendertarget_mesh>
{
    Scene_root* scene_root = get_scene_root(parent);
    if (scene_root == nullptr) {
        return {};
    }

    auto new_node = std::make_shared<erhe::scene::Node>("new light node");
    auto new_mesh = std::make_shared<Rendertarget_mesh>(
        *m_context.graphics_instance,
        *m_context.mesh_memory,
        2048,
        2048,
        600.0f
    );
    new_mesh->mesh_data.layer_id = scene_root->layers().rendertarget()->id;
    new_mesh->enable_flag_bits(
        erhe::scene::Item_flags::rendertarget |
        erhe::scene::Item_flags::visible      |
        erhe::scene::Item_flags::translucent  |
        erhe::scene::Item_flags::show_in_ui
    );

    new_node = std::make_shared<erhe::scene::Node>("Hud RT node");
    new_node->set_parent_from_node(
        erhe::toolkit::mat4_rotate_xz_180
    );
    new_node->set_parent(scene_root->scene().get_root_node());
    new_node->attach(new_mesh);
    new_node->enable_flag_bits(
        erhe::scene::Item_flags::rendertarget |
        erhe::scene::Item_flags::visible      |
        erhe::scene::Item_flags::show_in_ui
    );
    auto node_raytrace = new_mesh->get_node_raytrace();
    if (node_raytrace) {
        new_node->attach(node_raytrace);
    }

    auto rendertarget_imgui_viewport = std::make_shared<Rendertarget_imgui_viewport>(
        *m_context.imgui_renderer,
        *m_context.imgui_windows,
        *m_context.rendergraph,
        m_context,
        new_mesh.get(),
        "Rendertarget Viewport",
        true
    );

    rendertarget_imgui_viewport->set_menu_visible(true);
    m_context.imgui_windows->queue(
        [this, rendertarget_imgui_viewport]() {
            m_context.imgui_windows->register_imgui_viewport(rendertarget_imgui_viewport.get());
        }
    );

    new_mesh->mesh_data.layer_id = scene_root->layers().rendertarget()->id;
    m_context.operation_stack->push(
        std::make_shared<Compound_operation>(
            Compound_operation::Parameters{
                .operations = {
                    std::make_shared<Item_insert_remove_operation>(
                        Item_insert_remove_operation::Parameters{
                            .context = m_context,
                            .item    = new_node,
                            .parent  = (parent != nullptr)
                                ? std::static_pointer_cast<erhe::scene::Node>(parent->shared_from_this())
                                : scene_root->get_hosted_scene()->get_root_node(),
                            .mode    = Scene_item_operation::Mode::insert
                        }
                    ),
                    std::make_shared<Attach_operation>(new_mesh, new_node)
                }
            }
        )
    );

    return new_mesh;
}

} // namespace editor
