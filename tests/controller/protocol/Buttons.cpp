#include "Buttons.h"

#include "../TestSupport.h"

#include "controller/LaunchpadProtocol.h"

namespace ProtocolTest
{

using ControllerTest::check;

void testButtons()
{
    check (Launchpad::topRowCCForIndex (0) == 91
               && Launchpad::topRowCCForIndex (7) == 98,
           "top row CCs are 91..98");
    check (Launchpad::topRowIndexForCC (91) == 0
               && Launchpad::topRowIndexForCC (98) == 7,
           "top row CCs map back to buttons");
    check (! Launchpad::topRowIndexForCC (90) && ! Launchpad::topRowIndexForCC (99),
           "notes and the logo are not top-row buttons");

    check (Launchpad::rightColumnCCForIndex (0) == 89
               && Launchpad::rightColumnCCForIndex (7) == 19,
           "right column CCs are 89..19 top to bottom");
    check (Launchpad::rightColumnIndexForCC (89) == 0
               && Launchpad::rightColumnIndexForCC (19) == 7,
           "right column CCs map back to buttons");
    check (! Launchpad::rightColumnIndexForCC (88) && ! Launchpad::rightColumnIndexForCC (18),
           "non-column CCs are rejected");

    check (Launchpad::logoCC == 99, "the logo LED is CC 99");
}

}
