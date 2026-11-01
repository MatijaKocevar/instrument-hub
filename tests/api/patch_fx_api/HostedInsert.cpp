#include "HostedInsert.h"

#include "../FxChainJson.h"
#include "../JsonSupport.h"
#include "../RequestDispatch.h"
#include "../TestSupport.h"

#include <iostream>
#include <string>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

namespace ApiTest::PatchFxApi
{

void runHostedInsert (SessionRig& rig)
{
    auto& client = rig.client();

    const std::string pluginPath = IHUB_TESTPLUGIN_VST3;

    if (pluginPath.empty())
    {
        std::cout << "patch-fx-api-test: no TestPlugin path; skipping the VST3 insert"
                  << std::endl;
        return;
    }

    const std::string body = std::string (R"({"type":"vst3","path":")")
                             + pluginPath + R"(","parameters":{"Gain":0.25}})";
    auto response = postWithDispatch (client, "/patches/0/fx", body);
    check (response && response->status == 200,
           "POST a hosted VST3 insert returns 200");

    if (! response || response->status != 200)
        std::cerr << "patch-fx-api-test: VST3 add failed: "
                  << (response ? response->body : std::string ("no response"))
                  << std::endl;

    if (response && response->status == 200)
    {
        const auto chain = parseJson (response->body);
        check (insertCount (chain) == 3, "the VST3 was added to the chain");
        check (insertType (chain, 2) == "vst3", "the VST3 insert is published");
        check (property ((*arrayProperty (chain, "inserts"))[2], "kind").toString()
                   == "vst3",
               "the VST3 insert reports its kind");
    }

    response = deleteWithDispatch (client, "/patches/0/fx/2");
    check (response && response->status == 200, "DELETE the VST3 insert returns 200");
    check (response && insertCount (parseJson (response->body)) == 2,
           "the VST3 insert was removed");
}

} // namespace ApiTest::PatchFxApi
