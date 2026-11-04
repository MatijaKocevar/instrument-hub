#pragma once

#include <optional>
#include <vector>

namespace Api
{

class MidiGridMap
{
public:
    static constexpr int padCount = 64;
    static constexpr int lowestNote = 0;
    static constexpr int highestNote = 127;
    static constexpr int highestNoteBase = highestNote - padCount + 1;

    int noteBase() const noexcept { return base; }
    int learnedCount() const noexcept;
    bool isLearning() const noexcept { return learning; }
    bool hasCompleteLearn() const noexcept;
    bool isConfigured() const noexcept;

    void setNoteBase (int noteBase);
    void startLearn();
    void cancelLearn();

    bool capture (int note);

    std::optional<int> padForNote (int note) const noexcept;

    std::vector<int> learnedNotes() const { return learned; }

    bool restore (const std::vector<int>& notes);

private:
    int base = -1;
    std::vector<int> learned;
    bool learning = false;
};

}
