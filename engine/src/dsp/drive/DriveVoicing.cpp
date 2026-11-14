#include "dsp/drive/DriveVoicing.h"

#include <algorithm>
#include <cmath>

namespace ihub::dsp
{

    namespace
    {
        constexpr float overdriveBias = 0.12f;
        constexpr float bluesBias = 0.08f;
        constexpr float grungeBias = 0.10f;
        constexpr float fuzzBias = 0.20f;
    }

    DriveVoicing voicingFor (DriveModel model) noexcept
    {
        switch (model)
        {
            case DriveModel::overdrive:
                return {110.0f, 720.0f, 3.0f, 0.9f, 2600.0f, 0.0f, 0.0f, 30.0f};

            case DriveModel::blues: return {90.0f, 500.0f, 2.0f, 0.8f, 3200.0f, 1.0f, -6.0f, 24.0f};

            case DriveModel::rock: return {100.0f, 900.0f, 2.0f, 0.9f, 3000.0f, 2.5f, 0.0f, 36.0f};

            case DriveModel::distortion:
                return {130.0f, 850.0f, 1.0f, 1.0f, 3200.0f, 3.0f, 6.0f, 42.0f};

            case DriveModel::grunge:
                return {160.0f, 700.0f, -4.0f, 1.0f, 3000.0f, -1.5f, 6.0f, 42.0f};

            case DriveModel::fuzz: return {80.0f, 900.0f, -2.0f, 0.8f, 3000.0f, 1.0f, 6.0f, 45.0f};

            case DriveModel::numModels: break;
        }

        return {};
    }

    float shapeDrive (float input, DriveModel model) noexcept
    {
        switch (model)
        {
            case DriveModel::overdrive:
                return std::tanh (input + overdriveBias) - std::tanh (overdriveBias);

            case DriveModel::blues:
                return std::tanh (input * 0.85f + bluesBias) - std::tanh (bluesBias);

            case DriveModel::rock: return std::tanh (input * 1.25f);

            case DriveModel::distortion: return std::tanh (input * 1.9f);

            case DriveModel::grunge:
                return std::clamp (input * 1.6f + grungeBias, -1.0f, 1.0f) - grungeBias;

            case DriveModel::fuzz:
                return std::tanh (input * 2.2f + fuzzBias) - std::tanh (fuzzBias);

            case DriveModel::numModels: break;
        }

        return input;
    }

}
