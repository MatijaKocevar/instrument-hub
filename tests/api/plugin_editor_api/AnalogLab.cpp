#include "AnalogLab.h"

#include "../RequestDispatch.h"
#include "../TestSupport.h"

#include <iostream>

namespace ApiTest::PluginEditorApi
{

void runAnalogLab (ihub::Session& session, httplib::Client& client)
{
    const auto analogLab = findAnalogLabBundle();

    if (! analogLab.exists())
    {
        std::cout << "SKIP the Analog Lab V editor (no bundle installed)" << std::endl;
        return;
    }

    std::cout << "Analog Lab V: " << analogLab.getFullPathName() << std::endl;

    auto response = postWithDispatch (client, "/patches/1/source",
                                      patchSourceJson (analogLab.getFullPathName(),
                                                       "Analog Lab V"));
    check (response && response->status == 200,
           "a yabridge/Arturia bundle is accepted as a source");

    if (waitForWarm (session, 1, 20000))
    {
        response = postWithDispatch (client, "/patches/1/editor", "{}");
        check (response && response->status == 200,
               "Analog Lab V opens its native editor");
        check (session.isEditorOpen (1), "the Analog Lab V window is tracked");

        response = deleteWithDispatch (client, "/patches/1/editor");
        check (response && response->status == 200,
               "the Analog Lab V window closes cleanly");
        check (session.hasPluginState (1),
               "closing the Analog Lab V editor captured its plugin state");
        check (session.getPluginStateSize (1) > 0,
               "the Analog Lab V state blob is non-empty");
    }
    else
    {
        std::cout << "SKIP the Analog Lab V editor (the bundle did not become resident)"
                  << std::endl;
    }
}

}
