const VERSION = 4;
const SIZE = 17 + 4 * VERSION;
const ECC_CODEWORDS = 18;
const NUM_BLOCKS = 2;
const DATA_CODEWORDS_PER_BLOCK = 32;
const DATA_CODEWORDS = NUM_BLOCKS * DATA_CODEWORDS_PER_BLOCK;
const MAX_PAYLOAD_BYTES = 62;

export interface QrCode
{
    version: number;
    size: number;
    mask: number;
    modules: boolean[][];
}

export interface QrOptions
{
    mask?: number;
}


const GF_EXP = new Uint8Array (512);
const GF_LOG = new Uint8Array (256);

{
    let value = 1;

    for (let index = 0; index < 255; index += 1)
    {
        GF_EXP[index] = value;
        GF_LOG[value] = index;
        value <<= 1;

        if ((value & 0x100) !== 0)
            value ^= 0x11d;
    }

    for (let index = 255; index < 512; index += 1)
        GF_EXP[index] = GF_EXP[index - 255]!;
}

function gfMultiply (left: number, right: number): number
{
    if (left === 0 || right === 0)
        return 0;

    return GF_EXP[GF_LOG[left]! + GF_LOG[right]!]!;
}

function reedSolomonGenerator (degree: number): number[]
{
    let polynomial = [1];

    for (let power = 0; power < degree; power += 1)
    {
        const next = new Array<number> (polynomial.length + 1).fill (0);

        for (let index = 0; index < polynomial.length; index += 1)
        {
            next[index] = next[index]! ^ polynomial[index]!;
            next[index + 1] = next[index + 1]! ^ gfMultiply (polynomial[index]!, GF_EXP[power]!);
        }

        polynomial = next;
    }

    return polynomial.slice (1);
}

function reedSolomonRemainder (data: readonly number[], degree: number): number[]
{
    const generator = reedSolomonGenerator (degree);
    const result = new Array<number> (degree).fill (0);

    for (const byte of data)
    {
        const factor = byte ^ (result[0] ?? 0);

        result.copyWithin (0, 1);
        result[degree - 1] = 0;

        for (let index = 0; index < degree; index += 1)
            result[index] = result[index]! ^ gfMultiply (generator[index]!, factor);
    }

    return result;
}


function encodeDataCodewords (bytes: readonly number[]): number[]
{
    const bits: number[] = [];

    const push = (value: number, count: number): void =>
    {
        for (let index = count - 1; index >= 0; index -= 1)
            bits.push ((value >>> index) & 1);
    };

    push (0b0100, 4);
    push (bytes.length, 8);

    for (const byte of bytes)
        push (byte, 8);

    const capacity = DATA_CODEWORDS * 8;

    for (let index = 0; index < 4 && bits.length < capacity; index += 1)
        bits.push (0);

    while (bits.length % 8 !== 0)
        bits.push (0);

    const codewords: number[] = [];

    for (let index = 0; index < bits.length; index += 8)
    {
        let value = 0;

        for (let offset = 0; offset < 8; offset += 1)
            value = (value << 1) | bits[index + offset]!;

        codewords.push (value);
    }

    for (let pad = 0; codewords.length < DATA_CODEWORDS; pad += 1)
        codewords.push (pad % 2 === 0 ? 0xec : 0x11);

    return codewords;
}

function interleave (data: readonly number[]): number[]
{
    const blocks: number[][] = [];
    const eccBlocks: number[][] = [];

    for (let block = 0; block < NUM_BLOCKS; block += 1)
    {
        const start = block * DATA_CODEWORDS_PER_BLOCK;
        const bytes = data.slice (start, start + DATA_CODEWORDS_PER_BLOCK);

        blocks.push (bytes);
        eccBlocks.push (reedSolomonRemainder (bytes, ECC_CODEWORDS));
    }

    const result: number[] = [];

    for (let index = 0; index < DATA_CODEWORDS_PER_BLOCK; index += 1)
        for (const block of blocks)
            result.push (block[index]!);

    for (let index = 0; index < ECC_CODEWORDS; index += 1)
        for (const block of eccBlocks)
            result.push (block[index]!);

    return result;
}


function maskBit (mask: number, x: number, y: number): boolean
{
    switch (mask)
    {
        case 0: return (x + y) % 2 === 0;
        case 1: return y % 2 === 0;
        case 2: return x % 3 === 0;
        case 3: return (x + y) % 3 === 0;
        case 4: return (Math.floor (x / 3) + Math.floor (y / 2)) % 2 === 0;
        case 5: return ((x * y) % 2) + ((x * y) % 3) === 0;
        case 6: return (((x * y) % 2) + ((x * y) % 3)) % 2 === 0;
        case 7: return (((x + y) % 2) + ((x * y) % 3)) % 2 === 0;
        default: throw new Error (`QR mask must be 0-7, got ${String (mask)}`);
    }
}

function buildMatrix (text: string, mask: number): boolean[][]
{
    const bytes = [...new TextEncoder ().encode (text)];

    if (bytes.length > MAX_PAYLOAD_BYTES)
        throw new Error (`QR payload is ${String (bytes.length)} bytes; the pairing code allows ${String (MAX_PAYLOAD_BYTES)}`);

    const codewords = interleave (encodeDataCodewords (bytes));
    const modules = Array.from ({ length: SIZE }, () => new Array<boolean> (SIZE).fill (false));
    const isFunction = Array.from ({ length: SIZE }, () => new Array<boolean> (SIZE).fill (false));

    const setFunction = (x: number, y: number, dark: boolean): void =>
    {
        if (x < 0 || y < 0 || x >= SIZE || y >= SIZE)
            return;

        modules[y]![x] = dark;
        isFunction[y]![x] = true;
    };

    for (const [centreX, centreY] of [[3, 3], [SIZE - 4, 3], [3, SIZE - 4]] as Array<[number, number]>)
    {
        for (let offsetY = -4; offsetY <= 4; offsetY += 1)
        {
            for (let offsetX = -4; offsetX <= 4; offsetX += 1)
            {
                const distance = Math.max (Math.abs (offsetX), Math.abs (offsetY));
                setFunction (centreX + offsetX, centreY + offsetY,
                             distance <= 3 && distance !== 2);
            }
        }
    }

    for (let index = 8; index < SIZE - 8; index += 1)
    {
        setFunction (6, index, index % 2 === 0);
        setFunction (index, 6, index % 2 === 0);
    }

    for (let offsetY = -2; offsetY <= 2; offsetY += 1)
        for (let offsetX = -2; offsetX <= 2; offsetX += 1)
            setFunction (26 + offsetX, 26 + offsetY,
                         Math.max (Math.abs (offsetX), Math.abs (offsetY)) !== 1);

    const formatData = mask;
    let remainder = formatData;

    for (let index = 0; index < 10; index += 1)
        remainder = (remainder << 1) ^ (((remainder >>> 9) & 1) * 0x537);

    const formatBits = ((formatData << 10) | remainder) ^ 0x5412;
    const formatBit = (index: number): boolean => ((formatBits >>> index) & 1) !== 0;

    for (let index = 0; index <= 5; index += 1)
        setFunction (8, index, formatBit (index));

    setFunction (8, 7, formatBit (6));
    setFunction (8, 8, formatBit (7));
    setFunction (7, 8, formatBit (8));

    for (let index = 9; index < 15; index += 1)
        setFunction (14 - index, 8, formatBit (index));

    for (let index = 0; index < 8; index += 1)
        setFunction (SIZE - 1 - index, 8, formatBit (index));

    for (let index = 8; index < 15; index += 1)
        setFunction (8, SIZE - 15 + index, formatBit (index));

    setFunction (8, SIZE - 8, true);

    const dataBits: number[] = [];

    for (const codeword of codewords)
        for (let index = 7; index >= 0; index -= 1)
            dataBits.push ((codeword >>> index) & 1);

    let bitIndex = 0;

    for (let right = SIZE - 1; right >= 1; right -= 2)
    {
        if (right === 6)
            right = 5;

        for (let vertical = 0; vertical < SIZE; vertical += 1)
        {
            for (let offset = 0; offset < 2; offset += 1)
            {
                const x = right - offset;
                const upward = ((right + 1) & 2) === 0;
                const y = upward ? SIZE - 1 - vertical : vertical;

                if (isFunction[y]![x]!)
                    continue;

                let dark = bitIndex < dataBits.length ? dataBits[bitIndex] === 1 : false;
                bitIndex += 1;

                if (maskBit (mask, x, y))
                    dark = !dark;

                modules[y]![x] = dark;
            }
        }
    }

    return modules;
}

function penalty (modules: readonly boolean[][]): number
{
    let score = 0;

    for (let y = 0; y < SIZE; y += 1)
    {
        let run = 1;

        for (let x = 1; x < SIZE; x += 1)
        {
            if (modules[y]![x] === modules[y]![x - 1])
            {
                run += 1;
            }
            else
            {
                if (run >= 5)
                    score += 3 + (run - 5);

                run = 1;
            }
        }

        if (run >= 5)
            score += 3 + (run - 5);
    }

    for (let x = 0; x < SIZE; x += 1)
    {
        let run = 1;

        for (let y = 1; y < SIZE; y += 1)
        {
            if (modules[y]![x] === modules[y - 1]![x])
            {
                run += 1;
            }
            else
            {
                if (run >= 5)
                    score += 3 + (run - 5);

                run = 1;
            }
        }

        if (run >= 5)
            score += 3 + (run - 5);
    }

    for (let y = 0; y < SIZE - 1; y += 1)
    {
        for (let x = 0; x < SIZE - 1; x += 1)
        {
            const colour = modules[y]![x]!;

            if (colour === modules[y]![x + 1]
                && colour === modules[y + 1]![x]
                && colour === modules[y + 1]![x + 1])
            {
                score += 3;
            }
        }
    }

    const forward = [true, false, true, true, true, false, true, false, false, false, false];
    const backward = [false, false, false, false, true, false, true, true, true, false, true];

    const matches = (line: readonly boolean[], start: number, pattern: readonly boolean[]): boolean =>
        pattern.every ((value, index) => line[start + index] === value);

    for (let y = 0; y < SIZE; y += 1)
        for (let x = 0; x <= SIZE - 11; x += 1)
            if (matches (modules[y]!, x, forward) || matches (modules[y]!, x, backward))
                score += 40;

    for (let x = 0; x < SIZE; x += 1)
    {
        const column = modules.map (row => row[x]!);

        for (let y = 0; y <= SIZE - 11; y += 1)
            if (matches (column, y, forward) || matches (column, y, backward))
                score += 40;
    }

    let dark = 0;

    for (const row of modules)
        for (const value of row)
            if (value)
                dark += 1;

    const total = SIZE * SIZE;
    score += Math.floor (Math.abs (dark * 20 - total * 10) / total) * 10;

    return score;
}

export function encodeQr (text: string, options: QrOptions = {}): QrCode
{
    if (text.length === 0)
        throw new Error ("QR payload must not be empty");

    let mask = options.mask ?? -1;

    if (mask < 0)
    {
        let bestScore = Number.POSITIVE_INFINITY;

        for (let candidate = 0; candidate < 8; candidate += 1)
        {
            const score = penalty (buildMatrix (text, candidate));

            if (score < bestScore)
            {
                bestScore = score;
                mask = candidate;
            }
        }
    }

    return {
        version: VERSION,
        size: SIZE,
        mask,
        modules: buildMatrix (text, mask),
    };
}

export function qrPathData (code: QrCode): string
{
    const parts: string[] = [];

    for (let y = 0; y < code.size; y += 1)
    {
        for (let x = 0; x < code.size; x += 1)
        {
            if (code.modules[y]![x]!)
                parts.push (`M${String (x)} ${String (y)}h1v1h-1z`);
        }
    }

    return parts.join ("");
}
