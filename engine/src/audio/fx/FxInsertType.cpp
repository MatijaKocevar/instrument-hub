#include "audio/FxRack.h"

namespace AudioEngine
{

    const char* toString (FxInsertType type) noexcept
    {
        switch (type)
        {
            case FxInsertType::equalizer: return "eq";
            case FxInsertType::compressor: return "compressor";
            case FxInsertType::delay: return "delay";
            case FxInsertType::reverb: return "reverb";
            case FxInsertType::drive: return "drive";
            case FxInsertType::tuner: return "tuner";
            case FxInsertType::vst3: return "vst3";
        }

        return "eq";
    }

    bool fxInsertTypeFromString (std::string_view text, FxInsertType& type) noexcept
    {
        if (text == "eq" || text == "equalizer")
        {
            type = FxInsertType::equalizer;
            return true;
        }
        if (text == "compressor" || text == "comp")
        {
            type = FxInsertType::compressor;
            return true;
        }
        if (text == "delay")
        {
            type = FxInsertType::delay;
            return true;
        }
        if (text == "reverb")
        {
            type = FxInsertType::reverb;
            return true;
        }
        if (text == "drive" || text == "distortion" || text == "overdrive")
        {
            type = FxInsertType::drive;
            return true;
        }
        if (text == "tuner")
        {
            type = FxInsertType::tuner;
            return true;
        }
        if (text == "vst3" || text == "plugin")
        {
            type = FxInsertType::vst3;
            return true;
        }

        return false;
    }

}
