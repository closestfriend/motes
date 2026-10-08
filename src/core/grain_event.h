#pragma once
/* One spawned grain, as the UI sees it. Plain struct, no atomics: safe to include from C++. */
typedef struct {
    float pos;   /* source position, 0..1 */
    float span;  /* fraction of the source the grain reads (length * pitch_ratio / frames), clamped to <= 1 */
} GrainEvent;
