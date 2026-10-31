// Deliberately hostile VST3 binary for the T021 scanner acceptance test.
//
// The out-of-process scanner has to survive a plugin that dies while being
// loaded. This shared library takes its host process down from an ELF
// constructor, i.e. inside dlopen(), before any VST3 entry point is called.
//
// It is built into a fake bundle layout:
//
//     CrashingPlugin.vst3/Contents/<machine>-linux/CrashingPlugin.so
//
// with no moduleinfo.json, so JUCE takes the slow path and actually loads the
// library. Never link this into the application.

#include <csignal>

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((constructor)) static void crashOnLoad()
{
    std::raise (SIGSEGV);
}
#endif

// Present so a curious reader can see the intended VST3ish shape; it is never
// reached because the constructor runs first.
extern "C" __attribute__ ((visibility ("default"))) void* GetPluginFactory()
{
    return nullptr;
}
