#pragma once

namespace ihub::dsp
{

    enum class DriveModel
    {
        overdrive = 0,
        blues,
        rock,
        distortion,
        grunge,
        fuzz,
        numModels
    };

    struct DriveVoicing
    {
        float highPassHz = 110.0f;
        float midHz = 720.0f;
        float midGainDb = 3.0f;
        float midQ = 0.9f;
        float presenceHz = 2600.0f;
        float presenceGainDb = 0.0f;
        float minimumDriveDb = 0.0f;
        float maximumDriveDb = 30.0f;
    };

    DriveVoicing voicingFor (DriveModel model) noexcept;

    float shapeDrive (float input, DriveModel model) noexcept;

}
