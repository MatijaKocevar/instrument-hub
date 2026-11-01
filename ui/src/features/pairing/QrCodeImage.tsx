import { useMemo, type ReactElement } from "react";

import { encodeQr, qrPathData } from "./qr";

export interface QrCodeImageProps
{
    value: string;
    size?: number;
    className?: string;
}

export function QrCodeImage ({ value, size = 220, className }: QrCodeImageProps): ReactElement
{
    const code = useMemo (() => (value.length > 0 ? encodeQr (value) : null), [value]);

    if (code === null)
    {
        return (
            <div
                className={`pairing__qr-placeholder${className === undefined ? "" : ` ${className}`}`}
                data-testid="pairing-qr-placeholder"
                style={{ width: size, height: size }}
            >
                No QR code while LAN mode is off
            </div>
        );
    }

    const quietZone = 4;
    const extent = code.size + quietZone * 2;

    return (
        <svg
            className={className === undefined ? "pairing__qr" : `pairing__qr ${className}`}
            data-testid="pairing-qr"
            data-payload={value}
            data-version={code.version}
            data-mask={code.mask}
            data-size={code.size}
            role="img"
            aria-label={`Pairing QR code for ${value}`}
            width={size}
            height={size}
            viewBox={`${String (-quietZone)} ${String (-quietZone)} ${String (extent)} ${String (extent)}`}
            shapeRendering="crispEdges"
        >
            <rect
                x={-quietZone}
                y={-quietZone}
                width={extent}
                height={extent}
                fill="#ffffff"
            />
            <path d={qrPathData (code)} fill="#101014" />
        </svg>
    );
}
