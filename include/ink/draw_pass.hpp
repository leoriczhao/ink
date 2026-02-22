#pragma once

/**
 * @file draw_pass.hpp
 * @brief Sort key and draw pass for optimizing draw operation execution order.
 */

#include "ink/types.hpp"
#include "ink/recording.hpp"
#include <vector>
#include <algorithm>

namespace ink {

/// @brief 64-bit sort key for DrawOp ordering.
///
/// Layout: [63:48] clip_id, [47:40] op_type, [39:8] color_hash, [7:0] sequence.
struct SortKey {
    u64 key;      ///< The packed 64-bit sort key.
    u32 opIndex;  ///< Index into the Recording's operation list.

    /// @brief Construct a sort key from draw operation attributes.
    /// @param clipId Current clip group identifier.
    /// @param type The draw operation type.
    /// @param c The draw color (used for batching).
    /// @param seq Sequence number for ordering within a batch.
    /// @param idx Index of the operation in the Recording.
    /// @return A new SortKey.
    static SortKey make(u16 clipId, DrawOp::Type type, Color c, u8 seq, u32 idx) {
        u64 k = 0;
        k |= (u64(clipId) << 48);
        k |= (u64(static_cast<u8>(type)) << 40);
        u32 colorHash = (u32(c.r) << 24) | (u32(c.g) << 16) | (u32(c.b) << 8) | u32(c.a);
        k |= (u64(colorHash) << 8);
        k |= u64(seq);
        return {k, idx};
    }

    /// @brief Compare sort keys for ordering.
    bool operator<(const SortKey& o) const { return key < o.key; }
};

/// @brief Sorts recorded draw operations for optimal execution order.
///
/// DrawPass reorders operations by clip group, type, and color to minimize
/// state changes in the rendering backend.
class DrawPass {
public:
    /// @brief Create a DrawPass that sorts the operations in a recording.
    /// @param recording The recording to sort.
    /// @return A new DrawPass with sorted indices.
    static DrawPass create(const Recording& recording);

    /// @brief Get the sorted operation indices.
    /// @return Indices into Recording::ops() in sorted execution order.
    const std::vector<u32>& sortedIndices() const { return sortedIndices_; }
    
private:
    std::vector<u32> sortedIndices_;
};

}
