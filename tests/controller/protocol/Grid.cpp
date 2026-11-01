#include "Grid.h"

#include "../TestSupport.h"

#include "controller/LaunchpadProtocol.h"

#include <string>

namespace ProtocolTest
{

using ControllerTest::check;

void testGrid()
{
    check (Launchpad::padCount == 64, "the grid has 64 pads");

    for (int index = 0; index < Launchpad::padCount; ++index)
    {
        const int note = Launchpad::padNoteForIndex (index);

        check (note >= Launchpad::padNoteFirst && note <= Launchpad::padNoteLast,
               "pad " + std::to_string (index) + " has a grid note ("
                   + std::to_string (note) + ")");
        check (Launchpad::padIndexForNote (note) == index,
               "note " + std::to_string (note) + " maps back to pad "
                   + std::to_string (index));
    }

    check (Launchpad::padNoteForIndex (0) == 11, "pad 0 is note 11 (bottom-left)");
    check (Launchpad::padNoteForIndex (7) == 18, "pad 7 is note 18 (bottom-right)");
    check (Launchpad::padNoteForIndex (56) == 81, "pad 56 is note 81 (top-left)");
    check (Launchpad::padNoteForIndex (63) == 88, "pad 63 is note 88 (top-right)");

    check (Launchpad::padNoteForIndex (-1) == -1 && Launchpad::padNoteForIndex (64) == -1,
           "out-of-range pad indices have no note");

    for (const int note : { 0, 10, 19, 20, 80, 89, 90, 99, 100, 127 })
        check (Launchpad::padIndexForNote (note) == -1 && ! Launchpad::isGridNote (note),
               "note " + std::to_string (note) + " is not a grid note");

    check (Launchpad::padRowForIndex (0) == 0 && Launchpad::padColumnForIndex (0) == 0,
           "pad 0 is row 0, column 0");
    check (Launchpad::padRowForIndex (63) == 7 && Launchpad::padColumnForIndex (63) == 7,
           "pad 63 is row 7, column 7");
}

}
