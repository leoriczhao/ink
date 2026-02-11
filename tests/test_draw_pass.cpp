#include <gtest/gtest.h>
#include <ink/draw_pass.hpp>

#include <algorithm>
#include <set>
#include <vector>

using namespace ink;

// --- Helper: build a Recording from a lambda that populates a Recorder ---

template <typename Fn>
static std::unique_ptr<Recording> record(Fn fn) {
    Recorder rec;
    fn(rec);
    return rec.finish();
}

// --- Empty recording produces empty DrawPass ---

TEST(DrawPass, EmptyRecording) {
    auto recording = record([](Recorder&) {});
    auto pass = DrawPass::create(*recording);
    EXPECT_TRUE(pass.sortedIndices().empty());
}

// --- Single op produces single sorted index ---

TEST(DrawPass, SingleOp) {
    auto recording = record([](Recorder& r) {
        r.fillRect({0, 0, 10, 10}, {255, 0, 0, 255});
    });
    auto pass = DrawPass::create(*recording);
    ASSERT_EQ(pass.sortedIndices().size(), 1u);
    EXPECT_EQ(pass.sortedIndices()[0], 0u);
}

// --- Ops of same type stay grouped together after sorting ---

TEST(DrawPass, SameTypeGroupedTogether) {
    auto recording = record([](Recorder& r) {
        r.fillRect({0, 0, 1, 1}, {255, 0, 0, 255});
        r.strokeRect({0, 0, 2, 2}, {0, 255, 0, 255}, 1.0f);
        r.fillRect({0, 0, 3, 3}, {0, 0, 255, 255});
    });

    auto pass = DrawPass::create(*recording);
    const auto& idx = pass.sortedIndices();
    ASSERT_EQ(idx.size(), 3u);

    // After sorting by type, the two FillRects (indices 0 and 2) should be
    // adjacent, and the StrokeRect (index 1) should be separate.
    // Find positions of each original op in the sorted order.
    int posOfFill0 = -1, posOfStroke = -1, posOfFill2 = -1;
    for (size_t i = 0; i < idx.size(); ++i) {
        if (idx[i] == 0) posOfFill0 = static_cast<int>(i);
        if (idx[i] == 1) posOfStroke = static_cast<int>(i);
        if (idx[i] == 2) posOfFill2 = static_cast<int>(i);
    }
    ASSERT_NE(posOfFill0, -1);
    ASSERT_NE(posOfStroke, -1);
    ASSERT_NE(posOfFill2, -1);

    // The two FillRects must be adjacent in the sorted output.
    EXPECT_EQ(std::abs(posOfFill0 - posOfFill2), 1);
}

// --- SetClip creates a new clip group ---

TEST(DrawPass, SetClipCreatesNewGroup) {
    auto recording = record([](Recorder& r) {
        r.fillRect({0, 0, 10, 10}, {255, 0, 0, 255});   // index 0, clip group 0
        r.setClip({0, 0, 100, 100});                      // index 1, starts group 1
        r.fillRect({0, 0, 20, 20}, {0, 255, 0, 255});    // index 2, clip group 1
    });

    auto pass = DrawPass::create(*recording);
    const auto& idx = pass.sortedIndices();
    ASSERT_EQ(idx.size(), 3u);

    // Op 0 (pre-clip FillRect) must appear before the SetClip (op 1),
    // and op 1 must appear before op 2 (post-clip FillRect), because
    // clip group 0 sorts before clip group 1.
    int pos0 = -1, pos1 = -1, pos2 = -1;
    for (size_t i = 0; i < idx.size(); ++i) {
        if (idx[i] == 0) pos0 = static_cast<int>(i);
        if (idx[i] == 1) pos1 = static_cast<int>(i);
        if (idx[i] == 2) pos2 = static_cast<int>(i);
    }
    EXPECT_LT(pos0, pos1);
    EXPECT_LT(pos1, pos2);
}

// --- ClearClip is placed at end of its clip group ---

TEST(DrawPass, ClearClipAtEndOfGroup) {
    auto recording = record([](Recorder& r) {
        r.setClip({0, 0, 100, 100});                      // index 0
        r.fillRect({0, 0, 10, 10}, {255, 0, 0, 255});    // index 1
        r.strokeRect({0, 0, 20, 20}, {0, 0, 255, 255}, 1.0f); // index 2
        r.clearClip();                                     // index 3
    });

    auto pass = DrawPass::create(*recording);
    const auto& idx = pass.sortedIndices();
    ASSERT_EQ(idx.size(), 4u);

    // SetClip (index 0) should be first in its group.
    // ClearClip (index 3) should be last among these four ops.
    // The draw ops (indices 1, 2) should be between them.
    int posSetClip = -1, posClearClip = -1;
    int posFill = -1, posStroke = -1;
    for (size_t i = 0; i < idx.size(); ++i) {
        if (idx[i] == 0) posSetClip = static_cast<int>(i);
        if (idx[i] == 1) posFill = static_cast<int>(i);
        if (idx[i] == 2) posStroke = static_cast<int>(i);
        if (idx[i] == 3) posClearClip = static_cast<int>(i);
    }
    EXPECT_LT(posSetClip, posFill);
    EXPECT_LT(posSetClip, posStroke);
    EXPECT_LT(posFill, posClearClip);
    EXPECT_LT(posStroke, posClearClip);
}

// --- Ops of different types within same clip group are sorted by type ---

TEST(DrawPass, DifferentTypesSortedByTypeWithinGroup) {
    // DrawOp::Type enum order: FillRect=0, StrokeRect=1, Line=2, ...
    auto recording = record([](Recorder& r) {
        r.drawLine({0, 0}, {1, 1}, {0, 0, 0, 255}, 1.0f);  // index 0, type Line (2)
        r.fillRect({0, 0, 10, 10}, {0, 0, 0, 255});         // index 1, type FillRect (0)
        r.strokeRect({0, 0, 5, 5}, {0, 0, 0, 255}, 1.0f);   // index 2, type StrokeRect (1)
    });

    auto pass = DrawPass::create(*recording);
    const auto& idx = pass.sortedIndices();
    ASSERT_EQ(idx.size(), 3u);

    // After sorting, FillRect (index 1) should come before StrokeRect (index 2),
    // which should come before Line (index 0).
    int posFill = -1, posStroke = -1, posLine = -1;
    for (size_t i = 0; i < idx.size(); ++i) {
        if (idx[i] == 0) posLine = static_cast<int>(i);
        if (idx[i] == 1) posFill = static_cast<int>(i);
        if (idx[i] == 2) posStroke = static_cast<int>(i);
    }
    EXPECT_LT(posFill, posStroke);
    EXPECT_LT(posStroke, posLine);
}

// --- DrawPass preserves all indices (no ops lost) ---

TEST(DrawPass, PreservesAllIndices) {
    auto recording = record([](Recorder& r) {
        r.fillRect({0, 0, 1, 1}, {10, 0, 0, 255});
        r.strokeRect({0, 0, 2, 2}, {20, 0, 0, 255}, 1.0f);
        r.drawLine({0, 0}, {3, 3}, {30, 0, 0, 255}, 1.0f);
        r.setClip({0, 0, 100, 100});
        r.fillRect({0, 0, 4, 4}, {40, 0, 0, 255});
        r.clearClip();
        r.drawLine({0, 0}, {5, 5}, {50, 0, 0, 255}, 1.0f);
    });

    auto pass = DrawPass::create(*recording);
    const auto& idx = pass.sortedIndices();

    // All 7 ops must be present
    ASSERT_EQ(idx.size(), 7u);

    // Collect all indices into a set and verify completeness
    std::set<u32> seen(idx.begin(), idx.end());
    EXPECT_EQ(seen.size(), 7u);
    for (u32 i = 0; i < 7; ++i) {
        EXPECT_TRUE(seen.count(i)) << "Missing op index " << i;
    }
}
