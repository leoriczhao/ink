#pragma once

#include "ink/types.hpp"
#include "ink/recording.hpp"
#include <vector>
#include <algorithm>

namespace ink {

// 64-bit sort key for DrawOp ordering
// [63:48] clip_id    [47:40] op_type    [39:8] color_hash    [7:0] sequence
struct SortKey {
    u64 key;
    u32 opIndex;
    
    static SortKey make(u16 clipId, DrawOp::Type type, Color c, u8 seq, u32 idx) {
        u64 k = 0;
        k |= (u64(clipId) << 48);
        k |= (u64(static_cast<u8>(type)) << 40);
        u32 colorHash = (u32(c.r) << 24) | (u32(c.g) << 16) | (u32(c.b) << 8) | u32(c.a);
        k |= (u64(colorHash) << 8);
        k |= u64(seq);
        return {k, idx};
    }
    
    bool operator<(const SortKey& o) const { return key < o.key; }
};

class DrawPass {
public:
    static DrawPass create(const Recording& recording);
    
    const std::vector<u32>& sortedIndices() const { return sortedIndices_; }
    
private:
    std::vector<u32> sortedIndices_;
};

}
