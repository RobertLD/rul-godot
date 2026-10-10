#include "wave_function_collapse.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <rul/wfc.hpp>

#include <algorithm>

namespace wfc = rul::algo::wfc;

namespace {
// The core packs states into a 16-bit id.
constexpr int kMaxStates = 65535;
} // namespace

void WaveFunctionCollapse::_bind_methods() {
    ClassDB::bind_method(D_METHOD("allow", "state", "direction", "neighbour"),
                         &WaveFunctionCollapse::allow);
    ClassDB::bind_method(D_METHOD("allow_all", "state", "neighbour"),
                         &WaveFunctionCollapse::allow_all);
    ClassDB::bind_method(D_METHOD("is_allowed", "state", "direction", "neighbour"),
                         &WaveFunctionCollapse::is_allowed);
    ClassDB::bind_method(D_METHOD("set_weight", "state", "weight"),
                         &WaveFunctionCollapse::set_weight);
    ClassDB::bind_method(D_METHOD("get_weight", "state"), &WaveFunctionCollapse::get_weight);
    ClassDB::bind_method(D_METHOD("clear"), &WaveFunctionCollapse::clear);
    ClassDB::bind_method(
        D_METHOD("generate", "size", "seed"), &WaveFunctionCollapse::generate, DEFVAL(0));

    BIND_ENUM_CONSTANT(NORTH);
    BIND_ENUM_CONSTANT(EAST);
    BIND_ENUM_CONSTANT(SOUTH);
    BIND_ENUM_CONSTANT(WEST);
}

void WaveFunctionCollapse::grow_to_fit(int p_state) {
    if (static_cast<int>(weights.size()) <= p_state) {
        weights.resize(static_cast<std::size_t>(p_state) + 1, 1.0);
    }
}

void WaveFunctionCollapse::allow(int p_state, Direction p_direction, int p_neighbour) {
    ERR_FAIL_COND_MSG(p_state < 0 || p_state > kMaxStates, "WaveFunctionCollapse: bad state.");
    ERR_FAIL_COND_MSG(p_neighbour < 0 || p_neighbour > kMaxStates,
                      "WaveFunctionCollapse: bad neighbour state.");

    grow_to_fit(std::max(p_state, p_neighbour));
    rules.push_back(Rule{p_state, static_cast<int>(p_direction), p_neighbour});
}

void WaveFunctionCollapse::allow_all(int p_state, int p_neighbour) {
    for (int d = NORTH; d <= WEST; ++d) {
        allow(p_state, static_cast<Direction>(d), p_neighbour);
    }
}

bool WaveFunctionCollapse::is_allowed(int p_state, Direction p_direction, int p_neighbour) const {
    const int opposite = (static_cast<int>(p_direction) + 2) % 4;
    for (const Rule& rule : rules) {
        // allow() is symmetric: a rule also permits the mirrored pair.
        if (rule.state == p_state && rule.direction == p_direction &&
            rule.neighbour == p_neighbour) {
            return true;
        }
        if (rule.state == p_neighbour && rule.direction == opposite && rule.neighbour == p_state) {
            return true;
        }
    }
    return false;
}

void WaveFunctionCollapse::set_weight(int p_state, double p_weight) {
    ERR_FAIL_COND_MSG(p_state < 0 || p_state > kMaxStates, "WaveFunctionCollapse: bad state.");
    ERR_FAIL_COND_MSG(p_weight <= 0.0, "WaveFunctionCollapse: weight must be positive.");

    grow_to_fit(p_state);
    weights[static_cast<std::size_t>(p_state)] = p_weight;
}

double WaveFunctionCollapse::get_weight(int p_state) const {
    if (p_state < 0 || static_cast<std::size_t>(p_state) >= weights.size()) {
        return 1.0;
    }
    return weights[static_cast<std::size_t>(p_state)];
}

void WaveFunctionCollapse::clear() {
    rules.clear();
    weights.clear();
}

PackedInt32Array WaveFunctionCollapse::generate(const Vector2i& p_size, int p_seed) const {
    PackedInt32Array out;
    ERR_FAIL_COND_V_MSG(
        p_size.x <= 0 || p_size.y <= 0, out, "WaveFunctionCollapse: size must be positive.");
    ERR_FAIL_COND_V_MSG(weights.empty(), out, "WaveFunctionCollapse: no rules were added.");

    wfc::WaveFunctionCollapse solver(p_seed, p_size.x, p_size.y, static_cast<int>(weights.size()));
    solver.set_weights(weights);
    for (const Rule& rule : rules) {
        solver.allow(rule.state, static_cast<wfc::Direction>(rule.direction), rule.neighbour);
    }

    const auto grid = solver.collapse();
    if (!grid.has_value()) {
        return out; // contradiction; retry with another seed
    }
    const wfc::WfcGrid& cells = *grid;

    out.resize(static_cast<int64_t>(p_size.x) * p_size.y);
    for (int y = 0; y < p_size.y; ++y) {
        for (int x = 0; x < p_size.x; ++x) {
            out.set(static_cast<int64_t>(y) * p_size.x + x,
                    cells(static_cast<std::size_t>(y), static_cast<std::size_t>(x)));
        }
    }
    return out;
}
