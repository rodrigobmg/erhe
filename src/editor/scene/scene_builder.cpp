
#include "scene/scene_builder.hpp"

#include "editor_rendering.hpp"
#include "editor_scenes.hpp"
#include "editor_settings.hpp"
#include "task_queue.hpp"

#include "tools/brushes/brush.hpp"
#include "parsers/gltf.hpp"
#include "parsers/json_polyhedron.hpp"
#include "parsers/wavefront_obj.hpp"
#include "renderers/mesh_memory.hpp"
#include "scene/content_library.hpp"
#include "scene/material_library.hpp"
#include "scene/scene_root.hpp"
#include "scene/viewport_scene_view.hpp"
#include "scene/viewport_scene_views.hpp"
#include "windows/item_tree_window.hpp"
#include "windows/settings_window.hpp"

#include "SkylineBinPack.h" // RectangleBinPack

#include "erhe_configuration/configuration.hpp"
#include "erhe_imgui/imgui_windows.hpp"
#include "erhe_imgui/imgui_windows.hpp"
#include "erhe_rendergraph/rendergraph.hpp"
#include "erhe_geometry/shapes/box.hpp"
#include "erhe_geometry/shapes/cone.hpp"
#include "erhe_geometry/shapes/sphere.hpp"
#include "erhe_geometry/shapes/torus.hpp"
#include "erhe_geometry/shapes/regular_polyhedron.hpp"
#include "erhe_graphics/buffer_transfer_queue.hpp"
#include "erhe_math/math_util.hpp"
#include "erhe_physics/icollision_shape.hpp"
#include "erhe_physics/iworld.hpp"
#include "erhe_primitive/primitive.hpp"
#include "erhe_primitive/primitive_builder.hpp"
#include "erhe_primitive/material.hpp"
#include "erhe_primitive/material.hpp"
#include "erhe_profile/profile.hpp"
#include "erhe_raytrace/iscene.hpp"
#include "erhe_scene_renderer/shadow_renderer.hpp"
#include "erhe_scene/camera.hpp"
#include "erhe_scene/light.hpp"
#include "erhe_scene/mesh.hpp"
#include "erhe_scene/node.hpp"
#include "erhe_scene/scene.hpp"
#include "erhe_verify/verify.hpp"

#include <fmt/format.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>

#define ERHE_ENABLE_SECOND_CAMERA 1

namespace editor {

using erhe::geometry::shapes::make_dodecahedron;
using erhe::geometry::shapes::make_icosahedron;
using erhe::geometry::shapes::make_octahedron;
using erhe::geometry::shapes::make_tetrahedron;
using erhe::geometry::shapes::make_cuboctahedron;
using erhe::geometry::shapes::make_cube;
using erhe::geometry::shapes::make_cone;
using erhe::geometry::shapes::make_sphere;
using erhe::geometry::shapes::make_torus;
using erhe::geometry::shapes::torus_volume;
using erhe::geometry::shapes::make_cylinder;
using erhe::scene::Light;
using erhe::scene::Node;
using erhe::scene::Projection;
using erhe::Item_flags;
using erhe::geometry::shapes::make_box;
using erhe::primitive::Normal_style;
using glm::mat3;
using glm::mat4;
using glm::ivec3;
using glm::vec2;
using glm::vec3;
using glm::vec4;

Scene_builder::Config::Config()
{
    const auto& ini = erhe::configuration::get_ini_file_section("erhe.ini", "scene");
    ini.get("camera_exposure",             camera_exposure);
    ini.get("directional_light_intensity", directional_light_intensity);
    ini.get("directional_light_radius",    directional_light_radius);
    ini.get("directional_light_height",    directional_light_height);
    ini.get("directional_light_count",     directional_light_count);
    ini.get("spot_light_intensity",        spot_light_intensity);
    ini.get("spot_light_radius",           spot_light_radius);
    ini.get("spot_light_height",           spot_light_height);
    ini.get("spot_light_count",            spot_light_count);
    ini.get("floor_size",                  floor_size);
    ini.get("mass_scale",                  mass_scale);
    ini.get("detail",                      detail);
    ini.get("floor",                       floor);
}

Scene_builder::Scene_builder(
    erhe::graphics::Instance&       graphics_instance,
    erhe::imgui::Imgui_renderer&    imgui_renderer,
    erhe::imgui::Imgui_windows&     imgui_windows,
    erhe::rendergraph::Rendergraph& rendergraph,
    erhe::scene::Scene_message_bus& scene_message_bus,
    Editor_context&                 editor_context,
    Editor_message_bus&             editor_message_bus,
    Editor_rendering&               editor_rendering,
    Editor_scenes&                  editor_scenes,
    Editor_settings&                editor_settings,
    Mesh_memory&                    mesh_memory,
    Post_processing&                post_processing,
    Tools&                          tools,
    Scene_views&                    scene_views
)
    : m_context{editor_context}
{
    ERHE_PROFILE_FUNCTION();

    auto content_library = std::make_shared<Content_library>();
    add_default_materials(*content_library.get());

    m_scene_root = std::make_shared<Scene_root>(
        &imgui_renderer,
        &imgui_windows,
        scene_message_bus,
        &editor_context,
        &editor_message_bus,
        &editor_scenes,
        content_library,
        "Default Scene"
    );
    auto browser_window = m_scene_root->make_browser_window(imgui_renderer, imgui_windows, editor_context, editor_settings);

    setup_cameras(
        graphics_instance,
        imgui_renderer,
        imgui_windows,
        rendergraph,
        editor_rendering,
        editor_settings,
        post_processing,
        tools,
        scene_views
    );
    setup_lights   ();
    make_brushes   (editor_settings, mesh_memory);
    add_room       ();
}

void Scene_builder::add_rendertarget_viewports(int count)
{
    static_cast<void>(count);
#if defined(ERHE_GUI_LIBRARY_IMGUI) && 0 //// TODO
    const auto& test_scene_root = get_scene_root();

    if (count >= 1) {
        auto rendertarget_node_1 = std::make_shared<erhe::scene::Node>("RT Node 1");
        auto rendertarget_mesh_1 = std::make_shared<Rendertarget_mesh>(
            1920,  // width
            1080,  // height
            2000.0f // pixels per meter
        );
        rendertarget_mesh_1->layer_id = Mesh_layer_id::rendertarget;
        rendertarget_node_1->attach(rendertarget_mesh_1);
        rendertarget_node_1->set_parent(test_scene_root->get_scene().get_root_node());

        rendertarget_node_1->set_world_from_node(
            erhe::math::create_look_at(
                glm::vec3{-0.3f, 0.6f, -0.3f},
                glm::vec3{ 0.0f, 0.7f,  0.0f},
                glm::vec3{ 0.0f, 1.0f,  0.0f}
            )
        );

        auto imgui_viewport_1 = std::make_shared<editor::Rendertarget_imgui_host>(
            rendertarget_mesh_1.get(),
            "Rendertarget ImGui Viewport 1"
        );

        g_grid_tool->set_viewport(imgui_viewport_1.get());
        g_grid_tool->show();

#if defined(ERHE_XR_LIBRARY_OPENXR)
        if (g_headset_view != nullptr)
        {
            g_headset_view->set_viewport(imgui_viewport_1.get());
        }
#endif
    }

    if (count >= 2) {
        const auto camera_b = make_camera(
            "Camera B",
            glm::vec3{-7.0f, 1.0f, 0.0f},
            glm::vec3{ 0.0f, 0.5f, 0.0f}
        );
        //auto* camera_node_b = camera_b->get_node();
        camera_b->set_wireframe_color(glm::vec4{ 0.3f, 0.6f, 1.00f, 1.0f });

        auto secondary_viewport_window = g_viewport_windows->create_viewport_scene_view(
            "Secondary Viewport",
            test_scene_root,
            camera_b,
            2 // low MSAA
        );
        auto secondary_Imgui_window_scene_view = g_viewport_windows->create_Imgui_window_scene_view(
            secondary_viewport_window
        );

        auto rendertarget_node_2 = std::make_shared<erhe::scene::Node>("RT Node 2");
        auto rendertarget_mesh_2 = std::make_shared<Rendertarget_mesh>(
            1920,
            1080,
            2000.0f
        );
        rendertarget_node_2->attach(rendertarget_mesh_2);
        rendertarget_node_2->set_parent(test_scene_root->get_scene().get_root_node());

        rendertarget_node_2->set_world_from_node(
            erhe::math::create_look_at(
                glm::vec3{0.3f, 0.6f, -0.3f},
                glm::vec3{0.0f, 0.7f,  0.0f},
                glm::vec3{0.0f, 1.0f,  0.0f}
            )
        );

        auto imgui_viewport_2 = std::make_shared<editor::Rendertarget_imgui_host>(
            rendertarget_mesh_2.get(),
            "Rendertarget ImGui Viewport 2"
        );

        secondary_Imgui_window_scene_view->set_viewport(imgui_viewport_2.get());
        secondary_Imgui_window_scene_view->show();

        g_rendergraph->connect(
            erhe::rendergraph::Rendergraph_node_key::window,
            secondary_Imgui_window_scene_view,
            imgui_viewport_2
        );
    }
#endif
}

auto Scene_builder::make_camera(std::string_view name, vec3 position, vec3 look_at) -> std::shared_ptr<erhe::scene::Camera>
{
    auto node   = std::make_shared<erhe::scene::Node>(name);
    auto camera = std::make_shared<erhe::scene::Camera>(name);
    camera->projection()->fov_y           = glm::radians(35.0f);
    camera->projection()->projection_type = erhe::scene::Projection::Type::perspective_vertical;
    camera->projection()->z_near          = 0.03f;
    camera->projection()->z_far           = 80.0f;
    camera->enable_flag_bits(Item_flags::content | Item_flags::show_in_ui);
    camera->set_exposure(m_config.camera_exposure);
    node->attach(camera);
    node->set_parent(m_scene_root->get_scene().get_root_node());

    const mat4 m = erhe::math::create_look_at(
        position, // eye
        look_at,  // center
        vec3{0.0f, 1.0f,  0.0f}  // up
    );
    node->set_parent_from_node(m);
    node->enable_flag_bits(Item_flags::content | Item_flags::show_in_ui);

    return camera;
}

void Scene_builder::setup_cameras(
    erhe::graphics::Instance&       graphics_instance,
    erhe::imgui::Imgui_renderer&    imgui_renderer,
    erhe::imgui::Imgui_windows&     imgui_windows,
    erhe::rendergraph::Rendergraph& rendergraph,
    Editor_rendering&               editor_rendering,
    Editor_settings&                editor_settings,
    Post_processing&                post_processing,
    Tools&                          tools,
    Scene_views&                    scene_views
)
{
    const auto& camera_a = make_camera(
        "Camera A",
        vec3{0.0f, 1.0f, 3.0f},
        vec3{0.0f, 0.5f, 0.0f}
    );
    camera_a->projection()->z_far = 64.0f;
    //// camera_a->set_wireframe_color(glm::vec4{1.0f, 0.6f, 0.3f, 1.0f});

#if defined(ERHE_ENABLE_SECOND_CAMERA)
    const auto& camera_b = make_camera(
        "Camera B",
        vec3{-7.0f, 1.0f, 0.0f},
        vec3{ 0.0f, 0.5f, 0.0f}
    );
    camera_b->projection()->z_far = 64.0f;
    //// camera_b->set_wireframe_color(glm::vec4{0.3f, 0.6f, 1.00f, 1.0f});
#endif

    //// TODO Read these from ini
    const bool enable_post_processing = true;
    const bool window_viewport        = true;
    bool imgui_window_scene_view = true;
    {
        const auto& ini = erhe::configuration::get_ini_file_section("erhe.ini", "scene");
        ini.get("imgui_window_scene_view", imgui_window_scene_view);
    }

    if (!imgui_window_scene_view) {
        return;
    }

    const int msaa_sample_count = editor_settings.graphics.current_graphics_preset.msaa_sample_count;
    m_primary_viewport_window = scene_views.create_viewport_scene_view(
        graphics_instance,
        rendergraph,
        editor_rendering,
        editor_settings,
        post_processing,
        tools,
        "Primary Viewport_scene_view",
        m_scene_root,
        camera_a,
        std::max(2, msaa_sample_count), //// TODO Fix rendergraph
        enable_post_processing
    );
    
    if (window_viewport) {
        scene_views.create_imgui_window_scene_view_node(imgui_renderer, imgui_windows, rendergraph, m_primary_viewport_window, "Scene", "");
    } else {
        scene_views.create_basic_viewport_scene_view_node(rendergraph, m_primary_viewport_window, "Scene");
    }
}

auto Scene_builder::make_brush(Content_library_node& folder, Brush_data&& brush_create_info) -> std::shared_ptr<Brush>
{
    return folder.make<Brush>(brush_create_info);
}

auto Scene_builder::make_brush(
    Content_library_node&      folder,
    Editor_settings&           editor_settings,
    Mesh_memory&               mesh_memory,
    erhe::geometry::Geometry&& geometry
) -> std::shared_ptr<Brush>
{
    return make_brush(
        folder,
        Brush_data{
            .context         = m_context,
            .editor_settings = editor_settings,
            .build_info      = build_info(mesh_memory),
            .normal_style    = Normal_style::polygon_normals,
            .geometry        = std::make_shared<erhe::geometry::Geometry>(std::move(geometry)),
            .density         = m_config.mass_scale,
        }
    );
}

auto Scene_builder::build_info(Mesh_memory& mesh_memory) -> erhe::primitive::Build_info
{
    return erhe::primitive::Build_info{
        .primitive_types = {
            .fill_triangles  = true,
            .edge_lines      = true,
            .corner_points   = true,
            .centroid_points = true
        },
        .buffer_info = mesh_memory.buffer_info
    };
}

auto Scene_builder::make_brush(
    Content_library_node&                            folder,
    Editor_settings&                                 editor_settings,
    Mesh_memory&                                     mesh_memory,
    const std::shared_ptr<erhe::geometry::Geometry>& geometry
) -> std::shared_ptr<Brush>
{
    return make_brush(
        folder,
        Brush_data{
            .context         = m_context,
            .editor_settings = editor_settings,
            .build_info      = build_info(mesh_memory),
            .normal_style    = Normal_style::polygon_normals,
            .geometry        = geometry,
            .density         = m_config.mass_scale,
        }
    );
}

void Scene_builder::make_brushes(Editor_settings& editor_settings, Mesh_memory& mesh_memory)
{
    ERHE_PROFILE_FUNCTION();

    std::unique_ptr<ITask_queue> execution_queue;

    const bool parallel_initialization = false; //// TODO
    if (parallel_initialization) {
        const std::size_t thread_count = std::min(
            8U,
            std::max(std::thread::hardware_concurrency() - 0, 1U)
        );
        execution_queue = std::make_unique<Parallel_task_queue>("scene builder", thread_count);
    } else {
        execution_queue = std::make_unique<Serial_task_queue>();
    }

    // Floor
    if (m_config.floor) {
        auto floor_box_shape = erhe::physics::ICollision_shape::create_box_shape_shared(
            0.5f * vec3{m_config.floor_size, 1.0f, m_config.floor_size}
        );

        // Otherwise it will be destructed when leave add_floor() scope
        m_collision_shapes.push_back(floor_box_shape);

        execution_queue->enqueue(
            [this, &floor_box_shape, &editor_settings, &mesh_memory]() {
                ERHE_PROFILE_SCOPE("Floor brush");

                auto floor_geometry = std::make_shared<erhe::geometry::Geometry>(
                    make_box(m_config.floor_size, 1.0f, m_config.floor_size)
                );
                floor_geometry->name = "floor";
                floor_geometry->build_edges();

                m_floor_brush = std::make_unique<Brush>(
                    Brush_data{
                        .context         = m_context,
                        .editor_settings = editor_settings,
                        .build_info      = build_info(mesh_memory),
                        .normal_style    = Normal_style::polygon_normals,
                        .geometry        = floor_geometry,
                        .density         = 0.0f,
                        .volume          = 0.0f,
                        .collision_shape = floor_box_shape,
                    }
                );
            }
        );
    }

    auto content_library = m_scene_root->content_library();
    Content_library_node& brushes = *(content_library->brushes.get());

#pragma region Platonic Solids
    execution_queue->enqueue(
        [this, &editor_settings, &mesh_memory, &brushes]() {
            ERHE_PROFILE_SCOPE("Platonic solids");

            const auto scale = 1.0f; //m_config.object_scale;
            auto platonic_solids = brushes.make_folder("Platonic Solids");
            auto& folder = *platonic_solids.get();
            m_platonic_solids.push_back(make_brush(folder, editor_settings, mesh_memory, make_dodecahedron (scale)));
            m_platonic_solids.push_back(make_brush(folder, editor_settings, mesh_memory, make_icosahedron  (scale)));
            m_platonic_solids.push_back(make_brush(folder, editor_settings, mesh_memory, make_octahedron   (scale)));
            m_platonic_solids.push_back(make_brush(folder, editor_settings, mesh_memory, make_tetrahedron  (scale)));
            m_platonic_solids.push_back(make_brush(folder, editor_settings, mesh_memory, make_cuboctahedron(scale)));
            m_platonic_solids.push_back(make_brush(
                folder,
                Brush_data{
                    .context         = m_context,
                    .editor_settings = editor_settings,
                    .build_info      = build_info(mesh_memory),
                    .normal_style    = Normal_style::polygon_normals,
                    .geometry        = std::make_shared<erhe::geometry::Geometry>(make_cube(scale)),
                    .density         = m_config.mass_scale,
                    .collision_shape = erhe::physics::ICollision_shape::create_box_shape_shared(
                        vec3{scale * 0.5f}
                    )
                }
            ));
        }
    );
#pragma endregion Platonic Solids

#pragma region Sphere
    execution_queue->enqueue(
        [this, &editor_settings, &mesh_memory, &brushes]() {
            ERHE_PROFILE_SCOPE("Sphere");
            m_sphere_brush = make_brush(
                brushes,
                Brush_data{
                    .context         = m_context,
                    .editor_settings = editor_settings,
                    .build_info      = build_info(mesh_memory),
                    .normal_style    = Normal_style::corner_normals,
                    .geometry        = std::make_shared<erhe::geometry::Geometry>(
                        make_sphere(
                            1.0f, //config.object_scale,
                            8 * std::max(1, m_config.detail), // slice count
                            6 * std::max(1, m_config.detail)  // stack count
                        )
                    ),
                    .density         = m_config.mass_scale,
                    .collision_shape = erhe::physics::ICollision_shape::create_sphere_shape_shared(
                        1.0f // config.object_scale
                    )
                }
            );
        }
    );
#pragma endregion Sphere

#pragma region Torus
    execution_queue->enqueue(
        [this, &editor_settings, &mesh_memory, &brushes]() {
            ERHE_PROFILE_SCOPE("Torus");

            const float major_radius = 1.0f ; // * config.object_scale;
            const float minor_radius = 0.25f; // * config.object_scale;

            auto torus_collision_volume_calculator = [=](float scale) -> float {
                return torus_volume(major_radius * scale, minor_radius * scale);
            };

            auto torus_collision_shape_generator = [major_radius, minor_radius](float scale) -> std::shared_ptr<erhe::physics::ICollision_shape> {
                ERHE_PROFILE_SCOPE("torus_collision_shape_generator");

                erhe::physics::Compound_shape_create_info torus_shape_create_info;

                const double     subdivisions        = 16.0;
                const double     scaled_major_radius = major_radius * scale;
                const double     scaled_minor_radius = minor_radius * scale;
                const double     major_circumference = glm::two_pi<double>() * scaled_major_radius;
                const double     capsule_length      = major_circumference / subdivisions;
                const glm::dvec3 forward{0.0, 1.0, 0.0};
                const glm::dvec3 side   {scaled_major_radius, 0.0, 0.0};

                auto capsule = erhe::physics::ICollision_shape::create_capsule_shape_shared(
                    erhe::physics::Axis::Z,
                    static_cast<float>(scaled_minor_radius),
                    static_cast<float>(capsule_length)
                );
                for (int i = 0; i < static_cast<int>(subdivisions); i++) {
                    const double     rel      = static_cast<double>(i) / subdivisions;
                    const double     theta    = rel * glm::two_pi<double>();
                    const glm::dvec3 position = glm::rotate(side, theta, forward);
                    const glm::dquat q        = glm::angleAxis(theta, forward);
                    const glm::dmat3 m        = glm::toMat3(q);

                    torus_shape_create_info.children.emplace_back(
                        erhe::physics::Compound_child{
                            .shape = capsule,
                            .transform = erhe::physics::Transform{
                                mat3{m},
                                vec3{position}
                            }
                        }
                        //capsule,
                        //erhe::physics::Transform{
                        //    mat3{m},
                        //    vec3{position}
                        //}
                    );
                }
                return erhe::physics::ICollision_shape::create_compound_shape_shared(torus_shape_create_info);
            };
            const auto torus_geometry = std::make_shared<erhe::geometry::Geometry>(
                make_torus(
                    major_radius,
                    minor_radius,
                    10 * std::max(1, m_config.detail),
                     8 * std::max(1, m_config.detail)
                )
            );
            m_torus_brush = make_brush(
                brushes,
                Brush_data{
                    .context                     = m_context,
                    .editor_settings             = editor_settings,
                    .build_info                  = build_info(mesh_memory),
                    .normal_style                = Normal_style::corner_normals,
                    .geometry                    = torus_geometry,
                    .density                     = m_config.mass_scale,
                    .collision_volume_calculator = torus_collision_volume_calculator,
                    .collision_shape_generator   = torus_collision_shape_generator,
                }
            );
        }
    );
#pragma endregion Torus

#pragma region Cylinder
    execution_queue->enqueue(
        [this, &editor_settings, &mesh_memory, &brushes]() {
            const float scale = 1.0f; //config.object_scale;
            std::size_t index = 0;
            for (float h = 0.1f; h < 1.1f; h += 0.9f) {
                auto cylinder_geometry = make_cylinder(
                    -h * scale,
                        h * scale,
                        1.0f * scale,
                    true,
                    true,
                    9 * std::max(1, m_config.detail),
                    1 * std::max(1, m_config.detail)
                ); // always axis = x
                cylinder_geometry.transform(erhe::math::mat4_swap_xy);

                m_cylinder_brush[index++] = make_brush(
                    brushes,
                    Brush_data{
                        .context         = m_context,
                        .editor_settings = editor_settings,
                        .build_info      = build_info(mesh_memory),
                        .normal_style    = Normal_style::corner_normals,
                        .geometry        = std::make_shared<erhe::geometry::Geometry>(
                            std::move(cylinder_geometry)
                        ),
                        .density         = m_config.mass_scale,
                        .collision_shape = erhe::physics::ICollision_shape::create_cylinder_shape_shared(
                            erhe::physics::Axis::Y,
                            vec3{h * scale, scale, h * scale}
                        )
                    }
                );
            }
        }
    );
#pragma endregion Cylinder

#pragma region Cone
    execution_queue->enqueue(
        [this, &editor_settings, &mesh_memory, &brushes]() {
            ERHE_PROFILE_SCOPE("Cone");

            auto cone_geometry = make_cone( // always axis = x
                -1.0f, // * config.object_scale,             // min x
                 1.0f, // * config.object_scale,              // max x
                 1.0f, // * config.object_scale,              // bottom radius
                true,                             // use bottm
                10 * std::max(1, m_config.detail),  // slice count
                 5 * std::max(1, m_config.detail)   // stack count
            );
            cone_geometry.transform(erhe::math::mat4_swap_xy); // convert to axis = y

            m_cone_brush = make_brush(
                brushes,
                Brush_data{
                    .context         = m_context,
                    .editor_settings = editor_settings,
                    .build_info      = build_info(mesh_memory),
                    .normal_style    = Normal_style::corner_normals,
                    .geometry        = std::make_shared<erhe::geometry::Geometry>(
                        std::move(cone_geometry)
                    ),
                    .density         = m_config.mass_scale
                    // Sadly, Jolt does not have cone shape
                    //erhe::physics::ICollision_shape::create_cone_shape_shared(
                    //    erhe::physics::Axis::Y,
                    //    object_scale,
                    //    2.0f * object_scale
                    //)
                }
            );
        }
    );
#pragma endregion Cone

    Json_library library;//("res/polyhedra/johnson.json");
    {
        // TODO When tasks can have dependencies we could queue this as well
        ERHE_PROFILE_SCOPE("Johnson solids");

        library = Json_library("res/polyhedra/johnson.json");
        auto& folder = *(brushes.make_folder("Johnson Solids").get());
        for (const auto& key_name : library.names) {
            execution_queue->enqueue(
                [this, &editor_settings, &mesh_memory, &library, &key_name, &folder]() {
                    auto geometry = library.make_geometry(key_name);
                    if (geometry.get_polygon_count() == 0) {
                        return;
                    }
                    geometry.compute_polygon_normals();
                    const auto shared_geometry = std::make_shared<erhe::geometry::Geometry>(std::move(geometry));

                    make_brush(
                        folder,
                        Brush_data{
                            .context            = m_context,
                            .editor_settings    = editor_settings,
                            .name               = shared_geometry->name,
                            .build_info         = build_info(mesh_memory),
                            .normal_style       = Normal_style::polygon_normals,
                            .geometry_generator = [shared_geometry](){ return shared_geometry; },
                            .density            = m_config.mass_scale
                        }
                    );
                }
            );
        }
    }

    execution_queue->wait();

    mesh_memory.gl_buffer_transfer_queue.flush();
}

void Scene_builder::add_room()
{
    ERHE_PROFILE_FUNCTION();

    if (!m_floor_brush) {
        return;
    }

    auto& material_library = m_scene_root->content_library()->materials;
    auto floor_material = material_library->make<erhe::primitive::Material>(
        "Floor",
        //vec4{0.02f, 0.02f, 0.02f, 1.0f}, aces
        vec4{0.07f, 0.07f, 0.07f, 1.0f},
        //vec4{0.01f, 0.01f, 0.01f, 1.0f},
        glm::vec2{0.9f, 0.9f},
        0.01f
    );

    // Notably shadow cast is not enabled for floor
    Instance_create_info floor_brush_instance_create_info{
        .node_flags      = Item_flags::visible | Item_flags::content | Item_flags::show_in_ui | Item_flags::lock_viewport_selection | Item_flags::lock_viewport_transform,
        .mesh_flags      = Item_flags::visible | Item_flags::content | Item_flags::opaque | Item_flags::id | Item_flags::show_in_ui | Item_flags::lock_viewport_selection | Item_flags::lock_viewport_transform,
        .scene_root      = m_scene_root.get(),
        .world_from_node = erhe::math::create_translation<float>(0.0f, -0.51f, 0.0f),
        .material        = floor_material,
        .scale           = 1.0f,
        .motion_mode     = erhe::physics::Motion_mode::e_static
    };

    auto floor_instance_node = m_floor_brush->make_instance(
        floor_brush_instance_create_info
    );

    floor_instance_node->set_parent(m_scene_root->get_scene().get_root_node());
}

void Scene_builder::add_platonic_solids(const Make_mesh_config& config)
{
    make_mesh_nodes(config, m_platonic_solids);
}

void Scene_builder::add_curved_shapes(const Make_mesh_config& config)
{
    std::vector<std::shared_ptr<Brush>> brushes;
    brushes.push_back(m_sphere_brush);
    brushes.push_back(m_cylinder_brush[0]);
    brushes.push_back(m_cylinder_brush[1]);
    brushes.push_back(m_cone_brush);
    brushes.push_back(m_torus_brush);
    make_mesh_nodes(config, brushes);
}

void Scene_builder::add_torus_chain(const Make_mesh_config& config, bool connected)
{
    const float major_radius = 1.0f  * config.object_scale;
    const float minor_radius = 0.25f * config.object_scale;

    auto&       material_library = m_scene_root->content_library()->materials;
    const auto  materials        = material_library->get_all<erhe::primitive::Material>();
    std::size_t material_index   = 0;

    float x = 0.0f;
    float y = major_radius + minor_radius;
    float z = 0.0f;

    float x_stride = connected 
        ? major_radius - 2.0f * minor_radius + major_radius
        : 3 * major_radius;
    glm::mat4 alternate_rotate[2] = {
        glm::mat4{1.0f},
        erhe::math::create_rotation<float>(glm::pi<float>() / 2.0f, glm::vec3{1.0f, 0.0f, 0.0f})
    };

    const std::shared_ptr<Brush>& brush = m_torus_brush;
    for (int i = 0; i < config.instance_count; ++i) {
        const Instance_create_info brush_instance_create_info{
            .node_flags      = Item_flags::show_in_ui | Item_flags::visible | Item_flags::content,
            .mesh_flags      = Item_flags::show_in_ui | Item_flags::visible | Item_flags::opaque | Item_flags::content | Item_flags::id | Item_flags::shadow_cast,
            .scene_root      = m_scene_root.get(),
            .world_from_node = erhe::math::create_translation(x, y, z) * alternate_rotate[connected ? (i & 1) : 0],
            .material        = config.material ? config.material : materials.at(material_index),
            .scale           = config.object_scale
        };
        auto instance_node = brush->make_instance(brush_instance_create_info);
        instance_node->set_name(fmt::format("{}.{}", instance_node->get_name(), i + 1));
        instance_node->set_parent(m_scene_root->get_scene().get_root_node());
        x = x + x_stride;
    }
}

void Scene_builder::make_mesh_nodes(const Make_mesh_config& config, std::vector<std::shared_ptr<Brush>>& brushes)
{
    ERHE_PROFILE_FUNCTION();

#if !defined(NDEBUG)
    m_scene_root->get_scene().sanity_check();
#endif

    class Pack_entry
    {
    public:
        Pack_entry() = default;
        explicit Pack_entry(Brush* brush)
            : brush    {brush}
            , rectangle{0, 0, 0, 0}
        {
        }

        Brush*    brush{nullptr};
        rbp::Rect rectangle;
        int       instance_number{0};
    };

    std::sort(
        brushes.begin(),
        brushes.end(),
        [](const std::shared_ptr<Brush>& lhs, const std::shared_ptr<Brush>& rhs){
            return lhs->get_name() < rhs->get_name();
        }
    );

    std::vector<Pack_entry> pack_entries;

    for (const auto& brush : brushes) {
        for (int i = 0; i < config.instance_count; ++i) {
            const erhe::math::Bounding_box& bounding_box = brush->get_bounding_box();
            ERHE_VERIFY(bounding_box.is_valid());
            pack_entries.emplace_back(brush.get());
            pack_entries.back().instance_number = i;
        }
    }

    constexpr float bottom_y_pos = 0.001f;

    glm::ivec2 max_corner;
    {
        rbp::SkylineBinPack packer;
        const float gap = config.instance_gap;
        int group_width = 2;
        int group_depth = 2;
        for (;;) {
            max_corner = glm::ivec2{0, 0};
            packer.Init(group_width, group_depth, false);

            bool pack_failed = false;
            for (auto& entry : pack_entries) {
                auto*      brush = entry.brush;
                const vec3 size  = brush->get_bounding_box().diagonal();
                const int  width = static_cast<int>(256.0f * (size.x + gap));
                const int  depth = static_cast<int>(256.0f * (size.z + gap));
                entry.rectangle = packer.Insert(width + 1, depth + 1, rbp::SkylineBinPack::LevelBottomLeft);
                if ((entry.rectangle.width == 0) || (entry.rectangle.height == 0)) {
                    pack_failed = true;
                    break;
                }
                max_corner.x = std::max(max_corner.x, entry.rectangle.x + entry.rectangle.width);
                max_corner.y = std::max(max_corner.y, entry.rectangle.y + entry.rectangle.height);
            }

            if (!pack_failed) {
                break;
            }

            if (group_width <= group_depth) {
                group_width *= 2;
            } else {
                group_depth *= 2;
            }

            //ERHE_VERIFY(group_width <= 16384);
        }
    }

    {
        ERHE_PROFILE_SCOPE("make instances");

        auto&       material_library = m_scene_root->content_library()->materials;
        const auto  materials        = material_library->get_all<erhe::primitive::Material>();
        std::size_t material_index   = 0;

        std::size_t visible_material_count = 0;
        for (const auto& material : materials) {
            if (material->is_shown_in_ui()) {
                ++visible_material_count;
            }
        }

        ERHE_VERIFY(!materials.empty());
        ERHE_VERIFY(visible_material_count > 0);

        for (auto& entry : pack_entries) {
            // TODO this will lock up if there are no visible materials
            do {
                material_index = (material_index + 1) % materials.size();
            } while (!materials.at(material_index)->is_shown_in_ui());

            auto* brush = entry.brush;
            float x     = static_cast<float>(entry.rectangle.x) / 256.0f;
            float z     = static_cast<float>(entry.rectangle.y) / 256.0f;
                  x    += 0.5f * static_cast<float>(entry.rectangle.width ) / 256.0f;
                  z    += 0.5f * static_cast<float>(entry.rectangle.height) / 256.0f;
                  x    -= 0.5f * static_cast<float>(max_corner.x) / 256.0f;
                  z    -= 0.5f * static_cast<float>(max_corner.y) / 256.0f;
            float y     = bottom_y_pos - (brush->get_bounding_box().min.y * config.object_scale);
            //x -= 0.5f * static_cast<float>(group_width);
            //z -= 0.5f * static_cast<float>(group_depth);
            //const auto& material = m_scene_root->materials().at(material_index);
            const Instance_create_info brush_instance_create_info
            {
                .node_flags =
                    Item_flags::show_in_ui |
                    Item_flags::visible    |
                    Item_flags::content,
                .mesh_flags =
                    Item_flags::show_in_ui |
                    Item_flags::visible    |
                    Item_flags::opaque     |
                    Item_flags::content    |
                    Item_flags::id         |
                    Item_flags::shadow_cast,
                .scene_root      = m_scene_root.get(),
                .world_from_node = erhe::math::create_translation(x, y, z),
                .material        = materials.at(material_index),
                .scale           = config.object_scale
            };
            auto instance_node = brush->make_instance(brush_instance_create_info);
            if (config.instance_count > 1) {
                instance_node->set_name(fmt::format("{}.{}", instance_node->get_name(), entry.instance_number + 1));
            }
            instance_node->set_parent(m_scene_root->get_scene().get_root_node());

#if !defined(NDEBUG)
            m_scene_root->get_scene().sanity_check();
#endif
        }
    }
}

void Scene_builder::make_cube_benchmark(Mesh_memory& mesh_memory)
{
    ERHE_PROFILE_FUNCTION();

#if !defined(NDEBUG)
    m_scene_root->get_scene().sanity_check();
#endif

    auto& material_library = m_scene_root->content_library()->materials;
    auto material = material_library->make<erhe::primitive::Material>(
        "cube", vec3{1.0, 1.0f, 1.0f}, glm::vec2{0.3f, 0.4f}, 0.0f
    );

    constexpr float scale   = 0.5f;
    constexpr int   x_count = 20;
    constexpr int   y_count = 20;
    constexpr int   z_count = 20;

    erhe::primitive::Element_mappings dummy; // TODO make Element_mappings optional
    const erhe::primitive::Primitive primitive{
        make_buffer_mesh(
            make_cube(0.1f),
            build_info(mesh_memory),
            dummy,
            Normal_style::polygon_normals
        ),
        material
    };
    for (int i = 0; i < x_count; ++i) {
        const float x_rel = static_cast<float>(i) - static_cast<float>(x_count) * 0.5f;
        for (int j = 0; j < y_count; ++j) {
            const float y_rel = static_cast<float>(j);
            for (int k = 0; k < z_count; ++k) {
                const float z_rel = static_cast<float>(k) - static_cast<float>(z_count) * 0.5f;
                const vec3 pos{scale * x_rel, 1.0f + scale * y_rel, scale * z_rel};
                auto node = std::make_shared<erhe::scene::Node>();
                auto mesh = std::make_shared<erhe::scene::Mesh>("", primitive);
                mesh->layer_id = m_scene_root->layers().content()->id;
                mesh->enable_flag_bits(Item_flags::content | Item_flags::shadow_cast | Item_flags::opaque);
                node->attach(mesh);
                node->set_world_from_node(erhe::math::create_translation<float>(pos));
                node->set_parent(m_scene_root->get_scene().get_root_node());
            }
        }
    }

#if !defined(NDEBUG)
    m_scene_root->get_scene().sanity_check();
#endif
}

auto Scene_builder::make_directional_light(
    const std::string_view name,
    const vec3             position,
    const vec3             color,
    const float            intensity
) -> std::shared_ptr<Light>
{
    auto node  = std::make_shared<erhe::scene::Node>(name);
    auto light = std::make_shared<erhe::scene::Light>(name);
    light->type      = Light::Type::directional;
    light->color     = color;
    light->intensity = intensity;
    light->range     = 0.0f;
    light->layer_id  = m_scene_root->layers().light()->id;
    light->enable_flag_bits(Item_flags::content | Item_flags::visible | Item_flags::show_in_ui);
    node->attach          (light);
    node->set_parent      (m_scene_root->get_scene().get_root_node());
    node->enable_flag_bits(Item_flags::content | Item_flags::visible | Item_flags::show_in_ui);

    const mat4 m = erhe::math::create_look_at(
        position,                // eye
        vec3{0.0f, 0.0f, 0.0f},  // center
        vec3{0.0f, 1.0f, 0.0f}   // up
    );
    node->set_parent_from_node(m);

    return light;
}

auto Scene_builder::make_spot_light(
    const std::string_view name,
    const vec3             position,
    const vec3             target,
    const vec3             color,
    const float            intensity,
    const vec2             spot_cone_angle
) -> std::shared_ptr<Light>
{
    auto node  = std::make_shared<erhe::scene::Node>(name);
    auto light = std::make_shared<erhe::scene::Light>(name);
    light->type             = Light::Type::spot;
    light->color            = color;
    light->intensity        = intensity;
    light->range            = 25.0f;
    light->inner_spot_angle = spot_cone_angle[0];
    light->outer_spot_angle = spot_cone_angle[1];
    light->layer_id         = m_scene_root->layers().light()->id;
    light->enable_flag_bits(Item_flags::content | Item_flags::visible | Item_flags::show_in_ui);
    node->attach          (light);
    node->set_parent      (m_scene_root->get_scene().get_root_node());
    node->enable_flag_bits(Item_flags::content | Item_flags::visible | Item_flags::show_in_ui);

    const mat4 m = erhe::math::create_look_at(position, target, vec3{0.0f, 0.0f, 1.0f});
    node->set_parent_from_node(m);

    return light;
}

void Scene_builder::setup_lights()
{
    const auto& layers = m_scene_root->layers();
    layers.light()->ambient_light = vec4{0.042f, 0.044f, 0.049f, 0.0f};

#if 0
    make_directional_light(
        "X",
        vec3{1.0f, 0.0f, 0.0f},
        vec3{1.0f, 0.0f, 0.0f},
        2.0f
    );
    make_directional_light(
        "Y",
        vec3{0.0f, 1.0f, 0.0f},
        vec3{0.0f, 1.0f, 0.0f},
        2.0f
    );
    make_directional_light(
        "Z",
        vec3{0.0f, 0.0f, 1.0f},
        vec3{0.0f, 0.0f, 1.0f},
        2.0f
    );
#endif
    //make_spot_light(
    //    "Spot",
    //    vec3{0.0f, 1.0f, 0.0f}, // position
    //    vec3{0.0f, 0.0f, 0.0f}, // target
    //    vec3{0.0f, 1.0f, 0.0f}, // color
    //    10.0f,                  // intensity
    //    vec2{                   // cone angles
    //        glm::pi<float>() * 0.125f,
    //        glm::pi<float>() * 0.25f
    //    }
    //);

    for (int i = 0; i < m_config.directional_light_count; ++i) {
        const float rel = static_cast<float>(i) / static_cast<float>(m_config.directional_light_count);
        const float R   = m_config.directional_light_radius;
        const float h   = rel * 360.0f;
        const float s   = (m_config.directional_light_count > 1) ? 0.5f : 0.0f;
        const float v   = 1.0f;
        float r, g, b;

        erhe::math::hsv_to_rgb(h, s, v, r, g, b);

        const vec3        color     = vec3{r, g, b};
        const float       intensity = m_config.directional_light_intensity / static_cast<float>(m_config.directional_light_count);
        const std::string name      = fmt::format("Directional light {}", i);
        const float       x_pos     = R * std::sin(rel * glm::two_pi<float>() + 1.0f / 7.0f);
        const float       z_pos     = R * std::cos(rel * glm::two_pi<float>() + 1.0f / 7.0f);
        const vec3        position  = vec3{x_pos, m_config.directional_light_height, z_pos};
        make_directional_light(name, position, color, intensity);
    }

    for (int i = 0; i < m_config.spot_light_count; ++i) {
        const float rel   = static_cast<float>(i) / static_cast<float>(m_config.spot_light_count);
        const float theta = rel * glm::two_pi<float>();
        const float R     = m_config.spot_light_radius;
        const float h     = rel * 360.0f;
        const float s     = (m_config.spot_light_count > 1) ? 0.9f : 0.0f;
        const float v     = 1.0f;
        float r, g, b;

        erhe::math::hsv_to_rgb(h, s, v, r, g, b);

        const vec3        color           = vec3{r, g, b};
        const float       intensity       = m_config.spot_light_intensity;
        const std::string name            = fmt::format("Spot {}", i);
        const float       x_pos           = R * std::sin(theta);
        const float       z_pos           = R * std::cos(theta);
        const vec3        position        = vec3{x_pos, m_config.spot_light_height, z_pos};
        const vec3        target          = vec3{x_pos * 0.1f, 0.0f, z_pos * 0.1f};
        const vec2        spot_cone_angle = vec2{
            glm::pi<float>() / 5.0f,
            glm::pi<float>() / 4.0f
        };
        make_spot_light(name, position, target, color, intensity, spot_cone_angle);
    }
}

void Scene_builder::animate_lights(const double time_d)
{
    //if (time_d >= 0.0) {
    //    return;
    //}
    const float time        = static_cast<float>(time_d);
    const auto& layers      = m_scene_root->layers();
    const auto& light_layer = *layers.light();
    const auto& lights      = light_layer.lights;
    const int   n_lights    = static_cast<int>(lights.size());
    int         light_index = 0;

    for (auto i = lights.begin(); i != lights.end(); ++i) {
        const auto& l = *i;
        if (l->type == Light::Type::directional) {
            continue;
        }

        const float rel = static_cast<float>(light_index) / static_cast<float>(n_lights);
        const float t   = 0.5f * time + rel * glm::pi<float>() * 7.0f;
        const float R   = 4.0f;
        const float r   = 8.0f;

        const auto eye    = vec3{R * std::sin(rel + t * 0.52f), 8.0f, R * std::cos(rel + t * 0.71f)};
        const auto center = vec3{r * std::sin(rel + t * 0.35f), 0.0f, r * std::cos(rel + t * 0.93f)};

        const glm::vec3 up{0.0f, 1.0f, 0.0f};
        const auto m = erhe::math::create_look_at(eye, center, up);

        l->get_node()->set_parent_from_node(m);

        light_index++;
    }
}

auto Scene_builder::get_scene_root() const -> std::shared_ptr<Scene_root>
{
    return m_scene_root;
}

} // namespace editor
