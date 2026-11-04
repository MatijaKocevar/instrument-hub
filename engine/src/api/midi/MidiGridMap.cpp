#include "api/midi/MidiGridMap.h"

#include <algorithm>

namespace Api
{

int MidiGridMap::learnedCount() const noexcept
{
    return static_cast<int> (learned.size());
}

bool MidiGridMap::hasCompleteLearn() const noexcept
{
    return learned.size() == static_cast<std::size_t> (padCount);
}

bool MidiGridMap::isConfigured() const noexcept
{
    return hasCompleteLearn() || base >= 0;
}

void MidiGridMap::setNoteBase (int noteBase)
{
    base = noteBase >= 0 && noteBase <= highestNoteBase ? noteBase : -1;
    learned.clear();
    learning = false;
}

void MidiGridMap::startLearn()
{
    base = -1;
    learned.clear();
    learning = true;
}

void MidiGridMap::cancelLearn()
{
    if (! hasCompleteLearn())
        learned.clear();

    learning = false;
}

bool MidiGridMap::capture (int note)
{
    if (! learning || note < lowestNote || note > highestNote)
        return false;

    if (std::find (learned.begin(), learned.end(), note) != learned.end())
        return false;

    if (learned.size() >= static_cast<std::size_t> (padCount))
        return false;

    learned.push_back (note);

    if (learned.size() == static_cast<std::size_t> (padCount))
        learning = false;

    return true;
}

std::optional<int> MidiGridMap::padForNote (int note) const noexcept
{
    if (hasCompleteLearn())
    {
        const auto found = std::find (learned.begin(), learned.end(), note);

        if (found != learned.end())
            return static_cast<int> (std::distance (learned.begin(), found));

        return std::nullopt;
    }

    if (base < 0)
        return std::nullopt;

    const int pad = note - base;

    if (pad < 0 || pad >= padCount)
        return std::nullopt;

    return pad;
}

bool MidiGridMap::restore (const std::vector<int>& notes)
{
    if (notes.size() != static_cast<std::size_t> (padCount))
        return false;

    std::vector<int> unique = notes;
    std::sort (unique.begin(), unique.end());

    if (unique.front() < lowestNote || unique.back() > highestNote)
        return false;

    if (std::adjacent_find (unique.begin(), unique.end()) != unique.end())
        return false;

    learned = notes;
    base = -1;
    learning = false;
    return true;
}

}
