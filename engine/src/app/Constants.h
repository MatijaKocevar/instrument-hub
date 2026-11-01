#pragma once

namespace ihub::app
{

/** Size of the app content, and therefore of a `--snapshot` image. */
inline constexpr int kWindowWidth = 1200;
inline constexpr int kWindowHeight = 800;
inline constexpr int kSnapshotWidth = 1200;
inline constexpr int kSnapshotHeight = 800;

/** Bounds of the single-purpose UI modes. */
inline constexpr double kUiCheckTimeoutMs = 20000.0;
inline constexpr double kSnapshotTimeoutMs = 30000.0;
inline constexpr double kSnapshotSettleMs = 400.0;

} // namespace ihub::app
