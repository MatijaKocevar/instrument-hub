#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Api
{

    struct FxInsertState
    {
        int index = 0;

        std::string type;

        std::string kind;

        std::string id;
        std::string name;

        bool bypassed = false;

        double wet = 1.0;

        int latencySamples = 0;

        std::map<std::string, double> parameters;
    };

    struct FxChainState
    {
        int slot = -1;
        int instrumentLatencySamples = 0;
        int fxLatencySamples = 0;
        int latencySamples = 0;
        double latencyMs = 0.0;
        std::vector<FxInsertState> inserts;
    };

    struct FxTunerState
    {
        int slot = -1;
        int index = -1;
        bool valid = false;
        double frequencyHz = 0.0;
        int midiNote = -1;
        double cents = 0.0;
        double confidence = 0.0;
        double levelDb = -120.0;
    };

    struct FxInsertRequest
    {
        std::string type;
        std::string path;
        std::string id;
        std::string name;
        bool bypassed = false;
        double wet = 1.0;

        int position = -1;

        std::map<std::string, double> parameters;
    };

    struct FxUpdateRequest
    {
        std::optional<std::string> name;
        std::optional<bool> bypassed;
        std::optional<double> wet;
        std::optional<int> position;

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

    struct FxCommand
    {
        FxCommandKind kind = FxCommandKind::add;

        int index = -1;

        std::vector<FxInsertRequest> inserts;

        std::vector<int> order;

        FxUpdateRequest update;
    };

    using FxChainProvider = std::function<bool (int slot, FxChainState&, std::string&)>;
    using FxTunerProvider = std::function<bool (int slot, int index, FxTunerState&, std::string&)>;
    using FxCommandHandler =
        std::function<bool (int slot, const FxCommand&, FxChainState&, std::string&)>;

}
