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

                if (!checkKeys (sceneValue, {"scene", "name"}, error))
                    return false;

                const auto* sceneNumber = sceneValue.find ("scene");
                const auto* name = sceneValue.find ("name");

                if (sceneNumber == nullptr || name == nullptr)
                    return fail (error, sceneValue, "a scene entry needs scene and name");

                SceneSpec scene;

                if (!readInt (*sceneNumber, scene.scene, error, "scene") ||
                    !readString (*name, scene.name, error, "scene name"))
                    return false;

                if (scene.scene < 0 || scene.scene >= SetMapping::maximumScenes)
                    return fail (error,
                                 *sceneNumber,
                                 "scene must be within 0.." +
                                     std::to_string (SetMapping::maximumScenes - 1));

                if (out.sceneBinding (scene.scene) != nullptr)
                    return fail (error,
                                 *sceneNumber,
                                 "duplicate scene binding for scene " +
                                     std::to_string (scene.scene));

                out.scenes[static_cast<std::size_t> (scene.scene)] = std::move (scene);
            }
        }

        return true;
    }

}
