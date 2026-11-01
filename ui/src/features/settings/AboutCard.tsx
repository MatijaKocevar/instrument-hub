import type { ReactElement } from "react";

function formatUptime (milliseconds: number | null): string
{
    if (milliseconds === null)
        return "—";

    const totalSeconds = Math.max (0, Math.floor (milliseconds / 1000));
    const minutes = Math.floor (totalSeconds / 60);
    const seconds = totalSeconds % 60;

    if (minutes >= 60)
    {
        const hours = Math.floor (minutes / 60);

        return `${String (hours)} h ${String (minutes % 60)} min`;
    }

    return minutes > 0 ? `${String (minutes)} min ${String (seconds)} s` : `${String (seconds)} s`;
}

interface AboutCardProps
{
    readonly application: { readonly name: string; readonly version: string } | null;
    readonly schemaVersion: number | null;
    readonly uptimeMs: number | null;
}

export function AboutCard ({
    application,
    schemaVersion,
    uptimeMs,
}: AboutCardProps): ReactElement
{
    const version = application?.version ?? "0.1.0";

    return (
        <section
            className="settings__card"
            aria-labelledby="settings-about-title"
            data-testid="settings-about"
        >
            <header className="settings__head">
                <p className="settings__eyebrow">About</p>
                <h3 className="settings__title" id="settings-about-title">
                    Instrument Hub
                </h3>
            </header>

            <dl className="settings__meta">
                <div>
                    <dt>Application</dt>
                    <dd data-testid="settings-app-name">
                        {application?.name ?? "Instrument Hub"}
                    </dd>
                </div>
                <div>
                    <dt>Version</dt>
                    <dd data-testid="settings-version">v{version}</dd>
                </div>
                <div>
                    <dt>State schema</dt>
                    <dd data-testid="settings-schema">
                        {schemaVersion === null ? "—" : schemaVersion}
                    </dd>
                </div>
                <div>
                    <dt>Uptime</dt>
                    <dd data-testid="settings-uptime">{formatUptime (uptimeMs)}</dd>
                </div>
            </dl>

            <p className="settings__hint">
                Audio workspace for instruments, controller and set. The engine
                owns the audio path; this window only controls it.
            </p>
        </section>
    );
}
