#include "scene/scene_root.hpp"

#include "editor_log.hpp"
#include "rendertarget_mesh.hpp"

#include "scene/content_library.hpp"
#include "scene/node_physics.hpp"
#include "scene/node_raytrace.hpp"

#include "erhe/primitive/material.hpp"
#include "erhe/physics/iworld.hpp"
#include "erhe/physics/irigid_body.hpp"
#include "erhe/raytrace/iinstance.hpp"
#include "erhe/raytrace/iscene.hpp"
#include "erhe/scene/camera.hpp"
#include "erhe/scene/light.hpp"
#include "erhe/scene/mesh.hpp"
#include "erhe/scene/node.hpp"
#include "erhe/scene/scene.hpp"
#include "erhe/scene/skin.hpp"
#include "erhe/toolkit/profile.hpp"
#include "erhe/toolkit/verify.hpp"

#if defined(ERHE_GUI_LIBRARY_IMGUI)
#   include <imgui.h>
#endif

#include <algorithm>

namespace editor
{

using erhe::scene::Light;
using erhe::scene::Light_layer;
using erhe::scene::Mesh;
using erhe::scene::Mesh_layer;
using erhe::scene::Scene;
using erhe::primitive::Material;

Scene_layers::Scene_layers()
{
    m_brush        = std::make_shared<Mesh_layer>("brush",        erhe::scene::Item_flags::brush,        Mesh_layer_id::brush);
    m_content      = std::make_shared<Mesh_layer>("content",      erhe::scene::Item_flags::content,      Mesh_layer_id::content);
    m_controller   = std::make_shared<Mesh_layer>("controller",   erhe::scene::Item_flags::controller,   Mesh_layer_id::controller);
    m_rendertarget = std::make_shared<Mesh_layer>("rendertarget", erhe::scene::Item_flags::rendertarget, Mesh_layer_id::rendertarget);
    m_tool         = std::make_shared<Mesh_layer>("tool",         erhe::scene::Item_flags::tool,         Mesh_layer_id::tool);

    m_light = std::make_shared<Light_layer>("lights", 0);
}

void Scene_layers::add_layers_to_scene(erhe::scene::Scene& scene)
{
    scene.add_mesh_layer(m_brush);
    scene.add_mesh_layer(m_content);
    scene.add_mesh_layer(m_controller);
    scene.add_mesh_layer(m_rendertarget);
    scene.add_mesh_layer(m_tool);

    scene.add_light_layer(m_light);
}

auto Scene_layers::brush() const -> erhe::scene::Mesh_layer*
{
    return m_brush.get();
}

auto Scene_layers::content() const -> erhe::scene::Mesh_layer*
{
    return m_content.get();
}

auto Scene_layers::controller() const -> erhe::scene::Mesh_layer*
{
    return m_controller.get();
}

auto Scene_layers::tool() const -> erhe::scene::Mesh_layer*
{
    return m_tool.get();
}

auto Scene_layers::rendertarget() const -> erhe::scene::Mesh_layer*
{
    return m_rendertarget.get();
}

auto Scene_layers::light() const -> erhe::scene::Light_layer*
{
    return m_light.get();
}

Scene_root::Scene_root(
    erhe::scene::Scene_message_bus&         scene_message_bus,
    const std::shared_ptr<Content_library>& content_library,
    const std::string_view                  name
)
    : m_content_library{content_library}
{
    ERHE_PROFILE_FUNCTION();

    m_scene = std::make_shared<Scene>(scene_message_bus, name, this);
    m_layers.add_layers_to_scene(*m_scene.get());

    // Layer configuration
    using std::make_shared;
    using std::make_unique;
    using erhe::scene::Node;

    m_scene->enable_flag_bits(erhe::scene::Item_flags::show_in_ui);
    m_physics_world  = erhe::physics::IWorld::create_unique();
    m_raytrace_scene = erhe::raytrace::IScene::create_unique("root");
}

[[nodiscard]] auto Scene_root::get_host_name() const -> const char*
{
    return "Scene_root";
}

[[nodiscard]] auto Scene_root::get_hosted_scene() -> Scene*
{
    return m_scene.get();
}

void Scene_root::register_node(const std::shared_ptr<erhe::scene::Node>& node)
{
    if (m_scene) {
        m_scene->register_node(node);
    }
}

void Scene_root::unregister_node(const std::shared_ptr<erhe::scene::Node>& node)
{
    if (m_scene) {
        m_scene->unregister_node(node);
    }
}

void Scene_root::register_camera(const std::shared_ptr<erhe::scene::Camera>& camera)
{
    if (m_scene) {
        m_scene->register_camera(camera);
    }
}
void Scene_root::unregister_camera(const std::shared_ptr<erhe::scene::Camera>& camera)
{
    if (m_scene) {
        m_scene->unregister_camera(camera);
    }
}

void Scene_root::register_mesh(const std::shared_ptr<erhe::scene::Mesh>& mesh)
{
    ERHE_VERIFY(mesh);

    if (m_scene) {
        m_scene->register_mesh(mesh);
    }

    if (is_rendertarget(mesh)) {
        const std::lock_guard<std::mutex> lock{m_rendertarget_meshes_mutex};
        m_rendertarget_meshes.push_back(as_rendertarget(mesh));
    }

    // Make sure materials are in the material library
    auto& material_library = content_library()->materials;
    for (const auto& primitive : mesh->mesh_data.primitives) {
        if (!primitive.material) {
            continue;
        }
        material_library.add(primitive.material);
    }
}

void Scene_root::unregister_mesh(const std::shared_ptr<erhe::scene::Mesh>& mesh)
{
    if (is_rendertarget(mesh)) {
        const std::lock_guard<std::mutex> lock{m_rendertarget_meshes_mutex};
        const auto rendertarget = as_rendertarget(mesh);
        const auto i = std::remove(m_rendertarget_meshes.begin(), m_rendertarget_meshes.end(), rendertarget);
        if (i == m_rendertarget_meshes.end()) {
            log_scene->error("rendertarget mesh {} not in scene root", rendertarget->get_name());
        } else {
            m_rendertarget_meshes.erase(i, m_rendertarget_meshes.end());
        }
    }
    if (m_scene) {
        m_scene->unregister_mesh(mesh);
    }

    // TODO reference count? Remove materials from material library
    // auto& material_library = content_library()->materials;
    // material_library.remove(m_material);
}

void Scene_root::register_skin(const std::shared_ptr<erhe::scene::Skin>& skin)
{
    if (m_scene) {
        m_scene->register_skin(skin);
    }
}

void Scene_root::unregister_skin(const std::shared_ptr<erhe::scene::Skin>& skin)
{
    if (m_scene) {
        m_scene->unregister_skin(skin);
    }
}

void Scene_root::register_light(const std::shared_ptr<erhe::scene::Light>& light)
{
    if (m_scene) {
        m_scene->register_light(light);
    }
}

void Scene_root::unregister_light(const std::shared_ptr<erhe::scene::Light>& light)
{
    if (m_scene) {
        m_scene->unregister_light(light);
    }
}

void Scene_root::register_node_physics(Node_physics* node_physics)
{
#ifndef NDEBUG
    const auto i = std::find(m_node_physics.begin(), m_node_physics.end(), node_physics);
    if (i != m_node_physics.end()) {
        auto* node = node_physics->get_node();
        log_physics->error("Node_physics for '{}' already in Scene_root", (node != nullptr) ? node->get_name().c_str() : "");
    } else
#endif
    {
        m_node_physics.push_back(node_physics);
        m_node_physics_sorted = false;
    }

    if (m_physics_world) {
        node_physics->set_physics_world(m_physics_world.get());
        erhe::physics::IRigid_body* rigid_body = node_physics->get_rigid_body();
        if (rigid_body != nullptr) {
            m_physics_world->add_rigid_body(node_physics->get_rigid_body());
        }
    }
}

void Scene_root::unregister_node_physics(Node_physics* node_physics)
{
    const auto i = std::remove(
        m_node_physics.begin(),
        m_node_physics.end(),
        node_physics
    );
    if (i == m_node_physics.end()) {
        auto* node = node_physics->get_node();
        log_physics->error("Node_physics for '{}' not in Scene_root", (node != nullptr) ? node->get_name().c_str() : "");
    } else {
        m_node_physics.erase(i, m_node_physics.end());
        m_node_physics_sorted = false;
    }

    if (m_physics_world) {
        erhe::physics::IRigid_body* rigid_body = node_physics->get_rigid_body();
        if (rigid_body != nullptr) {
            m_physics_world->remove_rigid_body(node_physics->get_rigid_body());
        }
        node_physics->set_physics_world(nullptr);
    }
}

void Scene_root::update_physics_simulation_fixed_step(const double dt)
{
    if (m_physics_world) {
        m_physics_world->update_fixed_step(dt);
    }
}

void Scene_root::update_physics_simulation_once_per_frame()
{
    // Sort nodes, so that parent transforms are updated before child nodes
    if (!m_node_physics_sorted) {
        std::sort(
            m_node_physics.begin(),
            m_node_physics.end(),
            [](Node_physics* lhs, Node_physics* rhs) -> bool {
                erhe::scene::Node* lhs_node = lhs->get_node();
                erhe::scene::Node* rhs_node = rhs->get_node();
                if ((lhs_node == nullptr) || (rhs_node == nullptr)) {
                    return true;
                }
                return lhs_node->get_depth() < rhs_node->get_depth();
            }
        );
    }

    for (Node_physics* node_physics : m_node_physics) {
        auto* rigid_body = node_physics->get_rigid_body();
        if (rigid_body) {
            if (rigid_body->is_active()) {
                const glm::mat4 transform = rigid_body->get_world_transform();
                node_physics->set_world_from_node(transform);
            } else {
                log_physics_frame->trace("{} is sleeping", rigid_body->get_debug_label());
            }
        }
    }
}

//
void Scene_root::register_node_raytrace(Node_raytrace* node_raytrace)
{
#ifndef NDEBUG
    const auto i = std::find(m_node_raytraces.begin(), m_node_raytraces.end(), node_raytrace);
    if (i != m_node_raytraces.end()) {
        auto* node = node_raytrace->get_node();
        log_raytrace->error("Node_raytrace for '{}' already in Scene_root", (node != nullptr) ? node->get_name().c_str() : "");
    } else
#endif
    {
        m_node_raytraces.push_back(node_raytrace);
    }

    if (m_raytrace_scene) {
        erhe::raytrace::IInstance* rt_instance = node_raytrace->raytrace_instance();
        m_raytrace_scene->attach(rt_instance);
        uint32_t mask = 0;
        for (const auto& node_attachment : node_raytrace->get_node()->get_attachments()) {
            mask = mask | raytrace_node_mask(*node_attachment.get());
        }
        rt_instance->set_mask(mask);
        log_raytrace->trace("RT {} attached, mask = {}", node_raytrace->get_label(), mask);
    }
}

void Scene_root::unregister_node_raytrace(Node_raytrace* node_raytrace)
{
    const auto i = std::remove(
        m_node_raytraces.begin(),
        m_node_raytraces.end(),
        node_raytrace
    );
    if (i == m_node_raytraces.end()) {
        auto* node = node_raytrace->get_node();
        log_raytrace->error("Node_raytrace for '{}' not in Scene_root", (node != nullptr) ? node->get_name().c_str() : "");
    } else {
        m_node_raytraces.erase(i, m_node_raytraces.end());
    }

    if (m_raytrace_scene) {
        erhe::raytrace::IInstance* rt_instance = node_raytrace->raytrace_instance();
        m_raytrace_scene->detach(rt_instance);
        log_raytrace->trace("RT {} detached", node_raytrace->get_label());
    }
}

//

[[nodiscard]] auto Scene_root::get_shared_scene() -> std::shared_ptr<erhe::scene::Scene>
{
    return m_scene;
}

[[nodiscard]] auto Scene_root::layers() -> Scene_layers&
{
    return m_layers;
}

[[nodiscard]] auto Scene_root::layers() const -> const Scene_layers&
{
    return m_layers;
}

auto Scene_root::get_physics_world() -> erhe::physics::IWorld&
{
    ERHE_VERIFY(m_physics_world);
    return *m_physics_world.get();
}

auto Scene_root::get_raytrace_scene() -> erhe::raytrace::IScene&
{
    ERHE_VERIFY(m_raytrace_scene);
    return *m_raytrace_scene.get();
}

auto Scene_root::scene() -> erhe::scene::Scene&
{
    ERHE_VERIFY(m_scene);
    return *m_scene.get();
}

auto Scene_root::scene() const -> const erhe::scene::Scene&
{
    ERHE_VERIFY(m_scene);
    return *m_scene.get();
}

[[nodiscard]] auto Scene_root::get_name() const -> const std::string&
{
    ERHE_VERIFY(m_scene);
    return m_scene->get_name();
}

#if defined(ERHE_GUI_LIBRARY_IMGUI)
auto Scene_root::camera_combo(
    const char*           label,
    erhe::scene::Camera*& selected_camera,
    const bool            nullptr_option
) const -> bool
{
    int selected_camera_index = 0;
    int index = 0;
    std::vector<const char*>          names;
    std::vector<erhe::scene::Camera*> cameras;
    if (nullptr_option || (selected_camera == nullptr)) {
        names.push_back("(none)");
        cameras.push_back(nullptr);
    }
    const auto& scene_cameras = scene().get_cameras();
    for (const auto& camera : scene_cameras) {
        names.push_back(camera->get_name().c_str());
        cameras.push_back(camera.get());
        if (selected_camera == camera.get()) {
            selected_camera_index = index;
        }
        ++index;
    }

    const bool camera_changed =
        ImGui::Combo(
            label,
            &selected_camera_index,
            names.data(),
            static_cast<int>(names.size()),
            static_cast<int>(names.size())
        ) &&
        (selected_camera != cameras[selected_camera_index]);
    if (camera_changed) {
        selected_camera = cameras[selected_camera_index];
    }
    return camera_changed;
}

auto Scene_root::camera_combo(
    const char*                           label,
    std::shared_ptr<erhe::scene::Camera>& selected_camera,
    const bool                            nullptr_option
) const -> bool
{
    int selected_camera_index = 0;
    int index = 0;
    std::vector<const char*> names;
    std::vector<std::shared_ptr<erhe::scene::Camera>> cameras;
    if (nullptr_option || (selected_camera == nullptr)) {
        names.push_back("(none)");
        cameras.push_back(nullptr);
    }
    const auto& scene_cameras = scene().get_cameras();
    for (const auto& camera : scene_cameras) {
        names.push_back(camera->get_name().c_str());
        cameras.push_back(camera);
        if (selected_camera == camera) {
            selected_camera_index = index;
        }
        ++index;
    }

    const bool camera_changed =
        ImGui::Combo(
            label,
            &selected_camera_index,
            names.data(),
            static_cast<int>(names.size()),
            static_cast<int>(names.size())
        ) &&
        (selected_camera != cameras[selected_camera_index]);
    if (camera_changed) {
        selected_camera = cameras[selected_camera_index];
    }
    return camera_changed;
}

auto Scene_root::camera_combo(
    const char*                         label,
    std::weak_ptr<erhe::scene::Camera>& selected_camera,
    const bool                          nullptr_option
) const -> bool
{
    int selected_camera_index = 0;
    int index = 0;
    std::vector<const char*> names;
    std::vector<std::weak_ptr<erhe::scene::Camera>> cameras;
    if (nullptr_option || selected_camera.expired()) {
        names.push_back("(none)");
        cameras.push_back({});
    }
    const auto& scene_cameras = scene().get_cameras();
    for (const auto& camera : scene_cameras) {
        names.push_back(camera->get_name().c_str());
        cameras.push_back(camera);
        if (selected_camera.lock() == camera) {
            selected_camera_index = index;
        }
        ++index;
    }

    const bool camera_changed =
        ImGui::Combo(
            label,
            &selected_camera_index,
            names.data(),
            static_cast<int>(names.size()),
            static_cast<int>(names.size())
        ) &&
        (selected_camera.lock() != cameras[selected_camera_index].lock());
    if (camera_changed) {
        selected_camera = cameras[selected_camera_index];
    }
    return camera_changed;
}

#endif

namespace
{

[[nodiscard]] auto sort_value(const Light::Type light_type) -> int
{
    switch (light_type) {
        //using enum erhe::scene::Light_type;
        case erhe::scene::Light_type::directional: return 0;
        case erhe::scene::Light_type::point:       return 1;
        case erhe::scene::Light_type::spot:        return 2;
        default: return 3;
    }
}

class Light_comparator
{
public:
    [[nodiscard]] inline auto operator()(
        const std::shared_ptr<Light>& lhs,
        const std::shared_ptr<Light>& rhs
    ) -> bool
    {
        return sort_value(lhs->type) < sort_value(rhs->type);
    }
};

}

void Scene_root::sort_lights()
{
    std::sort(
        m_layers.light()->lights.begin(),
        m_layers.light()->lights.end(),
        Light_comparator()
    );
}

void Scene_root::update_pointer_for_rendertarget_meshes(Scene_view* scene_view)
{
    std::lock_guard<std::mutex> lock(m_rendertarget_meshes_mutex);

    for (const auto& rendertarget_mesh : m_rendertarget_meshes) {
        rendertarget_mesh->update_pointer(scene_view);
    }
}

auto Scene_root::content_library() const -> std::shared_ptr<Content_library>
{
    return m_content_library;
}

void Scene_root::sanity_check()
{
    m_scene->sanity_check();
}

} // namespace editor
