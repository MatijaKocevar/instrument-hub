import type { ReactElement, ReactNode } from "react";

import type { ControlPatch } from "../../api/client";
import { sourceKindLabel } from "./source";

interface SourceCardProps
{
    readonly patch: ControlPatch;
    readonly children?: ReactNode;
}

/**
    The source card's frame: the source identity and the plug-in metadata.
    The editor, save and picker controls are passed as children.
*/
export function SourceCard ({ patch, children }: SourceCardProps): ReactElement
{
    const instrument = patch.instrument;

    return (
        <section className="instrument__card instrument__source-card"
                 aria-label="Instrument source"
                 data-testid="instrument-source-card">
            <header className="instrument__card-head">
                <h3 className="instrument__card-title">Source</h3>
                <span className="instrument__card-hint">{sourceKindLabel (instrument.kind)}</span>
            </header>

            <p className="instrument__source"
               data-testid="instrument-source"
               data-kind={instrument.kind}
               data-id={instrument.id}
               data-name={instrument.name}>
                <span className="instrument__source-name">{instrument.name}</span>
                <span className="instrument__source-kind">{sourceKindLabel (instrument.kind)}</span>
            </p>

            <dl className="instrument__meta">
                {instrument.pluginFormat !== undefined && instrument.pluginFormat.length > 0
                    ? <div>
                          <dt>Format</dt>
                          <dd data-testid="source-format">{instrument.pluginFormat}</dd>
                      </div>
                    : null}
                {instrument.pluginManufacturer !== undefined && instrument.pluginManufacturer.length > 0
                    ? <div>
                          <dt>Manufacturer</dt>
                          <dd data-testid="source-manufacturer">{instrument.pluginManufacturer}</dd>
                      </div>
                    : null}
                {instrument.pluginVersion !== undefined && instrument.pluginVersion.length > 0
                    ? <div>
                          <dt>Version</dt>
                          <dd data-testid="source-version">{instrument.pluginVersion}</dd>
                      </div>
                    : null}
                {instrument.samplePath !== undefined && instrument.samplePath.length > 0
                    ? <div>
                          <dt>Kit</dt>
                          <dd data-testid="source-sample-path">{instrument.samplePath}</dd>
                      </div>
                    : null}
            </dl>

            {children}
        </section>
    );
}
