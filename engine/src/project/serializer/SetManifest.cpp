#include "project/serializer/SetManifest.h"

#include "project/Migrations.h"
#include "project/serializer/JsonFields.h"
#include "project/serializer/PluginStateBlob.h"
#include "project/serializer/SetPatch.h"

#include <string>
#include <utility>

namespace ihub::project::serializer {

using juce::var;

namespace {

bool parseTransport(const var& value, const juce::String& context,
                    TransportConfig& transport, juce::String& error) {
  if (!value.isObject()) {
    error = context + ": expected an object";
    return false;
  }

  double bpm = transport.bpm;
  double positionPpq = transport.positionPpq;

  if (!readNumber(value, "bpm", context, bpm, error, false) ||
      !readNumber(value, "positionPpq", context, positionPpq, error, false) ||
      !readBool(value, "playing", context, transport.playing, error, false))
    return false;

  transport.bpm = bpm;
  transport.positionPpq = positionPpq;
  return true;
}

}  // namespace

juce::String makeManifest(const SetFile& set) {
  auto root = makeObject();
  auto* object = root.getDynamicObject();

  object->setProperty("schemaVersion", set.schemaVersion);
  object->setProperty("name", juce::String(set.name));
  object->setProperty("notes", juce::String(set.notes));

  auto transport = makeObject();
  transport.getDynamicObject()->setProperty("bpm", set.transport.bpm);
  transport.getDynamicObject()->setProperty("positionPpq",
                                            set.transport.positionPpq);
  transport.getDynamicObject()->setProperty("playing", set.transport.playing);
  object->setProperty("transport", transport);

  juce::Array<var> slots;

  for (const auto& slot : set.slots) slots.add(slotToVar(slot));

  object->setProperty("slots", juce::Array<var>(slots));

  juce::Array<var> pluginStates;

  for (const auto& chunk : set.pluginStates)
    pluginStates.add(pluginStateToVar(chunk));

  object->setProperty("pluginStates", juce::Array<var>(pluginStates));

  auto mapping = makeObject();
  mapping.getDynamicObject()->setProperty(
      "bytes", static_cast<juce::int64>(set.mappingYaml.size()));
  object->setProperty("mapping", mapping);

  return juce::JSON::toString(root, false);
}

bool parseManifest(const juce::MemoryBlock& manifest, SetFile& set,
                   juce::int64& declaredMappingBytes, juce::String& error) {
  if (manifest.getSize() == 0) {
    error = "set.json is empty";
    return false;
  }

  const auto text =
      juce::String::fromUTF8(static_cast<const char*>(manifest.getData()),
                             static_cast<int>(manifest.getSize()));
  auto root = juce::JSON::parse(text);

  if (!root.isObject()) {
    error = "set.json is not a JSON object";
    return false;
  }

  if (!migrateManifest(root, error)) return false;

  set.schemaVersion = currentSetSchemaVersion;

  if (!readString(root, "name", "set", set.name, error)) return false;

  if (!readString(root, "notes", "set", set.notes, error, false)) return false;

  var transport;

  if (!readObject(root, "transport", "set", transport, error, false))
    return false;

  if (transport.isObject() &&
      !parseTransport(transport, "set.transport", set.transport, error))
    return false;

  var mapping;

  if (!readObject(root, "mapping", "set", mapping, error, false)) return false;

  if (mapping.isObject() && !readInteger64(mapping, "bytes", "set.mapping",
                                           declaredMappingBytes, error, false))
    return false;

  const juce::Array<var>* slots = nullptr;

  if (!readArray(root, "slots", "set", slots, error)) return false;

  for (int i = 0; i < slots->size(); ++i) {
    const auto& value = slots->getReference(i);
    const auto context = "set.slots[" + juce::String(i) + "]";

    if (!value.isObject()) {
      error = context + ": expected an object";
      return false;
    }

    SlotConfig slot;

    if (!parseSlot(value, context, slot, error)) return false;

    set.slots.push_back(std::move(slot));
  }

  return true;
}

}  // namespace ihub::project::serializer
