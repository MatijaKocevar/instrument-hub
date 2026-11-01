import { mkdirSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { renderToStaticMarkup } from "react-dom/server";

import App from "../App";
import { AppStoreProvider } from "../state/AppStoreContext";
import { AppStore } from "../state/store";

let failures = 0;

export function check(condition: boolean, what: string): void {
  console.log(`${condition ? "PASS" : "FAIL"} ${what}`);

  if (!condition) failures += 1;
}

export function failureCount(): number {
  return failures;
}

export function countOf(html: string, needle: string): number {
  return html.split(needle).length - 1;
}

function artifactPath(name: string): string {
  return fileURLToPath(
    new URL(`../../tests/artifacts/${name}`, import.meta.url),
  );
}

export function writeArtifact(name: string, html: string): void {
  mkdirSync(fileURLToPath(new URL("../../tests/artifacts/", import.meta.url)), {
    recursive: true,
  });
  writeFileSync(artifactPath(name), html);

  console.log(
    `snapshot: wrote ${String(html.length)} bytes to ${artifactPath(name)}`,
  );
}

export function renderApp(
  store: AppStore,
  initialPath?: string,
  viewportWidth?: number,
): string {
  return renderToStaticMarkup(
    <AppStoreProvider store={store}>
      <App
        {...(initialPath === undefined ? {} : { initialPath })}
        {...(viewportWidth === undefined ? {} : { viewportWidth })}
      />
    </AppStoreProvider>,
  );
}

export function waitFor(
  predicate: () => boolean,
  timeoutMs: number,
): Promise<void> {
  return new Promise((resolve, reject) => {
    const deadline = Date.now() + timeoutMs;

    const poll = (): void => {
      if (predicate()) {
        resolve();
        return;
      }

      if (Date.now() > deadline) {
        reject(new Error(`condition not met within ${String(timeoutMs)} ms`));
        return;
      }

      setTimeout(poll, 5);
    };

    poll();
  });
}
