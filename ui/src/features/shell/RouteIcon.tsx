import type { ReactElement } from "react";

import type { ShellRoute } from "../../state/router";

export function RouteIcon ({ route }: { readonly route: ShellRoute }): ReactElement
{
    switch (route)
    {
        case "grid":
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <rect x="3" y="3" width="7" height="7" rx="1.5" />
                    <rect x="14" y="3" width="7" height="7" rx="1.5" />
                    <rect x="3" y="14" width="7" height="7" rx="1.5" />
                    <rect x="14" y="14" width="7" height="7" rx="1.5" />
                </svg>
            );

        case "instrument":
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <path d="M4 6h8M18 6h2M4 12h3M11 12h9M4 18h8M16 18h4" />
                    <circle cx="14" cy="6" r="2" />
                    <circle cx="9" cy="12" r="2" />
                    <circle cx="14" cy="18" r="2" />
                </svg>
            );

        case "plugins":
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <rect x="3" y="3" width="8" height="8" rx="1.5" />
                    <rect x="13" y="13" width="8" height="8" rx="1.5" />
                    <path d="M13 7h4a2 2 0 0 1 2 2v2M7 13v4a2 2 0 0 0 2 2h2" />
                </svg>
            );

        case "audio":
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <path d="M3 12h2l2-5 3 10 3-8 2 5 2-2h4" />
                </svg>
            );

        case "settings":
        case "remote":
        default:
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <circle cx="12" cy="12" r="3" />
                    <path d="M12 3v2.5M12 18.5V21M3 12h2.5M18.5 12H21M5.6 5.6l1.8 1.8M16.6 16.6l1.8 1.8M18.4 5.6l-1.8 1.8M7.4 16.6l-1.8 1.8" />
                </svg>
            );
    }
}
