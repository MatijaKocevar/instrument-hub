#include "api/routes/RouteHelpers.h"

namespace Api
{

juce::var projectOverviewToVar (const ProjectOverview& overview)
{
    auto root = Routes::makeObject();
    auto* object = root.getDynamicObject();

    object->setProperty ("path", juce::String (overview.current.path));
    object->setProperty ("name", juce::String (overview.current.name));
    object->setProperty ("dirty", overview.current.dirty);
    object->setProperty ("saved", overview.current.saved);
    object->setProperty ("error", juce::String (overview.current.error));

    juce::Array<juce::var> recent;

    for (const auto& entry : overview.recent)
    {
        auto item = Routes::makeObject();
        item.getDynamicObject()->setProperty ("path", juce::String (entry.path));
        item.getDynamicObject()->setProperty ("name", juce::String (entry.name));
        recent.add (item);
    }

    object->setProperty ("recent", juce::var (recent));
    return root;
}

}
