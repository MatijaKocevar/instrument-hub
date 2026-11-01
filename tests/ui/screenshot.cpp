#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <cstdlib>
#include <iostream>
#include <set>

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

int countDistinctColours (const juce::Image& image)
{
    if (image.isNull() || image.getWidth() <= 0 || image.getHeight() <= 0)
        return 0;

    std::set<juce::uint32> colours;
    constexpr int steps = 64;

    for (int y = 0; y < steps; ++y)
    {
        const int py = juce::jlimit (0, image.getHeight() - 1,
                                     y * image.getHeight() / steps);

        for (int x = 0; x < steps; ++x)
        {
            const int px = juce::jlimit (0, image.getWidth() - 1,
                                         x * image.getWidth() / steps);
            colours.insert (image.getPixelAt (px, py).getARGB());
        }
    }

    return static_cast<int> (colours.size());
}

juce::File resolve (const juce::String& path, const juce::File& base)
{
    return juce::File::isAbsolutePath (path) ? juce::File (path)
                                             : base.getChildFile (path);
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;

    if (std::getenv ("DISPLAY") == nullptr)
    {
        std::cout << "SKIP ui-snapshot: DISPLAY is not set" << std::endl;
        return 0;
    }

    const auto workingDirectory = juce::File::getCurrentWorkingDirectory();

    const auto enginePath = argc > 1 ? juce::String (argv[1])
                                     : juce::String ("../engine/instrument-hub");
    const auto outputPath = argc > 2 ? juce::String (argv[2])
                                     : juce::String ("ui-snapshot.png");

    const auto engine = resolve (enginePath, workingDirectory);
    const auto output = resolve (outputPath, workingDirectory);

    check (engine.existsAsFile(), "engine binary exists: " + engine.getFullPathName());

    if (! engine.existsAsFile())
        return 1;

    output.deleteFile();

    juce::ChildProcess process;
    const juce::StringArray command { engine.getFullPathName(),
                                      "--snapshot",
                                      output.getFullPathName() };

    check (process.start (command), "engine --snapshot starts");

    if (! process.waitForProcessToFinish (90000))
    {
        process.kill();
        check (false, "engine --snapshot finished within 90 s");
        return 1;
    }

    const auto outputText = process.readAllProcessOutput();
    const auto exitCode = process.getExitCode();

    std::cout << outputText << std::endl;

    check (exitCode == 0, "engine --snapshot exits 0 (got "
                              + juce::String (static_cast<int> (exitCode)) + ")");
    check (output.existsAsFile() && output.getSize() > 0,
           "snapshot file was written: " + output.getFullPathName());

    if (! output.existsAsFile())
        return 1;

    const auto image = juce::ImageFileFormat::loadFrom (output);
    check (! image.isNull(), "snapshot decodes as an image");
    check (image.getWidth() == 1200 && image.getHeight() == 800,
           "snapshot is 1200x800 (got " + juce::String (image.getWidth()) + "x"
               + juce::String (image.getHeight()) + ")");

    const auto colours = countDistinctColours (image);
    check (colours >= 16, "snapshot is non-blank (" + juce::String (colours)
                              + " distinct colours)");

    check (outputText.contains ("snapshot: wrote"), "engine reported the snapshot");
    check (outputText.contains ("colours="), "engine reported pixel content");

   #if JUCE_DEBUG
    check (outputText.contains ("mode=debug-dev-server"),
           "Debug build loads the Vite dev server");
   #else
    check (outputText.contains ("mode=release-embedded"),
           "Release build serves the embedded assets");
    check (outputText.contains ("url=juce://"),
           "Release build loaded the embedded UI from the resource provider");
   #endif

    if (failures > 0)
    {
        std::cerr << "ui-snapshot: " << failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "ui-snapshot: all checks passed" << std::endl;
    return 0;
}
