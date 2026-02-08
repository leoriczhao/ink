#include <gtest/gtest.h>
#include <ink/ink.hpp>

using namespace ink;

// Helper: run canvas operations, finish recording, return the ops vector.
static std::unique_ptr<Recording> record(std::function<void(Canvas&)> fn) {
    Device device;
    device.beginFrame();
    Canvas canvas(&device);
    fn(canvas);
    device.endFrame();
    return device.finishRecording();
}

// Helper: count ops of a given type in a recording.
static int countOps(const Recording& rec, DrawOp::Type type) {
    int n = 0;
    for (const auto& op : rec.ops()) {
        if (op.type == type) ++n;
    }
    return n;
}

// --- Construction ---

TEST(Canvas, CanBeCreatedWithDevice) {
    Device device;
    Canvas canvas(&device);
    // Should not crash; no assertions beyond construction.
}

// --- clipRect records SetClip ---

TEST(Canvas, ClipRectRecordsSetClip) {
    auto rec = record([](Canvas& c) {
        c.clipRect({10, 10, 50, 50});
    });
    ASSERT_NE(rec, nullptr);
    EXPECT_GE(countOps(*rec, DrawOp::Type::SetClip), 1);
}

// --- save/restore preserves clip state ---

TEST(Canvas, SaveRestorePreservesClipState) {
    auto rec = record([](Canvas& c) {
        c.save();
        c.clipRect({10, 10, 50, 50});
        c.restore();
        // After restore, clip should be cleared (no clip was set before save).
        // restore calls applyClip -> resetClip -> recorder.clearClip
    });
    ASSERT_NE(rec, nullptr);
    // Should have a SetClip (from clipRect) and a ClearClip (from restore)
    EXPECT_GE(countOps(*rec, DrawOp::Type::SetClip), 1);
    EXPECT_GE(countOps(*rec, DrawOp::Type::ClearClip), 1);
}

// --- Nested save/restore ---

TEST(Canvas, NestedSaveRestore) {
    auto rec = record([](Canvas& c) {
        c.save();
        c.clipRect({0, 0, 100, 100});
        c.fillRect({5, 5, 10, 10}, {255, 0, 0, 255});

        c.save();
        c.clipRect({20, 20, 30, 30});
        c.fillRect({25, 25, 5, 5}, {0, 255, 0, 255});
        c.restore();  // back to first clip

        c.fillRect({50, 50, 10, 10}, {0, 0, 255, 255});
        c.restore();  // back to no clip
    });
    ASSERT_NE(rec, nullptr);

    // We should have at least 2 SetClip ops (one per clipRect call, plus
    // restore re-applies the outer clip) and at least 1 ClearClip.
    EXPECT_GE(countOps(*rec, DrawOp::Type::SetClip), 2);
    EXPECT_GE(countOps(*rec, DrawOp::Type::ClearClip), 1);
    EXPECT_EQ(countOps(*rec, DrawOp::Type::FillRect), 3);
}

// --- clipRect intersection ---

TEST(Canvas, ClipRectIntersection) {
    // Two overlapping clips should produce their intersection.
    // First clip: (0,0,100,100), second clip: (50,50,100,100)
    // Intersection: (50,50,50,50)
    auto rec = record([](Canvas& c) {
        c.clipRect({0, 0, 100, 100});
        c.clipRect({50, 50, 100, 100});
    });
    ASSERT_NE(rec, nullptr);

    // Find the last SetClip op and verify its rect is the intersection
    const auto& ops = rec->ops();
    const CompactDrawOp* lastClip = nullptr;
    for (const auto& op : ops) {
        if (op.type == DrawOp::Type::SetClip) {
            lastClip = &op;
        }
    }
    ASSERT_NE(lastClip, nullptr);
    EXPECT_FLOAT_EQ(lastClip->data.clip.rect.x, 50.0f);
    EXPECT_FLOAT_EQ(lastClip->data.clip.rect.y, 50.0f);
    EXPECT_FLOAT_EQ(lastClip->data.clip.rect.w, 50.0f);
    EXPECT_FLOAT_EQ(lastClip->data.clip.rect.h, 50.0f);
}

// --- clipRect with no overlap produces zero-size clip ---

TEST(Canvas, ClipRectNoOverlapProducesZeroSize) {
    auto rec = record([](Canvas& c) {
        c.clipRect({0, 0, 10, 10});
        c.clipRect({20, 20, 10, 10});
    });
    ASSERT_NE(rec, nullptr);

    const auto& ops = rec->ops();
    const CompactDrawOp* lastClip = nullptr;
    for (const auto& op : ops) {
        if (op.type == DrawOp::Type::SetClip) {
            lastClip = &op;
        }
    }
    ASSERT_NE(lastClip, nullptr);
    EXPECT_FLOAT_EQ(lastClip->data.clip.rect.w, 0.0f);
    EXPECT_FLOAT_EQ(lastClip->data.clip.rect.h, 0.0f);
}

// --- restore without save is a no-op ---

TEST(Canvas, RestoreWithoutSaveIsNoOp) {
    // Should not crash
    auto rec = record([](Canvas& c) {
        c.restore();
        c.fillRect({0, 0, 10, 10}, {255, 0, 0, 255});
    });
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(countOps(*rec, DrawOp::Type::FillRect), 1);
}

// --- drawImage records a DrawImage op ---

TEST(Canvas, DrawImageRecordsDrawImageOp) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(4, 4));
    ASSERT_TRUE(pm.valid());
    auto img = Image::MakeFromPixmap(pm);
    ASSERT_NE(img, nullptr);

    auto rec = record([&](Canvas& c) {
        c.drawImage(img, 10.0f, 20.0f);
    });
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(countOps(*rec, DrawOp::Type::DrawImage), 1);

    // Verify the position stored in the op
    for (const auto& op : rec->ops()) {
        if (op.type == DrawOp::Type::DrawImage) {
            EXPECT_FLOAT_EQ(op.data.image.x, 10.0f);
            EXPECT_FLOAT_EQ(op.data.image.y, 20.0f);
            break;
        }
    }
}
