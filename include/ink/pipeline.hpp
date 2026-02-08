#pragma once

#include "ink/types.hpp"
#include "ink/recording.hpp"

namespace ink {

/**
 * Pipeline - Abstract base class for rendering pipeline stages.
 *
 * Each pipeline handles a specific type of draw operations and manages
 * its own vertex accumulation and GPU state. This decouples the rendering
 * backend from specific draw operation types, making it easier to add
 * new operations or backends.
 *
 * Pipelines are expected to:
 * 1. Accumulate draw commands via encode()
 * 2. Submit accumulated work via flush()
 * 3. Reset state via reset() for the next frame
 */
class Pipeline {
public:
    virtual ~Pipeline() = default;

    /**
     * Encode a draw operation into this pipeline's internal buffers.
     * The pipeline should determine if the operation belongs to it
     * and either accumulate it or ignore it.
     */
    virtual void encode(const CompactDrawOp& op, const DrawOpArena& arena) = 0;

    /**
     * Flush all accumulated draw operations to the GPU.
     * Called when switching pipelines or at frame end.
     */
    virtual void flush() = 0;

    /**
     * Reset pipeline state for the next frame.
     * Clears vertex buffers and any cached state.
     */
    virtual void reset() = 0;
};

} // namespace ink
