import { instrumentPathForSlot } from "../../state/router";

/**
    Arm a patch and open its Instrument page.

    Exported as a pure function so the click contract (one action both arms and
    routes) is testable without a DOM.
*/
export function armAndOpenInstrument (
    slot: number,
    arm: (slot: number) => void,
    navigate: (path: string) => void): void
{
    arm (slot);
    navigate (instrumentPathForSlot (slot));
}
