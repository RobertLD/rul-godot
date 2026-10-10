#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <vector>

using namespace godot;

// States are plain ints, so a GDScript enum can be used directly as the tile set.
class WaveFunctionCollapse : public RefCounted {
    GDCLASS(WaveFunctionCollapse, RefCounted);

public:
    enum Direction {
        NORTH = 0,
        EAST = 1,
        SOUTH = 2,
        WEST = 3,
    };

    void allow(int p_state, Direction p_direction, int p_neighbour);
    void allow_all(int p_state, int p_neighbour);
    bool is_allowed(int p_state, Direction p_direction, int p_neighbour) const;

    void set_weight(int p_state, double p_weight);
    double get_weight(int p_state) const;

    void clear();
    PackedInt32Array generate(const Vector2i& p_size, int p_seed) const;

protected:
    static void _bind_methods();

private:
    struct Rule {
        int state;
        int direction;
        int neighbour;
    };

    void grow_to_fit(int p_state);

    std::vector<Rule> rules;
    std::vector<double> weights;
};

VARIANT_ENUM_CAST(WaveFunctionCollapse::Direction);
