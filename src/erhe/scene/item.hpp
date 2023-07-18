#pragma once

#include "erhe/toolkit/unique_id.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace erhe::scene
{

class Node;
class Scene;
class Item_host;

class Item_flags
{
public:
    static constexpr uint64_t none                      = 0u;
    static constexpr uint64_t no_message                = (1u <<  0);
    static constexpr uint64_t no_transform_update       = (1u <<  1);
    static constexpr uint64_t transform_world_normative = (1u <<  2);
    static constexpr uint64_t show_in_ui                = (1u <<  3);
    static constexpr uint64_t show_debug_visualizations = (1u <<  4);
    static constexpr uint64_t shadow_cast               = (1u <<  5);
    static constexpr uint64_t selected                  = (1u <<  6);
    static constexpr uint64_t lock_viewport_selection   = (1u <<  7);
    static constexpr uint64_t lock_viewport_transform   = (1u <<  8);
    static constexpr uint64_t visible                   = (1u <<  9);
    static constexpr uint64_t opaque                    = (1u << 10);
    static constexpr uint64_t translucent               = (1u << 11);
    static constexpr uint64_t render_wireframe          = (1u << 12); // TODO
    static constexpr uint64_t render_bounding_volume    = (1u << 13); // TODO
    static constexpr uint64_t content                   = (1u << 14);
    static constexpr uint64_t id                        = (1u << 15);
    static constexpr uint64_t tool                      = (1u << 16);
    static constexpr uint64_t brush                     = (1u << 17);
    static constexpr uint64_t controller                = (1u << 18);
    static constexpr uint64_t rendertarget              = (1u << 19);
    static constexpr uint64_t count                     = 20;

    static constexpr const char* c_bit_labels[] =
    {
        "No Message",
        "No Transform Update",
        "Transform World Normative",
        "Show In UI",
        "Show Debug",
        "Shadow Cast",
        "Selected",
        "Lock Selection",
        "Lock Transform",
        "Visible",
        "Opaque",
        "Translucent",
        "Render Wireframe",
        "Render Bounding Volume",
        "Content",
        "ID",
        "Tool",
        "Brush",
        "Controller",
        "Rendertarget"
    };

    [[nodiscard]] static auto to_string(uint64_t mask) -> std::string;
};

class Item_type
{
public:
    static constexpr uint64_t index_animation         =  1;
    static constexpr uint64_t index_animation_channel =  2;
    static constexpr uint64_t index_animation_sampler =  3;
    static constexpr uint64_t index_bone              =  4;
    static constexpr uint64_t index_brush             =  5;
    static constexpr uint64_t index_camera            =  6;
    static constexpr uint64_t index_composer          =  7;
    static constexpr uint64_t index_content_folder    =  8;
    static constexpr uint64_t index_frame_controller  =  9;
    static constexpr uint64_t index_grid              = 10;
    static constexpr uint64_t index_light             = 11;
    static constexpr uint64_t index_light_layer       = 12;
    static constexpr uint64_t index_material          = 13;
    static constexpr uint64_t index_mesh              = 14;
    static constexpr uint64_t index_mesh_layer        = 15;
    static constexpr uint64_t index_node              = 16;
    static constexpr uint64_t index_node_attachment   = 17;
    static constexpr uint64_t index_physics           = 18;
    static constexpr uint64_t index_raytrace          = 19;
    static constexpr uint64_t index_renderpass        = 20;
    static constexpr uint64_t index_rendertarget      = 21;
    static constexpr uint64_t index_scene             = 22;
    static constexpr uint64_t index_skin              = 23;
    static constexpr uint64_t index_texture           = 24;

    static constexpr uint64_t none              =  0u;
    static constexpr uint64_t animation         = (1u << index_animation        );
    static constexpr uint64_t animation_channel = (1u << index_animation_channel);
    static constexpr uint64_t animation_sampler = (1u << index_animation_sampler);
    static constexpr uint64_t bone              = (1u << index_bone             );
    static constexpr uint64_t brush             = (1u << index_brush            );
    static constexpr uint64_t camera            = (1u << index_camera           );
    static constexpr uint64_t composer          = (1u << index_composer         );
    static constexpr uint64_t content_folder    = (1u << index_content_folder   );
    static constexpr uint64_t frame_controller  = (1u << index_frame_controller );
    static constexpr uint64_t grid              = (1u << index_grid             );
    static constexpr uint64_t light             = (1u << index_light            );
    static constexpr uint64_t light_layer       = (1u << index_light_layer      );
    static constexpr uint64_t material          = (1u << index_material         );
    static constexpr uint64_t mesh              = (1u << index_mesh             );
    static constexpr uint64_t mesh_layer        = (1u << index_mesh_layer       );
    static constexpr uint64_t node              = (1u << index_node             );
    static constexpr uint64_t node_attachment   = (1u << index_node_attachment  );
    static constexpr uint64_t physics           = (1u << index_physics          );
    static constexpr uint64_t raytrace          = (1u << index_raytrace         );
    static constexpr uint64_t renderpass        = (1u << index_renderpass       );
    static constexpr uint64_t rendertarget      = (1u << index_rendertarget     );
    static constexpr uint64_t scene             = (1u << index_scene            );
    static constexpr uint64_t skin              = (1u << index_skin             );
    static constexpr uint64_t texture           = (1u << index_texture          );
    static constexpr uint64_t count             = 25;

    // NOTE: The names here must match the C++ class names
    static constexpr const char* c_bit_labels[] =
    {
        "none",
        "Animation",
        "Animation_channel",
        "Animation_sampler",
        "Bone",
        "Brush",
        "Camera",
        "Composer",
        "Content_folder",
        "Frame_controller",
        "Grid",
        "Light",
        "Light_layer",
        "Material",
        "Mesh",
        "Mesh_layer",
        "Node",
        "Node_attachment",
        "None",
        "Physics",
        "Raytrace",
        "Renderpass",
        "Rendertarget",
        "Scene",
        "Skin",
        "Texture"
    };

    [[nodiscard]] static auto to_string(uint64_t mask) -> std::string;
};

class Item_filter
{
public:
    [[nodiscard]] auto operator()(uint64_t filter_bits) const -> bool;

    auto describe() const -> std::string;

    uint64_t require_all_bits_set          {0};
    uint64_t require_at_least_one_bit_set  {0};
    uint64_t require_all_bits_clear        {0};
    uint64_t require_at_least_one_bit_clear{0};
};

class Item;

class Item_data
{
public:
    uint64_t                           flag_bits      {Item_flags::none};
    glm::vec4                          wireframe_color{0.6f, 0.7f, 0.8f, 0.7f};
    std::string                        name;
    std::string                        label;
    std::filesystem::path              source_path;

    std::weak_ptr<Item>                parent{};
    std::vector<std::shared_ptr<Item>> children;
    std::size_t                        depth{0};

    static constexpr unsigned int bit_children {1u << 0};
    static constexpr unsigned int bit_depth    {1u << 1};
    static constexpr unsigned int bit_flag_bits{1u << 2};
    static constexpr unsigned int bit_host     {1u << 3};
    static constexpr unsigned int bit_label    {1u << 4};
    static constexpr unsigned int bit_name     {1u << 5};
    static constexpr unsigned int bit_parent   {1u << 6};

    //// static auto diff_mask(const Item_data& lhs, const Item_data& rhs) -> unsigned int;
};

class Item
    : public std::enable_shared_from_this<Item>
{
public:
    Item();
    explicit Item(const std::string_view name);
    virtual ~Item() noexcept;

    [[nodiscard]] virtual auto get_item_host       () const -> Item_host*;
    [[nodiscard]] virtual auto get_type            () const -> uint64_t;
    [[nodiscard]] virtual auto get_type_name       () const -> const char*;
    [[nodiscard]] virtual auto get_parent          () const -> std::weak_ptr<Item>;
    [[nodiscard]] virtual auto get_depth           () const -> size_t;
    [[nodiscard]] virtual auto get_children        () const -> const std::vector<std::shared_ptr<Item>>&;
    [[nodiscard]] virtual auto get_mutable_children() -> std::vector<std::shared_ptr<Item>>&;
    [[nodiscard]] virtual auto get_root            () -> std::weak_ptr<Item>;
    [[nodiscard]] virtual auto get_child_count     () const -> std::size_t;
    [[nodiscard]] virtual auto get_child_count     (const Item_filter& filter) const -> std::size_t;
    [[nodiscard]] virtual auto get_index_in_parent () const -> std::size_t;
    [[nodiscard]] virtual auto get_index_of_child  (const Item* child) const -> std::optional<std::size_t>;
    [[nodiscard]] virtual auto is_ancestor         (const Item* ancestor_candidate) const -> bool;

    virtual void set_parent             (const std::shared_ptr<Item>& parent, std::size_t position = 0);
    virtual void set_visible            (bool value);
    virtual void handle_flag_bits_update(uint64_t old_flag_bits, uint64_t new_flag_bits );
    virtual void handle_add_child       (const std::shared_ptr<Item>& child_node, std::size_t position = 0);
    virtual void handle_remove_child    (Item* child_node);
    virtual void handle_parent_update   (Item* old_parent, Item* new_parent);

    [[nodiscard]] auto get_name                    () const -> const std::string&;
    [[nodiscard]] auto get_label                   () const -> const std::string&;
    [[nodiscard]] auto get_flag_bits               () const -> uint64_t;
    [[nodiscard]] auto get_id                      () const -> erhe::toolkit::Unique_id<Item>::id_type;
    [[nodiscard]] auto is_no_transform_update      () const -> bool;
    [[nodiscard]] auto is_transform_world_normative() const -> bool;
    [[nodiscard]] auto is_selected                 () const -> bool;
    [[nodiscard]] auto is_visible                  () const -> bool;
    [[nodiscard]] auto is_shown_in_ui              () const -> bool;
    [[nodiscard]] auto is_hidden                   () const -> bool;
    [[nodiscard]] auto describe                    () const -> std::string;
    [[nodiscard]] auto get_wireframe_color         () const -> glm::vec4;
    [[nodiscard]] auto get_source_path             () const -> const std::filesystem::path&;

    void remove                ();
    void recursive_remove      ();
    void set_parent            (Item* parent, std::size_t position = 0);
    void set_depth_recursive   (std::size_t depth);
    void sanity_check_root_path(const Item* node) const;
    void set_name              (const std::string_view name);
    void set_flag_bits         (uint64_t mask, bool value);
    void enable_flag_bits      (uint64_t mask);
    void disable_flag_bits     (uint64_t mask);
    void set_selected          (bool value);
    void show                  ();
    void hide                  ();
    void set_wireframe_color   (const glm::vec4& color);
    void set_source_path       (const std::filesystem::path& path);
    void trace                 ();
    void item_sanity_check     () const;

protected:
    erhe::toolkit::Unique_id<Item> m_id;
    Item_data                      item_data;
};

} // namespace erhe::scene
