#pragma once

#include "MappingModel.h"

#include <filesystem>
#include <string>

namespace Controller
{

//==============================================================================
// Loading
//==============================================================================

/** A parse or validation failure. `line` is 1-based; 0 means "no line". */
struct MappingError
{
    int line = 0;
    std::string message;

    bool isEmpty() const noexcept { return message.empty(); }
};

/** Parses the small YAML dialect used by `set.yaml`.

    Supported: block mappings, block sequences, `- key: value` items, quoted
    and plain scalars, the empty flow collections `[]` and `{}`, comments, and
    the document markers `---`/`...`. Not supported (and rejected rather than
    guessed at): anchors, aliases, tags, non-empty flow collections,
    multi-line scalars and tabs in indentation.

    On failure `result` is left untouched and `error` describes the first
    problem found.
*/
bool parseSetMappingYaml (const std::string& yaml, SetMapping& result, MappingError& error);

/** Reads and parses one `set.yaml`. */
bool loadSetMappingFile (const std::filesystem::path& file,
                         SetMapping& result,
                         MappingError& error);

} // namespace Controller
