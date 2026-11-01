/** Turn an unknown rejection into a message the UI can show. */
export function messageOf (error: unknown): string
{
    if (error instanceof Error && error.message.length > 0)
        return error.message;

    if (typeof error === "string" && error.length > 0)
        return error;

    return "the control API is unreachable";
}
