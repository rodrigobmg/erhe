#pragma once

#if defined(ERHE_XR_LIBRARY_OPENXR)
#   include "xr/hand_tracker.hpp"
#endif

#include "erhe/scene/mesh.hpp"

#include <glm/glm.hpp>

namespace erhe::graphics {
    class Framebuffer;
    class Instance;
    class Sampler;
    class Texture;
}
namespace erhe::primitive {
    class Material;
}
namespace erhe::scene {
    class Item_host;
}

namespace editor
{

class Editor_context;
class Editor_message;
class Forward_renderer;
class Hand_tracker;
class Mesh_memory;
class Node_raytrace;
class Render_context;
class Scene_root;
class Scene_view;
class Viewport_config_window;
class Viewport_window;

class Rendertarget_mesh
    : public erhe::scene::Mesh
{
public:
    Rendertarget_mesh(
        erhe::graphics::Instance& graphics_instance,
        Mesh_memory&              mesh_memory,
        int                       width,
        int                       height,
        float                     pixels_per_meter
    );

    // Implements Item
    [[nodiscard]] static auto get_static_type     () -> uint64_t;
    [[nodiscard]] static auto get_static_type_name() -> const char*;
    [[nodiscard]] auto get_type     () const -> uint64_t    override;
    [[nodiscard]] auto get_type_name() const -> const char* override;

    // Public API
    [[nodiscard]] auto texture         () const -> std::shared_ptr<erhe::graphics::Texture>;
    [[nodiscard]] auto framebuffer     () const -> std::shared_ptr<erhe::graphics::Framebuffer>;
    [[nodiscard]] auto width           () const -> float;
    [[nodiscard]] auto height          () const -> float;
    [[nodiscard]] auto pixels_per_meter() const -> float;
    [[nodiscard]] auto get_pointer     () const -> std::optional<glm::vec2>;
    [[nodiscard]] auto world_to_window (glm::vec3 world_position) const -> std::optional<glm::vec2>;

    [[nodiscard]] auto get_node_raytrace() -> std::shared_ptr<Node_raytrace>;

#if defined(ERHE_XR_LIBRARY_OPENXR)
    void update_headset();
#endif

    auto update_pointer(Scene_view* scene_view) -> bool;
    void bind          ();
    void clear         (glm::vec4 clear_color);
    void render_done   (Editor_context& context); // generates mipmaps, updates lod bias

    void resize_rendertarget(
        erhe::graphics::Instance& graphics_instance,
        Mesh_memory&              mesh_memory,
        int                       width,
        int                       height
    );

private:
    void make_primitive(Mesh_memory& mesh_memory);

    float                                        m_pixels_per_meter{0.0f};
    float                                        m_local_width     {0.0f};
    float                                        m_local_height    {0.0f};

    std::shared_ptr<erhe::graphics::Texture>     m_texture;
    std::shared_ptr<erhe::graphics::Sampler>     m_sampler;
    std::shared_ptr<erhe::primitive::Material>   m_material;
    std::shared_ptr<erhe::graphics::Framebuffer> m_framebuffer;
    std::shared_ptr<Node_raytrace>               m_node_raytrace;
    std::optional<glm::vec2>                     m_pointer;

#if defined(ERHE_XR_LIBRARY_OPENXR)
    std::optional<Finger_point> m_pointer_finger;
    bool                        m_finger_trigger{false};
#endif
};

[[nodiscard]] auto is_rendertarget(const erhe::scene::Item* scene_item) -> bool;
[[nodiscard]] auto is_rendertarget(const std::shared_ptr<erhe::scene::Item>& scene_item) -> bool;
[[nodiscard]] auto as_rendertarget(erhe::scene::Item* scene_item) -> Rendertarget_mesh*;
[[nodiscard]] auto as_rendertarget(const std::shared_ptr<erhe::scene::Item>& scene_item) -> std::shared_ptr<Rendertarget_mesh>;

[[nodiscard]] auto get_rendertarget(const erhe::scene::Node* node) -> std::shared_ptr<Rendertarget_mesh>;

} // namespace editor
