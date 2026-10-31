// ihub-ui-pages: the T127 page-snapshot acceptance wrapper.
//
// The functional page checks live in `ui/src/testing/snapshot.tsx`, which
// renders the real React tree under `react-dom/server` against a fixture
// control API and writes one snapshot per route. This test runs that harness
// from ctest and validates the produced Grid, Instrument, Plugins and Audio
// snapshots from outside, so `ctest -R 'ihub-e2e|ihub-ui'` covers the pages
// without needing a browser or a display.
//
// Run manually:
//   ./build/tests/ihub-ui-pages-test

#include <juce_core/juce_core.h>

#include <fstream>
#include <iostream>
#include <map>
#include <string>

#ifndef IHUB_UI_DIR
 #define IHUB_UI_DIR "ui"
#endif

#ifndef IHUB_TEST_ARTIFACTS_DIR
 #define IHUB_TEST_ARTIFACTS_DIR "tests/artifacts"
#endif

#ifndef IHUB_UI_PNPM
 #define IHUB_UI_PNPM "corepack"
#endif

#ifndef IHUB_UI_PNPM_USES_COREPACK
 #define IHUB_UI_PNPM_USES_COREPACK 1
#endif

namespace
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string readFile (const std::string& path)
{
    std::ifstream file (path, std::ios::binary);

    if (! file)
        return {};

    return std::string (std::istreambuf_iterator<char> (file),
                        std::istreambuf_iterator<char>());
}

std::size_t countOf (const std::string& text, const std::string& needle)
{
    if (needle.empty())
        return 0;

    std::size_t count = 0;

    for (std::size_t at = text.find (needle); at != std::string::npos;
         at = text.find (needle, at + needle.size()))
        ++count;

    return count;
}

struct PageSnapshot
{
    std::string name;
    std::string file;
    std::string marker;
    std::size_t bytes = 0;
};

} // namespace

int main()
{
    const juce::File uiDirectory = juce::File::getCurrentWorkingDirectory()
                                       .getChildFile (IHUB_UI_DIR)
                                       .getFullPathName();
    const juce::File artifacts = juce::File::getCurrentWorkingDirectory()
                                     .getChildFile (IHUB_TEST_ARTIFACTS_DIR)
                                     .getFullPathName();

    std::cout << "ihub-ui-pages: rendering the routed pages through "
              << uiDirectory.getFullPathName() << std::endl;

    juce::StringArray command;

#if IHUB_UI_PNPM_USES_COREPACK
    command.add (IHUB_UI_PNPM);
    command.add ("pnpm");
#else
    command.add (IHUB_UI_PNPM);
#endif

    command.add ("-C");
    command.add (uiDirectory.getFullPathName());
    command.add ("test:snapshot");

    juce::ChildProcess process;

    check (process.start (command), "the page snapshot harness starts");

    if (! process.isRunning() && process.getExitCode() != 0)
    {
        std::cerr << "ihub-ui-pages: could not start the snapshot harness" << std::endl;
        return 1;
    }

    if (! process.waitForProcessToFinish (180000))
    {
        process.kill();
        check (false, "the page snapshot harness finished within 180 s");
        return 1;
    }

    const auto harnessOutput = process.readAllProcessOutput().toStdString();
    std::cout << harnessOutput;

    check (process.getExitCode() == 0,
           "the page snapshot harness exits 0 (got "
               + std::to_string (static_cast<int> (process.getExitCode())) + ")");
    check (harnessOutput.find ("ui-snapshot: all checks passed") != std::string::npos,
           "the harness reports all page checks passed");

    const PageSnapshot pages[] = {
        { "Grid", "ui-state-snapshot.html", "data-testid=\"instrument-grid\"" },
        { "Instrument", "ui-instrument-snapshot.html", "data-testid=\"instrument-page\"" },
        { "Plugins", "ui-plugins-snapshot.html", "data-testid=\"plugins-page\"" },
        { "Audio", "ui-audio-midi-snapshot.html", "data-testid=\"audio-page\"" },
    };

    int validPages = 0;
    std::map<std::string, std::size_t> sizes;

    for (const auto& page : pages)
    {
        const auto path = artifacts.getChildFile (page.file);
        const auto html = readFile (path.getFullPathName().toStdString());

        check (path.existsAsFile() && ! html.empty(),
               page.name + " snapshot was produced: "
                   + path.getFullPathName().toStdString());

        check (html.find (page.marker) != std::string::npos,
               page.name + " snapshot contains " + page.marker);

        if (! html.empty() && html.find (page.marker) != std::string::npos)
            ++validPages;

        sizes[page.name] = html.size();
    }

    const auto grid = readFile (artifacts.getChildFile ("ui-state-snapshot.html")
                                    .getFullPathName().toStdString());
    const auto audio = readFile (artifacts.getChildFile ("ui-audio-midi-snapshot.html")
                                     .getFullPathName().toStdString());

    check (countOf (grid, "data-testid=\"grid-cell\"") == 64,
           "the Grid snapshot carries all 64 patches");
    check (countOf (grid, "data-fill=\"") >= 64,
           "every Grid cell carries a patch colour");
    check (countOf (grid, "data-fill=\"#ff1f2f\"") == 1,
           "the armed Grid cell is bright red");
    check (grid.find ("data-testid=\"nav-instrument\"") != std::string::npos
               && grid.find ("data-testid=\"nav-plugins\"") != std::string::npos
               && grid.find ("data-testid=\"nav-audio\"") != std::string::npos,
           "the Grid shell links the Instrument, Plugins and Audio pages");
    check (audio.find ("data-testid=\"audio-settings\"") != std::string::npos
               && audio.find ("data-testid=\"midi-panel\"") != std::string::npos,
           "the Audio snapshot carries the settings and MIDI panels");
    check (validPages == 4, "all four page snapshots were produced and recognised");

    {
        std::ofstream record ("ihub-ui-pages-record.txt");
        record << "test=ihub-ui-pages\n";

        for (const auto& [name, size] : sizes)
            record << "page_" << name << "_bytes=" << size << "\n";

        record << "grid_cells=" << countOf (grid, "data-testid=\"grid-cell\"") << "\n"
               << "grid_armed_red=" << (countOf (grid, "data-fill=\"#ff1f2f\"") == 1
                                           ? "true" : "false") << "\n"
               << "artifacts=" << artifacts.getFullPathName() << "\n"
               << "status=" << (failures == 0 ? "PASS" : "FAIL") << "\n";

        check (record.good(),
               "the page snapshot record was written (ihub-ui-pages-record.txt)");
    }

    if (failures > 0)
    {
        std::cerr << "ihub-ui-pages: " << failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "ihub-ui-pages: all checks passed" << std::endl;
    return 0;
}
