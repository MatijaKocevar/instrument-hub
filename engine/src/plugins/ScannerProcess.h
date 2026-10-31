#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <vector>

namespace ihub::plugins
{
/** Command-line contract between ScannerProcess and the ihub-scanner helper.

    The helper is intentionally dumb: scan exactly one bundle, write a
    KnownPluginList XML document to the given file, exit 0. Anything else
    (crash, timeout, missing file) is decided by the parent.
*/
namespace scanner_cli
{
inline constexpr const char* helperName  = "ihub-scanner";
inline constexpr const char* scanFlag    = "--scan";
inline constexpr const char* outFlag     = "--out";
inline constexpr const char* formatFlag  = "--format";
inline constexpr const char* defaultFormat = "VST3";
} // namespace scanner_cli

/** Result of one out-of-process scan.

    `succeeded` is only true when the helper exited normally *and* produced a
    parseable KnownPluginList containing at least one plugin. A helper that is
    taken down by a plugin (segfault, abort, timeout) leaves `crashed` or
    `timedOut` set and `error` explains what happened.
*/
struct ScanResult
{
    juce::File file;
    bool succeeded = false;
    bool launched = false;
    bool timedOut = false;
    bool crashed = false;
    int exitCode = 0;
    double elapsedMs = 0.0;
    juce::String error;
    juce::String output;
    std::vector<juce::PluginDescription> plugins;
};

struct ScannerProcessOptions
{
    /** Path to ihub-scanner. Empty means "find it next to the running app". */
    juce::File helperExecutable;

    /** Hard limit for a single plugin. A hanging plugin is killed. */
    int timeoutMs = 20000;

    /** Plugin format to ask the helper about. */
    juce::String formatName { scanner_cli::defaultFormat };
};

/** Launches ihub-scanner in a child process and collects its result.

    The class never loads a plugin binary into the calling process, so a
    crashing plugin can only take down the helper. The caller may use this
    from any thread; each call owns its own ChildProcess and temp files.
*/
class ScannerProcess
{
public:
    explicit ScannerProcess (ScannerProcessOptions options = {});

    ScanResult scanFile (const juce::File& pluginBundle);

    const ScannerProcessOptions& getOptions() const noexcept { return options; }

    /** Locate the helper binary: IHUB_SCANNER overrides, otherwise the
        ihub-scanner sibling of the current executable (with .exe on Windows).
    */
    static juce::File findHelperExecutable();

    /** Path used when no explicit helper has been configured. */
    juce::File getHelperExecutable() const noexcept { return options.helperExecutable; }

private:
    ScannerProcessOptions options;
};
} // namespace ihub::plugins
