#pragma once

#include "operations/compound_operation.hpp"

#include "erhe/imgui/imgui_window.hpp"
#include "erhe/scene/node.hpp"
#include "erhe/toolkit/unique_id.hpp"

#include "robin_hood.h"

#include <functional>
#include <memory>

namespace erhe::imgui {
    class Imgui_windows;
}
namespace erhe::scene {
    class Node;
    class Scene_message_bus;
}

namespace editor
{

class Editor_scenes;
class Editor_settings;
class Icon_set;
class IOperation;
class Operation_stack;
class Scene_commands;
class Selection_tool;

enum class Placement : unsigned int
{
    Before_anchor = 0, // Place node before anchor
    After_anchor,      // Place node after anchor
    Last               // Place node as last child of parent, ignore anchor
};

enum class Selection_usage : unsigned int
{
    Selection_ignored = 0,
    Selection_used
};

class Tree_node_state
{
public:
    bool is_open      {false};
    bool need_tree_pop{false};
};

class Item_tree_window
    : public erhe::imgui::Imgui_window
{
public:
    Item_tree_window(
        erhe::imgui::Imgui_renderer& imgui_renderer,
        erhe::imgui::Imgui_windows&  imgui_windows,
        Editor_context&              context
    );

    // Implements Imgui_window
    void imgui   () override;
    void on_begin() override;
    void on_end  () override;

private:
    void set_item_selection_terminator(const std::shared_ptr<erhe::scene::Item>& item);
    void set_item_selection           (const std::shared_ptr<erhe::scene::Item>& item, bool selected);
    void clear_selection              ();
    void recursive_add_to_selection   (const std::shared_ptr<erhe::scene::Item>& node);
    void select_all                   ();

    void move_selection(
        const std::shared_ptr<erhe::scene::Item>& target,
        std::size_t                               payload_id,
        Placement                                 placement
    );
    void attach_selection_to(
        const std::shared_ptr<erhe::scene::Item>& target_node,
        std::size_t                               payload_id
    );

    void item_popup_menu      (const std::shared_ptr<erhe::scene::Item>& item);
    void item_icon            (const std::shared_ptr<erhe::scene::Item>& item);
    auto item_icon_and_text   (const std::shared_ptr<erhe::scene::Item>& item, bool update) -> Tree_node_state;
    void item_update_selection(const std::shared_ptr<erhe::scene::Item>& item);
    void imgui_item_node      (const std::shared_ptr<erhe::scene::Item>& item);

    auto get_item_by_id(
        std::size_t id
    ) const -> std::shared_ptr<erhe::scene::Item>;

    void try_add_to_attach(
        Compound_operation::Parameters&           compound_parameters,
        const std::shared_ptr<erhe::scene::Item>& target_node,
        const std::shared_ptr<erhe::scene::Item>& node,
        Selection_usage                           selection_usage
    );

    void reposition(
        Compound_operation::Parameters&           compound_parameters,
        const std::shared_ptr<erhe::scene::Item>& anchor_node,
        const std::shared_ptr<erhe::scene::Item>& child_node,
        Placement                                 placement,
        Selection_usage                           selection_usage
    );
    void drag_and_drop_source(const std::shared_ptr<erhe::scene::Item>& node);
    auto drag_and_drop_target(const std::shared_ptr<erhe::scene::Item>& node) -> bool;

    robin_hood::unordered_map<
        erhe::toolkit::Unique_id<erhe::scene::Item>::id_type,
        std::shared_ptr<erhe::scene::Item>
    > m_tree_items;

    robin_hood::unordered_map<
        erhe::toolkit::Unique_id<erhe::scene::Item>::id_type,
        std::shared_ptr<erhe::scene::Item>
    > m_tree_items_last_frame;

    Editor_context&                    m_context;

    std::shared_ptr<IOperation>        m_operation;
    std::vector<std::function<void()>> m_operations;

    bool                               m_toggled_open{false};
    std::weak_ptr<erhe::scene::Item>   m_last_focus_item;
    std::shared_ptr<erhe::scene::Item> m_popup_item;
    std::string                        m_popup_id_string;
    unsigned int                       m_popup_id{0};

    erhe::scene::Item_filter           m_filter;
};

} // namespace editor
