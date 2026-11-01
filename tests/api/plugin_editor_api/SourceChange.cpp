#include "SourceChange.h"

#include "../RequestDispatch.h"
#include "../TestSupport.h"

#include "plugins/PluginWindow.h"

namespace ApiTest::PluginEditorApi
{

void runSourceChange (ihub::Session& session, httplib::Client& client)
{
    auto response = postWithDispatch (client, "/patches/0/source",
                                      "{\"kind\":\"init-tone\",\"frequency\":330}");
    check (response && response->status == 200, "the patch switches back to a tone");
    check (! session.isEditorOpen (0),
           "the source change closed the editor before the processor retired");
    check (ihub::plugins::PluginWindow::getOpenWindowCount() == 0,
           "no editor window survives the source change");
    check (waitForWarm (session, 0, 5000), "the tone source becomes resident");

    response = postWithDispatch (client, "/patches/0/editor", "{}");
    check (response && response->status == 400,
           "a non-plugin patch can no longer open an editor");

    response = postWithDispatch (client, "/patches/99/editor", "{}");
    check (response && response->status == 404, "an out-of-range slot is rejected");
}

}
