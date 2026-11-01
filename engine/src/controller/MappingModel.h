#pragma once

#include "PadState.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Controller
{

//==============================================================================
// Parsed set.yaml model
//==============================================================================

/** One insert effect in a patch's chain, as declared in `set.yaml`.

    `kind` is `builtin` for an engine effect (`eq`, `compressor`, `delay`,
    `reverb`) or `plugin` for a hosted VST3. A plugin entry with an empty
    `pluginState` is a cold descriptor: it names the plugin but carries no
    opaque state until the patch has been loaded or edited.
*/
struct PatchFxSpec
{
    std::string kind = "builtin";
    std::string id;
    std::string name;
    std::string pluginState;
    bool bypassed = false;
    std::map<std::string, float> parameters;

    bool operator== (const PatchFxSpec&) const = default;
};

/** One patch inside a family.

    `slot` is the fixed audio-graph slot that hosts the patch. A pad binding
    resolves to this slot, so changing a pad's instrument in `set.yaml`
    changes the slot the next pad press arms.

    The remaining fields are the patch's persistent state (T119): they are
    parsed from `set.yaml`, carried into the `.ihub` container and restored
    when the set is opened again. A patch that has never been loaded is a
    descriptor only: its `pluginState` is empty and no blob exists for it.
*/
struct InstrumentSpec
{
    std::string id;
    std::string name;
    int slot = -1;

    //==========================================================================
    // Instrument descriptor (present for every patch, hot or cold)

    /** `empty`, `init-tone`, `sampler`, `plugin` or `audio-input`. */
    std::string kind = "empty";

    /** Plugin identity. `pluginState` is the lazy part: accepted empty. */
    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string pluginState;

    /** Kit document for a sampler patch. */
    std::string samplePath;

    //==========================================================================
    // Mixer state

    /** Patch volume in dB, -60 .. +12. */
    float volumeDb = 0.0f;

    /** Stereo balance, -1 (left) .. +1 (right). */
    float pan = 0.0f;

    bool muted = false;
    bool solo = false;

    //==========================================================================
    // Colour

    /** "Use the family default" marker for `colorIndex`. */
    static constexpr int inheritColorIndex = -1;

    /** Patch override, or `inheritColorIndex`. */
    int colorIndex = inheritColorIndex;

    /** Filled by the parser: the override when present, else the family
        default. This is the value the API and the LEDs use. */
    int resolvedColorIndex = 0;

    //==========================================================================
    // Insert chain

    std::vector<PatchFxSpec> fxChain;

    bool operator== (const InstrumentSpec&) const = default;
};

/** A family of instruments (Keys, Drums, Bass, ...). */
struct FamilySpec
{
    std::string id;
    std::string name;

    /** Default colour for every patch in the family (0..15). */
    int colorIndex = 0;

    std::vector<InstrumentSpec> instruments;

    /** The instrument with this id, or nullptr. The pointer stays valid while
        the owning `SetMapping` snapshot is alive. */
    const InstrumentSpec* findInstrument (const std::string& instrumentId) const noexcept;
};

/** A named destination for an encoder.

    The parameter name is opaque to this layer; a later task maps it to a
    plugin parameter. `minimum`/`maximum` scale an encoder's raw 0..127 value.
*/
struct MacroSpec
{
    std::string id;
    std::string name;
    std::string parameter;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float defaultValue = 0.0f;
};

/** A pad -> family/instrument binding, resolved to an engine slot. */
struct PadBinding
{
    int pad = -1;
    std::string family;
    std::string instrument;
    int slot = -1;

    bool operator== (const PadBinding&) const = default;
};

/** An encoder -> macro binding. */
struct EncoderBinding
{
    int encoder = -1;
    std::string macro;

    bool operator== (const EncoderBinding&) const = default;
};

/** One macro page selected by the Launchpad's top row.

    The Launchpad has no encoders, so the eight top-row buttons (CC 91..98)
    select one of these pages instead. A page groups the macros the UI exposes
    while it is selected; the controller itself only reports the selection.
*/
struct MacroBankSpec
{
    int bank = -1;
    std::string name;
    std::vector<std::string> macros;

    bool operator== (const MacroBankSpec&) const = default;
};

/** One scene selected by the Launchpad's right column.

    The right column (CC 89..19, top to bottom) selects a scene. A scene names
    the family whose row of grid pads it refers to; selecting it is a controller
    event, not a graph change.
*/
struct SceneSpec
{
    int scene = -1;
    std::string name;
    std::string family;

    bool operator== (const SceneSpec&) const = default;
};

/** The full document, as parsed from `set.yaml`.

    A snapshot is immutable once published by `Mapping`; readers hold a
    `std::shared_ptr<const SetMapping>` so a hot reload can swap the whole
    document without invalidating anything they are looking at.
*/
struct SetMapping
{
    static constexpr int supportedSchemaVersion = 1;
    static constexpr int maximumSlot = 63;

    /** Patch colour palette indices accepted by `set.yaml`. */
    static constexpr int maximumColourIndex = 15;

    /** The Launchpad grid. `set.yaml` pads are validated against this. */
    static constexpr int maximumPads = 64;

    /** Encoder bindings remain a software concept: the Launchpad has none. */
    static constexpr int maximumEncoders = 8;

    /** Top-row macro pages and right-column scenes. */
    static constexpr int maximumBanks = 8;
    static constexpr int maximumScenes = 8;

    int schemaVersion = supportedSchemaVersion;
    std::string name;

    std::map<std::string, FamilySpec> families;
    std::map<std::string, MacroSpec> macros;

    std::array<std::optional<PadBinding>, maximumPads> pads;
    std::array<std::optional<EncoderBinding>, maximumEncoders> encoders;
    std::array<std::optional<MacroBankSpec>, maximumBanks> banks;
    std::array<std::optional<SceneSpec>, maximumScenes> scenes;

    const PadBinding* padBinding (int pad) const noexcept;
    const EncoderBinding* encoderBinding (int encoder) const noexcept;
    const MacroBankSpec* bankBinding (int bank) const noexcept;
    const SceneSpec* sceneBinding (int scene) const noexcept;

    const InstrumentSpec* instrumentFor (const PadBinding& binding) const noexcept;
    const MacroSpec* macroFor (const EncoderBinding& binding) const noexcept;

    /** Colour index for a patch: its override, else the family default. */
    int colorIndexFor (const PadBinding& binding) const noexcept;

    /** Number of patches declared across every family. */
    int countPatches() const noexcept;

    /** Number of bound pads / encoders / banks / scenes. */
    int countPads() const noexcept;
    int countEncoders() const noexcept;
    int countBanks() const noexcept;
    int countScenes() const noexcept;

    /** Every instrument declared by every family, family by family. */
    std::vector<const InstrumentSpec*> allInstruments() const;
};

} // namespace Controller
