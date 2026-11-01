#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <string>
#include <utility>

namespace Controller::MappingDetail
{

bool decodeScenes (const YamlValue& root, SetMapping& out, MappingError& error)
{
    if (const auto* scenes = root.find ("scenes"))
    {
        if (scenes->kind != YamlKind::sequence)
            return fail (error, *scenes, "scenes must be a sequence");

        for (const auto& sceneValue : scenes->sequence)
        {
            if (sceneValue.kind != YamlKind::mapping)
                return fail (error, sceneValue, "a scene entry must be a mapping");

            if (! checkKeys (sceneValue, { "scene", "name", "family" }, error))
                return false;

            const auto* sceneNumber = sceneValue.find ("scene");
            const auto* name = sceneValue.find ("name");
            const auto* family = sceneValue.find ("family");

            if (sceneNumber == nullptr || name == nullptr || family == nullptr)
                return fail (error, sceneValue, "a scene entry needs scene, name and family");

            SceneSpec scene;

            if (! readInt (*sceneNumber, scene.scene, error, "scene")
                || ! readString (*name, scene.name, error, "scene name")
                || ! readString (*family, scene.family, error, "scene family"))
                return false;

            if (scene.scene < 0 || scene.scene >= SetMapping::maximumScenes)
                return fail (error, *sceneNumber,
                             "scene must be within 0.."
                                 + std::to_string (SetMapping::maximumScenes - 1));

            if (out.sceneBinding (scene.scene) != nullptr)
                return fail (error, *sceneNumber,
                             "duplicate scene binding for scene "
                                 + std::to_string (scene.scene));

            if (out.families.find (scene.family) == out.families.end())
                return fail (error, *family,
                             "scene " + std::to_string (scene.scene)
                                 + " names unknown family '" + scene.family + "'");

            out.scenes[static_cast<std::size_t> (scene.scene)] = std::move (scene);
        }
    }

    return true;
}

}
