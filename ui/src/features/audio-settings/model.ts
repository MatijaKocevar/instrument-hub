import type { ControlAudioChannel, ControlAudioState } from "../../api/client";

/** Latency values smaller than this are shown with two decimals. */
const FINE_MS_THRESHOLD = 10;

/** Format a latency in milliseconds for the readout. */
export function formatMs (milliseconds: number): string
{
    if (!Number.isFinite (milliseconds) || milliseconds <= 0)
        return "0.00";

    const decimals = milliseconds < FINE_MS_THRESHOLD ? 2 : 1;

    return milliseconds.toFixed (decimals);
}

/** Format a sample rate compactly: 48000 -> "48 kHz", 44100 -> "44.1 kHz". */
export function formatSampleRate (rate: number): string
{
    if (!Number.isFinite (rate) || rate <= 0)
        return "—";

    const kilohertz = rate / 1000;
    const text = Number.isInteger (kilohertz)
        ? kilohertz.toFixed (0)
        : kilohertz.toFixed (1);

    return `${text} kHz`;
}

/** Human label for a device entry. */
export function deviceLabel (type: string, name: string): string
{
    return type.length > 0 ? `${name} (${type})` : name;
}

/** Channel indices that are part of the active mapping. */
export function activeChannels (channels: ControlAudioChannel[]): number[]
{
    return channels.filter (channel => channel.active).map (channel => channel.index);
}

/** A new channel list with `index` toggled. Sorted so requests are stable. */
export function toggleChannel (channels: ControlAudioChannel[], index: number): number[]
{
    const active = new Set (activeChannels (channels));

    if (active.has (index))
        active.delete (index);
    else
        active.add (index);

    return [...active].sort ((a, b) => a - b);
}

/** "2 of 2 active" for the channel-mapping heading. */
export function channelSummary (channels: ControlAudioChannel[]): string
{
    const active = activeChannels (channels).length;

    return `${String (active)} of ${String (channels.length)} active`;
}

/**
    Build the buffer-size patch the UI sends.

    Kept pure so the snapshot test can prove the control maps to the same
    request the engine receives.
*/
export function bufferSizePatch (bufferSize: number): { bufferSize: number }
{
    return { bufferSize };
}

/** True when the engine has opened a device. */
export function isOpen (audio: ControlAudioState | null): boolean
{
    return audio?.open === true;
}
