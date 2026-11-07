import type { CallToolResult } from "@modelcontextprotocol/sdk/types.js";

export function jsonResult(value: unknown): CallToolResult {
  return {
    content: [{ type: "text", text: JSON.stringify(value, null, 2) }],
  };
}

export function errorResult(error: unknown): CallToolResult {
  return {
    content: [{ type: "text", text: `Instrument Hub error: ${message(error)}` }],
    isError: true,
  };
}

export async function toolResult(
  load: () => Promise<unknown>,
): Promise<CallToolResult> {
  try {
    return jsonResult(await load());
  } catch (error) {
    return errorResult(error);
  }
}

export function formatJson(value: unknown): string {
  return JSON.stringify(value, null, 2);
}

function message(error: unknown): string {
  if (error instanceof Error) return error.message;
  return String(error);
}
