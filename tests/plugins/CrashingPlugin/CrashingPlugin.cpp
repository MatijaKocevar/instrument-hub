#include <csignal>

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((constructor)) static void crashOnLoad()
{
    std::raise (SIGSEGV);
}
#endif

extern "C" __attribute__ ((visibility ("default"))) void* GetPluginFactory()
{
    return nullptr;
}
