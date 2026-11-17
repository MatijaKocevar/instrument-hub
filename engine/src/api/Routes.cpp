#include "Routes.h"

#include "api/UiRoutes.h"
#include "api/routes/RouteHelpers.h"

namespace Api {

void registerRoutes(httplib::Server& server, RouteContext context) {
    Routes::registerStateRoutes(server, context);
    Routes::registerTrackRoutes(server, context);
    Routes::registerProjectRoutes(server, context);
    Routes::registerProjectDialogRoutes(server, context);
    Routes::registerPatchRoutes(server, context);
    Routes::registerFxRoutes(server, context);
    Routes::registerAudioRoutes(server, context);
    Routes::registerMidiRoutes(server, context);
    Routes::registerPluginRoutes(server, context);
    Routes::registerTransportRoutes(server, context);
    Routes::registerPairingRoutes(server, context);
    Routes::registerUiRoutes(server);
}

}  // namespace Api
