#include "rul/wfc.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <type_traits>

namespace rul::algo::wfc {
namespace {

// Every state may border every state, so collapsing can never fail.
WaveFunctionCollapse permissive(int seed, int width, int height, int state_count) {
    WaveFunctionCollapse wfc(seed, width, height, state_count);
    for (int a = 0; a < state_count; ++a) {
        for (int b = a; b < state_count; ++b) {
            wfc.allow_adjacent(a, b);
        }
    }
    return wfc;
}

} // namespace

TEST(WaveFunctionCollapseTest, IsNotDefaultConstructible) {
    EXPECT_FALSE(std::is_default_constructible_v<WaveFunctionCollapse>);
}

TEST(WaveFunctionCollapseTest, RejectsAnEmptyGrid) {
    EXPECT_THROW(WaveFunctionCollapse(1, 0, 4, 2), std::invalid_argument);
    EXPECT_THROW(WaveFunctionCollapse(1, 4, 4, 0), std::invalid_argument);
}

TEST(WaveFunctionCollapseTest, AllowIsSymmetric) {
    WaveFunctionCollapse wfc(1, 2, 2, 3);
    wfc.allow(0, Direction::East, 1);

    EXPECT_TRUE(wfc.allows(0, Direction::East, 1));
    EXPECT_TRUE(wfc.allows(1, Direction::West, 0));
    EXPECT_FALSE(wfc.allows(0, Direction::North, 1));
}

TEST(WaveFunctionCollapseTest, AllowAdjacentCoversEveryDirection) {
    WaveFunctionCollapse wfc(1, 2, 2, 3);
    wfc.allow_adjacent(0, 2);

    for (Direction dir : kDirections) {
        EXPECT_TRUE(wfc.allows(0, dir, 2));
        EXPECT_TRUE(wfc.allows(2, dir, 0));
    }
}

TEST(WaveFunctionCollapseTest, OppositeIsInvolutive) {
    for (Direction dir : kDirections) {
        EXPECT_EQ(opposite(opposite(dir)), dir);
        EXPECT_NE(opposite(dir), dir);
    }
}

TEST(WaveFunctionCollapseTest, FillsEveryCellWithAValidState) {
    auto wfc = permissive(42, 8, 5, 4);
    const auto grid = wfc.collapse();

    ASSERT_TRUE(grid.has_value());
    ASSERT_EQ(grid->rows(), 5u);
    ASSERT_EQ(grid->cols(), 8u);

    for (size_t r = 0; r < grid->rows(); ++r) {
        for (size_t c = 0; c < grid->cols(); ++c) {
            EXPECT_GE((*grid)(r, c), 0) << "uncollapsed cell at (" << r << ", " << c << ")";
            EXPECT_LT((*grid)(r, c), 4) << "invalid state at (" << r << ", " << c << ")";
        }
    }
}

TEST(WaveFunctionCollapseTest, NeverPlacesADisallowedPair) {
    // 0 and 2 may not touch, so every solution keeps a 1 between them.
    WaveFunctionCollapse wfc(3, 10, 10, 3);
    wfc.allow_adjacent(0, 0);
    wfc.allow_adjacent(0, 1);
    wfc.allow_adjacent(1, 1);
    wfc.allow_adjacent(1, 2);
    wfc.allow_adjacent(2, 2);

    const auto grid = wfc.collapse();
    ASSERT_TRUE(grid.has_value());

    for (size_t r = 0; r < grid->rows(); ++r) {
        for (size_t c = 0; c < grid->cols(); ++c) {
            if (c + 1 < grid->cols()) {
                EXPECT_NE(std::abs((*grid)(r, c) - (*grid)(r, c + 1)), 2) << "at " << r << "," << c;
            }
            if (r + 1 < grid->rows()) {
                EXPECT_NE(std::abs((*grid)(r, c) - (*grid)(r + 1, c)), 2) << "at " << r << "," << c;
            }
        }
    }
}

// A 1xN strip where neither state may border itself has exactly two solutions.
TEST(WaveFunctionCollapseTest, ForcedAlternationIsFullyDetermined) {
    WaveFunctionCollapse wfc(7, 9, 1, 2);
    wfc.allow_adjacent(0, 1);

    const auto grid = wfc.collapse();
    ASSERT_TRUE(grid.has_value());
    ASSERT_EQ(grid->cols(), 9u);

    for (size_t c = 1; c < grid->cols(); ++c) {
        EXPECT_NE((*grid)(0, c), (*grid)(0, c - 1)) << "at column " << c;
    }
}

TEST(WaveFunctionCollapseTest, SameSeedProducesSameGrid) {
    auto first = permissive(99, 6, 6, 3);
    auto second = permissive(99, 6, 6, 3);

    const auto a = first.collapse();
    const auto b = second.collapse();
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());

    for (size_t r = 0; r < a->rows(); ++r) {
        for (size_t c = 0; c < a->cols(); ++c) {
            EXPECT_EQ((*a)(r, c), (*b)(r, c)) << "at (" << r << ", " << c << ")";
        }
    }
}

TEST(WaveFunctionCollapseTest, HeavilyWeightedStateDominates) {
    auto wfc = permissive(55, 20, 20, 3);
    wfc.set_weights({1.0, 1.0, 1000.0});

    const auto grid = wfc.collapse();
    ASSERT_TRUE(grid.has_value());

    int twos = 0;
    for (int state : *grid) {
        twos += state == 2 ? 1 : 0;
    }
    EXPECT_GT(twos, 300) << "state 2 should cover most of a 400-cell grid";
}

TEST(WaveFunctionCollapseTest, RejectsBadWeights) {
    auto wfc = permissive(1, 4, 4, 3);
    EXPECT_THROW(wfc.set_weights({1.0, 1.0}), std::invalid_argument);
    EXPECT_THROW(wfc.set_weights({1.0, 0.0, 1.0}), std::invalid_argument);
}

TEST(WaveFunctionCollapseTest, ReportsContradictionWhenNothingIsAllowed) {
    // No rules at all: collapsing one cell leaves its neighbours with no legal state.
    WaveFunctionCollapse wfc(5, 3, 3, 2);
    EXPECT_FALSE(wfc.collapse().has_value());
}

TEST(WaveFunctionCollapseTest, RulesCannotChangeOnceCollapsed) {
    auto wfc = permissive(61, 4, 4, 2);
    ASSERT_TRUE(wfc.collapse().has_value());
    EXPECT_THROW(wfc.allow(0, Direction::North, 1), std::logic_error);
}

} // namespace rul::algo::wfc
