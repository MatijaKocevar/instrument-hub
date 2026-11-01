#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Api
{

/** One insert as returned by the FX chain API. */
struct FxInsertState
{
    int index = 0;

    /** Stable effect type: `eq`, `compressor`, `delay`, `reverb` or `vst3`. */
    std::string type;

    /** `builtin` or `vst3`. */
    std::string kind;

    /** Built-in id, or a plugin's bundle path. */
    std::string id;
    std::string name;

    bool bypassed = false;

    /** Wet/dry balance, 0 (dry) .. 1 (wet). */
    double wet = 1.0;

    int latencySamples = 0;

    /** Effect-specific values, read-only unless the effect supports writes. */
    std::map<std::string, double> parameters;
};

/** A patch's whole insert chain and latency budget. */
struct FxChainState
{
    int slot = -1;
    int instrumentLatencySamples = 0;
    int fxLatencySamples = 0;
    int latencySamples = 0;
    double latencyMs = 0.0;
    std::vector<FxInsertState> inserts;
};

/** A value to append or replace in a patch's chain. */
struct FxInsertRequest
{
    std::string type;
    std::string path;
    std::string id;
    std::string name;
    bool bypassed = false;
    double wet = 1.0;

    /** -1 appends; otherwise the new insert's 0-based position. */
    int position = -1;

    std::map<std::string, double> parameters;
};

/** A partial update of one insert. */
struct FxUpdateRequest
{
    std::optional<bool> bypassed;
    std::optional<double> wet;
    std::optional<int> position;

    /** When present, the effect's parameters are replaced by these values. */
    std::optional<std::map<std::string, double>> parameters;
};

enum class FxCommandKind
{
    add,
    replace,
    update,
    remove,
    reorder,
    clear
};

/** One mutation of a patch's insert chain. */
struct FxCommand
{
    FxCommandKind kind = FxCommandKind::add;

    /** Target insert for update/remove. */
    int index = -1;

    /** For add: the insert to append. For replace: the whole new chain. */
    std::vector<FxInsertRequest> inserts;

    /** For reorder: `order[0]` becomes the first insert. */
    std::vector<int> order;

    FxUpdateRequest update;
};

using FxChainProvider = std::function<bool (int slot, FxChainState&, std::string&)>;
using FxCommandHandler = std::function<bool (int slot,
                                             const FxCommand&,
                                             FxChainState&,
                                             std::string&)>;

} // namespace Api
