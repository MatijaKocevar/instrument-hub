import { useCallback, useEffect, useMemo, useRef, useState } from "react";

export type BridgeLinkStatus =
  "idle" | "connecting" | "linked" | "reconnecting" | "closed" | "error";

export interface MidiBridgeLink {
  readonly status: BridgeLinkStatus;
  readonly error: string | null;
  readonly sendMidi: (data: Uint8Array) => boolean;
  readonly reconnect: () => void;
}

export interface MidiBridgeLinkOptions {
  readonly baseUrl: string;
  readonly token: string | null;
  readonly clientId: string;
  readonly name: string;
  readonly active: boolean;
}

const HEARTBEAT_MS = 2500;
const PONG_TIMEOUT_MS = 8000;

export function midiBridgeUrl(baseUrl: string, token: string | null): string {
  try {
    const url = new URL(baseUrl);
    url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
    url.pathname = "/midi-input";
    url.search = "";
    url.hash = "";

    if (token !== null && token.length > 0)
      url.searchParams.set("token", token);

    return url.toString();
  } catch {
    return "";
  }
}

export function useMidiBridgeLink(
  options: MidiBridgeLinkOptions,
): MidiBridgeLink {
  const url = useMemo(
    () => midiBridgeUrl(options.baseUrl, options.token),
    [options.baseUrl, options.token],
  );
  const [status, setStatus] = useState<BridgeLinkStatus>("idle");
  const [error, setError] = useState<string | null>(null);

  const socketRef = useRef<WebSocket | null>(null);
  const retryRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const heartbeatRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const lastPongRef = useRef(0);
  const attemptRef = useRef(0);

  const clearTimers = useCallback((): void => {
    if (retryRef.current !== null) {
      clearTimeout(retryRef.current);
      retryRef.current = null;
    }

    if (heartbeatRef.current !== null) {
      clearInterval(heartbeatRef.current);
      heartbeatRef.current = null;
    }
  }, []);

  const connect = useCallback((): void => {
    clearTimers();

    const previous = socketRef.current;
    socketRef.current = null;
    previous?.close();

    if (url.length === 0) {
      setStatus("error");
      setError("The control API address is not valid");
      return;
    }

    let socket: WebSocket;

    try {
      socket = new WebSocket(url);
    } catch {
      setStatus("error");
      setError("This browser could not open the MIDI socket");
      return;
    }

    socketRef.current = socket;
    socket.binaryType = "arraybuffer";
    setStatus("connecting");
    setError(null);

    const retry = (): void => {
      if (socketRef.current !== socket) return;

      socketRef.current = null;
      clearTimers();
      setStatus("reconnecting");

      const delay = Math.min(5000, 400 * 2 ** attemptRef.current);
      attemptRef.current += 1;
      retryRef.current = setTimeout(() => {
        retryRef.current = null;
        connect();
      }, delay);
    };

    socket.addEventListener("open", () => {
      if (socketRef.current !== socket) return;

      socket.send(
        JSON.stringify({
          type: "hello",
          id: options.clientId,
          name: options.name,
        }),
      );

      lastPongRef.current = Date.now();
      heartbeatRef.current = setInterval(() => {
        if (socket.readyState !== WebSocket.OPEN) return;

        if (Date.now() - lastPongRef.current > PONG_TIMEOUT_MS) {
          socket.close();
          return;
        }

        socket.send(JSON.stringify({ type: "ping" }));
      }, HEARTBEAT_MS);
    });

    socket.addEventListener("message", (event) => {
      if (typeof event.data !== "string") return;

      try {
        const parsed: unknown = JSON.parse(event.data);

        if (
          parsed === null ||
          typeof parsed !== "object" ||
          !("type" in parsed)
        )
          return;

        const type = (parsed as { type?: unknown }).type;

        if (type === "ready") {
          attemptRef.current = 0;
          lastPongRef.current = Date.now();
          setStatus("linked");
          setError(null);
        } else if (type === "pong") {
          lastPongRef.current = Date.now();
        }
      } catch {}
    });

    socket.addEventListener("close", () => {
      retry();
    });

    socket.addEventListener("error", () => {
      if (socketRef.current !== socket) return;

      setError("The engine closed the MIDI link");
    });
  }, [clearTimers, options.clientId, options.name, url]);

  useEffect(() => {
    if (!options.active) {
      const socket = socketRef.current;
      socketRef.current = null;
      socket?.close();
      clearTimers();
      attemptRef.current = 0;
      setStatus("idle");
      setError(null);
      return;
    }

    attemptRef.current = 0;
    connect();

    return () => {
      const socket = socketRef.current;
      socketRef.current = null;
      socket?.close();
      clearTimers();
    };
  }, [clearTimers, connect, options.active]);

  useEffect(() => {
    const onVisibilityChange = (): void => {
      if (!options.active) return;

      if (document.visibilityState !== "visible") return;

      if (socketRef.current === null) {
        attemptRef.current = 0;
        connect();
      }
    };

    document.addEventListener("visibilitychange", onVisibilityChange);

    return () => {
      document.removeEventListener("visibilitychange", onVisibilityChange);
    };
  }, [connect, options.active]);

  const sendMidi = useCallback((data: Uint8Array): boolean => {
    const socket = socketRef.current;

    if (socket === null || socket.readyState !== WebSocket.OPEN) return false;

    const frame = new Uint8Array(data.length + 1);
    frame[0] = 1;
    frame.set(data, 1);

    try {
      socket.send(frame);
      return true;
    } catch {
      return false;
    }
  }, []);

  const reconnect = useCallback((): void => {
    if (!options.active) return;

    attemptRef.current = 0;
    connect();
  }, [connect, options.active]);

  return { status, error, sendMidi, reconnect };
}
