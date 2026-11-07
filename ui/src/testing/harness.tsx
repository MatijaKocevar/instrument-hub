import { mkdirSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

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
