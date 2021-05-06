#ifndef geometry_operation_hpp_erhe_geometry_operation
#define geometry_operation_hpp_erhe_geometry_operation

#include "erhe/geometry/geometry.hpp"
#include "erhe/geometry/property_map.hpp"
#include "erhe/geometry/property_map_collection.hpp"
#include <set>
#include <vector>

namespace erhe::geometry::operation
{

class Geometry_operation
{
public:
    explicit Geometry_operation(Geometry& source, Geometry& destination)
        : source{source}
        , destination{destination}
    {
    }

    static constexpr const size_t s_grow_size = 4096;
    Geometry&                                              source;
    Geometry&                                              destination;
    std::vector<Point_id  >                                point_old_to_new;
    std::vector<Polygon_id>                                polygon_old_to_new;
    std::vector<Corner_id >                                corner_old_to_new;
    std::vector<Edge_id   >                                edge_old_to_new;
    std::vector<Point_id  >                                old_polygon_centroid_to_new_points;
    std::vector<std::vector<std::pair<float, Point_id  >>> new_point_sources;
    std::vector<std::vector<std::pair<float, Corner_id >>> new_point_corner_sources;
    std::vector<std::vector<std::pair<float, Corner_id >>> new_corner_sources;
    std::vector<std::vector<std::pair<float, Polygon_id>>> new_polygon_sources;
    std::vector<std::vector<std::pair<float, Edge_id   >>> new_edge_sources;

private:
    static constexpr const size_t s_max_edge_point_slots = 300;
    std::vector<Point_id> m_old_edge_to_new_points;

public:
    void post_processing();

    void make_points_from_points();

    void make_polygon_centroids();

    void reserve_edge_to_new_points();

    auto find_or_make_point_from_edge(Point_id a, Point_id b, size_t count = 1) -> Point_id;

    void make_edge_midpoints(const std::initializer_list<float> relative_positions = { 0.5f } );

    auto get_edge_new_point(Point_id a, Point_id b) const -> Point_id;

    // Creates a new point to Destination from old point.
    // The new point is linked to the old point in Source.
    // Old point is set as source for the new point with specified weight.
    //
    // weight      Weight for old point as source
    // old_point   Old point used as source for the new point
    // return      The new point.
    auto make_new_point_from_point(float weight, Point_id old_point)
    -> Point_id;

    // Creates a new point to Destination from old point.
    // The new point is linked to the old point in Source.
    // Old point is set as source for the new point with weight 1.0.
    //
    // old_point   Old point used as source for the new point
    // returns     The new point.
    auto make_new_point_from_point(Point_id old_point)
    -> Point_id;

    // Creates a new point to Destination from centroid of old polygon.
    // The new point is linked to the old polygon in Source.
    // Each corner of the old polygon is added as source for the new point with weight 1.0.
    auto make_new_point_from_polygon_centroid(Polygon_id old_polygon)
    -> Point_id;

    void add_polygon_centroid(Point_id new_point, float weight, Polygon_id old_polygon);

    void add_point_ring(Point_id new_point, float weight, Point_id old_point);

    auto make_new_polygon_from_polygon(Polygon_id old_polygon)
    -> Polygon_id;

    auto make_new_corner_from_polygon_centroid(Polygon_id new_polygon, Polygon_id old_polygon)
    -> Corner_id;

    auto make_new_corner_from_point(Polygon_id new_polygon, Point_id new_point)
    -> Corner_id;

    auto make_new_corner_from_corner(Polygon_id new_polygon, Corner_id old_corner)
    -> Corner_id;

    void add_polygon_corners(Polygon_id new_polygon, Polygon_id old_polygon);

    void add_point_source(Point_id new_point, float weight, Point_id old_point);

    void add_point_corner_source(Point_id new_point, float weight, Corner_id old_corner);

    void add_corner_source(Corner_id new_corner, float weight, Corner_id old_corner);

    // Inherit point sources to corner
    void distribute_corner_sources(Corner_id new_corner, float weight, Point_id new_point);

    void add_polygon_source(Polygon_id new_polygon, float weight, Polygon_id old_polygon);

    void add_edge_source(Edge_id new_edge, float weight, Edge_id old_edge);

    void build_destination_edges_with_sourcing();

    void interpolate_all_property_maps();
};

} // namespace namespace geometry

#endif
