#pragma once
#include <rul/matrix.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace rul::algo::wfc {

using StateId = std::uint16_t;

enum class Direction : std::uint8_t { North, East, South, West };

inline constexpr std::array<Direction, 4> kDirections{
    Direction::North, Direction::East, Direction::South, Direction::West};
inline constexpr std::size_t kDirectionCount = kDirections.size();

[[nodiscard]] constexpr Direction opposite(Direction dir) {
    return static_cast<Direction>((static_cast<std::uint8_t>(dir) + 2) % kDirectionCount);
}

struct Cell {
    std::size_t row;
    std::size_t col;
};

// Fixed-capacity bit set, so the hot loops move 64 states at a time.
class StateSet {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    StateSet() = default;

    StateSet(std::size_t size, bool value)
        : m_size(size), m_words((size + kBits - 1) / kBits, value ? ~0ULL : 0ULL) {
        if (value) {
            trim();
        }
    }

    [[nodiscard]] bool test(std::size_t bit) const {
        return ((m_words[bit / kBits] >> (bit % kBits)) & 1ULL) != 0ULL;
    }

    void set(std::size_t bit) { m_words[bit / kBits] |= 1ULL << (bit % kBits); }

    void reset(std::size_t bit) { m_words[bit / kBits] &= ~(1ULL << (bit % kBits)); }

    [[nodiscard]] std::size_t count() const {
        std::size_t total = 0;
        for (std::uint64_t word : m_words) {
            total += static_cast<std::size_t>(std::popcount(word));
        }
        return total;
    }

    [[nodiscard]] std::size_t find_first() const { return scan_from(0); }

    [[nodiscard]] std::size_t find_next(std::size_t bit) const { return scan_from(bit + 1); }

    [[nodiscard]] bool operator==(const StateSet&) const = default;

private:
    static constexpr std::size_t kBits = 64;

    // Padding bits above m_size would corrupt count() and the scans.
    void trim() {
        const std::size_t tail = m_size % kBits;
        if (tail != 0) {
            m_words.back() &= (1ULL << tail) - 1ULL;
        }
    }

    [[nodiscard]] std::size_t scan_from(std::size_t bit) const {
        if (bit >= m_size) {
            return npos;
        }
        std::size_t word_index = bit / kBits;
        std::uint64_t word = m_words[word_index] & (~0ULL << (bit % kBits));
        while (word == 0ULL) {
            if (++word_index == m_words.size()) {
                return npos;
            }
            word = m_words[word_index];
        }
        return word_index * kBits + static_cast<std::size_t>(std::countr_zero(word));
    }

    std::size_t m_size = 0;
    std::vector<std::uint64_t> m_words;
};

// Indexed (direction, state): the states allowed on that side of that state.
using AdjacencyRules = collections::matrix::Matrix<StateSet>;
// Indexed (row, col): the states still possible in that cell.
using PossibleStateGrid = collections::matrix::Matrix<StateSet>;
using WfcGrid = collections::matrix::Matrix<int>;

class WaveFunctionCollapse {
public:
    WaveFunctionCollapse(int seed, int width, int height, int state_count)
        : m_rng(static_cast<std::mt19937::result_type>(seed)),
          m_width(checked_extent(width, "width")),
          m_height(checked_extent(height, "height")),
          m_state_count(checked_state_count(state_count)),
          m_weights(m_state_count, 1.0),
          m_weight_log_weight(m_state_count, 0.0),
          m_grid(m_height, m_width, kUncollapsed),
          m_possible(m_height, m_width, StateSet(m_state_count, true)),
          m_rules(kDirectionCount, m_state_count, StateSet(m_state_count, false)),
          m_remaining(m_height, m_width, static_cast<int>(m_state_count)),
          m_noise(m_height, m_width, 0.0) {
        scatter_noise();
    }

    // Relative likelihood of each state; one positive value per state. Defaults to all 1.0.
    void set_weights(std::vector<double> weights) {
        if (weights.size() != m_state_count) {
            throw std::invalid_argument("wfc: set_weights expects one weight per state");
        }
        for (double weight : weights) {
            if (!(weight > 0.0)) {
                throw std::invalid_argument("wfc: weights must be positive");
            }
        }
        m_weights = std::move(weights);
        m_weight_log_weight.resize(m_weights.size());
        for (std::size_t s = 0; s < m_weights.size(); ++s) {
            m_weight_log_weight[s] = m_weights[s] * std::log(m_weights[s]);
        }
    }

    // Records both directions so the rule set can never become asymmetric.
    void allow(int state, Direction dir, int neighbour_state) {
        require_mutable_rules();
        const std::size_t a = checked_state(state);
        const std::size_t b = checked_state(neighbour_state);
        m_rules(index_of(dir), a).set(b);
        m_rules(index_of(opposite(dir)), b).set(a);
    }

    // Direction-agnostic: the two states may sit next to each other on any side.
    void allow_adjacent(int state, int neighbour_state) {
        for (Direction dir : kDirections) {
            allow(state, dir, neighbour_state);
        }
    }

    [[nodiscard]] bool allows(int state, Direction dir, int neighbour_state) const {
        return m_rules(index_of(dir), checked_state(state)).test(checked_state(neighbour_state));
    }

    // Observe the lowest-entropy cell and propagate, until every cell is settled.
    // Empty when propagation hit a contradiction.
    [[nodiscard]] std::optional<WfcGrid> collapse() {
        if (!ensure_initialized()) {
            return std::nullopt;
        }
        while (const std::optional<Cell> cell = pop_lowest_entropy()) {
            collapse_to(*cell, choose_state(*cell));
            if (!propagate()) {
                return std::nullopt;
            }
        }
        write_settled_cells();
        return m_grid;
    }

private:
    struct Ban {
        Cell cell;
        StateId state;
    };

    struct Candidate {
        double key;
        Cell cell;
        int remaining;
    };

    // Total order, so an identical run always pops cells in an identical sequence.
    struct ByKey {
        bool operator()(const Candidate& a, const Candidate& b) const {
            if (a.key != b.key) {
                return a.key > b.key;
            }
            if (a.cell.row != b.cell.row) {
                return a.cell.row > b.cell.row;
            }
            return a.cell.col > b.cell.col;
        }
    };

    static constexpr int kUncollapsed = -1;
    static constexpr int kMaxStates = std::numeric_limits<StateId>::max();
    static constexpr double kEntropyJitter = 1e-6;

    [[nodiscard]] static constexpr std::size_t index_of(Direction dir) {
        return static_cast<std::size_t>(dir);
    }

    [[nodiscard]] static std::size_t checked_extent(int value, const char* what) {
        if (value <= 0) {
            throw std::invalid_argument(std::string("wfc: ") + what + " must be positive");
        }
        return static_cast<std::size_t>(value);
    }

    [[nodiscard]] static std::size_t checked_state_count(int value) {
        if (value <= 0 || value > kMaxStates) {
            throw std::invalid_argument("wfc: state_count must fit a StateId");
        }
        return static_cast<std::size_t>(value);
    }

    [[nodiscard]] std::size_t checked_state(int state) const {
        if (state < 0 || static_cast<std::size_t>(state) >= m_state_count) {
            throw std::out_of_range("wfc: state out of range");
        }
        return static_cast<std::size_t>(state);
    }

    void require_mutable_rules() const {
        if (m_initialized) {
            throw std::logic_error("wfc: rules cannot change once collapsing has started");
        }
    }

    void scatter_noise() {
        // Tie-break only; entropies closer together than this may be reordered.
        std::uniform_real_distribution<double> jitter(0.0, kEntropyJitter);
        for (double& value : m_noise) {
            value = jitter(m_rng);
        }
    }

    [[nodiscard]] std::size_t compat_index(Cell cell, std::size_t state, Direction dir) const {
        return ((cell.row * m_width + cell.col) * m_state_count + state) * kDirectionCount +
               index_of(dir);
    }

    // The rules are only complete once the caller stops calling allow(), so the
    // supporter counters are built on first use rather than in the constructor.
    bool ensure_initialized() {
        if (m_initialized) {
            return !m_failed;
        }
        m_initialized = true;
        m_compatible.assign(m_height * m_width * m_state_count * kDirectionCount, 0);

        std::array<std::vector<std::uint16_t>, kDirectionCount> supporters;
        for (Direction dir : kDirections) {
            auto& per_state = supporters[index_of(dir)];
            per_state.resize(m_state_count);
            for (std::size_t s = 0; s < m_state_count; ++s) {
                per_state[s] = static_cast<std::uint16_t>(m_rules(index_of(dir), s).count());
            }
        }

        for (std::size_t r = 0; r < m_height; ++r) {
            for (std::size_t c = 0; c < m_width; ++c) {
                const Cell cell{r, c};
                for (Direction dir : kDirections) {
                    const bool off_grid = !neighbour(cell, dir).has_value();
                    for (std::size_t s = 0; s < m_state_count; ++s) {
                        // Off-grid sides can never lose a supporter, so they get a stand-in.
                        const std::uint16_t count = off_grid ? 1 : supporters[index_of(dir)][s];
                        m_compatible[compat_index(cell, s, dir)] = count;
                        if (count == 0) {
                            ban(cell, static_cast<StateId>(s));
                        }
                    }
                }
            }
        }

        if (!propagate()) {
            return false;
        }
        rebuild_heap();
        return true;
    }

    // Shannon entropy over the normalised weights of the states still possible here.
    [[nodiscard]] double entropy(Cell cell) const {
        const StateSet& possible = m_possible(cell.row, cell.col);
        double sum_weight = 0.0;
        double sum_weight_log_weight = 0.0;
        for (auto s = possible.find_first(); s != StateSet::npos; s = possible.find_next(s)) {
            sum_weight += m_weights[s];
            sum_weight_log_weight += m_weight_log_weight[s];
        }
        return std::log(sum_weight) - sum_weight_log_weight / sum_weight;
    }

    void push_entropy(Cell cell) {
        m_heap.push(Candidate{
            entropy(cell) + m_noise(cell.row, cell.col), cell, m_remaining(cell.row, cell.col)});
    }

    void rebuild_heap() {
        m_heap = {};
        for (std::size_t r = 0; r < m_height; ++r) {
            for (std::size_t c = 0; c < m_width; ++c) {
                if (m_remaining(r, c) > 1) {
                    push_entropy(Cell{r, c});
                }
            }
        }
    }

    // Entropy only ever falls, so outdated entries are discarded rather than updated.
    [[nodiscard]] std::optional<Cell> pop_lowest_entropy() {
        while (!m_heap.empty()) {
            const Candidate best = m_heap.top();
            m_heap.pop();
            if (best.remaining > 1 && m_remaining(best.cell.row, best.cell.col) == best.remaining) {
                return best.cell;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] StateId choose_state(Cell cell) {
        const StateSet& possible = m_possible(cell.row, cell.col);

        double total = 0.0;
        for (auto s = possible.find_first(); s != StateSet::npos; s = possible.find_next(s)) {
            total += m_weights[s];
        }

        double roll = std::uniform_real_distribution<double>(0.0, total)(m_rng);
        StateId chosen = 0;
        for (auto s = possible.find_first(); s != StateSet::npos; s = possible.find_next(s)) {
            // Keeping the last candidate also covers rounding leaving roll above zero.
            chosen = static_cast<StateId>(s);
            roll -= m_weights[s];
            if (roll <= 0.0) {
                break;
            }
        }
        return chosen;
    }

    void collapse_to(Cell cell, StateId state) {
        StateSet& possible = m_possible(cell.row, cell.col);
        for (auto s = possible.find_first(); s != StateSet::npos;) {
            const auto next = possible.find_next(s);
            if (s != state) {
                ban(cell, static_cast<StateId>(s));
            }
            s = next;
        }
        m_grid(cell.row, cell.col) = static_cast<int>(state);
    }

    // Idempotent, and the single place a state ever leaves a cell.
    void ban(Cell cell, StateId state) {
        StateSet& possible = m_possible(cell.row, cell.col);
        if (!possible.test(state)) {
            return;
        }
        possible.reset(state);

        int& left = m_remaining(cell.row, cell.col);
        if (--left == 0) {
            m_failed = true;
            return;
        }
        if (left > 1) {
            push_entropy(cell);
        }
        m_bans.push_back(Ban{cell, state});
    }

    // Arc consistency by supporter counting: work scales with the bans, not the grid.
    bool propagate() {
        while (!m_bans.empty() && !m_failed) {
            const Ban removed = m_bans.back();
            m_bans.pop_back();

            for (Direction dir : kDirections) {
                const auto next = neighbour(removed.cell, dir);
                if (!next.has_value()) {
                    continue;
                }
                const Cell target = *next;
                const StateSet& supported = m_rules(index_of(dir), removed.state);
                const Direction back = opposite(dir);
                const StateSet& possible = m_possible(target.row, target.col);

                for (auto s = supported.find_first(); s != StateSet::npos;
                     s = supported.find_next(s)) {
                    if (!possible.test(s)) {
                        continue;
                    }
                    if (--m_compatible[compat_index(target, s, back)] == 0) {
                        ban(target, static_cast<StateId>(s));
                    }
                }
            }
        }
        if (m_failed) {
            m_bans.clear();
            return false;
        }
        return true;
    }

    // Clockwise N/E/S/W; the wave is indexed (row = y, col = x).
    [[nodiscard]] std::optional<Cell> neighbour(Cell cell, Direction dir) const {
        switch (dir) {
        case Direction::North:
            if (cell.row == 0) {
                break;
            }
            return Cell{cell.row - 1, cell.col};
        case Direction::East:
            if (cell.col + 1 >= m_width) {
                break;
            }
            return Cell{cell.row, cell.col + 1};
        case Direction::South:
            if (cell.row + 1 >= m_height) {
                break;
            }
            return Cell{cell.row + 1, cell.col};
        case Direction::West:
            if (cell.col == 0) {
                break;
            }
            return Cell{cell.row, cell.col - 1};
        }
        return std::nullopt;
    }

    // Propagation can settle a cell to one state without it ever being observed.
    void write_settled_cells() {
        for (std::size_t r = 0; r < m_height; ++r) {
            for (std::size_t c = 0; c < m_width; ++c) {
                if (m_grid(r, c) == kUncollapsed) {
                    m_grid(r, c) = static_cast<int>(m_possible(r, c).find_first());
                }
            }
        }
    }

    std::mt19937 m_rng;
    std::size_t m_width;
    std::size_t m_height;
    std::size_t m_state_count;
    std::vector<double> m_weights;
    std::vector<double> m_weight_log_weight;
    WfcGrid m_grid;
    PossibleStateGrid m_possible;
    AdjacencyRules m_rules;
    collections::matrix::Matrix<int> m_remaining;
    collections::matrix::Matrix<double> m_noise;
    // Supporters left for a state in a cell, seen from the neighbour on each side.
    std::vector<std::uint16_t> m_compatible;
    std::vector<Ban> m_bans;
    std::priority_queue<Candidate, std::vector<Candidate>, ByKey> m_heap;
    bool m_initialized = false;
    bool m_failed = false;
};
} // namespace rul::algo::wfc
