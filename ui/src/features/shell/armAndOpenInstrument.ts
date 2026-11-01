import { instrumentPathForSlot } from "../../state/router";

export function armAndOpenInstrument (
    slot: number,
    arm: (slot: number) => void,
    navigate: (path: string) => void): void
{
    arm (slot);
    navigate (instrumentPathForSlot (slot));
}
