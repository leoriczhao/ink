#include "ink/draw_pass.hpp"

namespace ink {

DrawPass DrawPass::create(const Recording& recording) {
    DrawPass pass;
    const auto& ops = recording.ops();

    if (ops.empty()) return pass;

    std::vector<SortKey> keys;
    keys.reserve(ops.size());

    u16 currentClipId = 0;
    u8 sequence = 0;

    for (u32 i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];

        if (op.type == DrawOp::Type::SetClip) {
            // Advance to new clip group; place SetClip at the very start
            // (sort key with type/color/seq all zero sorts first in group)
            ++currentClipId;
            sequence = 0;
            keys.push_back(SortKey::make(currentClipId, DrawOp::Type::FillRect, {}, 0, i));
            // Bump sequence so draw ops sort after the SetClip
            sequence = 1;
            continue;
        }
        if (op.type == DrawOp::Type::ClearClip) {
            // Place ClearClip at the very end of the current group
            // (sort key with max type/color/seq sorts last in group)
            keys.push_back(SortKey::make(currentClipId,
                static_cast<DrawOp::Type>(0xFF), {0xFF,0xFF,0xFF,0xFF}, 0xFE, i));
            // Advance clip group so subsequent ops don't merge with this one
            ++currentClipId;
            sequence = 0;
            continue;
        }

        keys.push_back(SortKey::make(currentClipId, op.type, op.color, sequence++, i));
        if (sequence >= 0xFE) sequence = 1;  // Reserve 0 for SetClip, 0xFE for ClearClip
    }

    std::stable_sort(keys.begin(), keys.end());

    pass.sortedIndices_.reserve(keys.size());
    for (const auto& key : keys) {
        pass.sortedIndices_.push_back(key.opIndex);
    }

    return pass;
}

}
