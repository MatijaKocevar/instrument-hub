import { readJsonObject, sendJson } from "../http";
import type { FixtureContext, FixtureRoute } from "../types";
import { recalculateChain } from "../../fixtures";

const OVER_GAIN = 1.9952623149688795;

function stepGain(step: number): number {
  if (step === 0) return 0;
  if (step === 5) return OVER_GAIN;

  return step / 4;
}

function stepDb(step: number): number {
  const gain = stepGain(step);

  return gain <= 0 ? -60 : 20 * Math.log10(gain);
}

function selectTrack(context: FixtureContext, index: number): boolean {
  const track = context.state.tracks?.find(
    (candidate) => candidate.index === index,
  );

  if (track === undefined) return false;

  context.state.armedSlot = track.slot;
  context.state.slots = context.state.slots.map((slot) => ({
    ...slot,
    armed: slot.index === track.slot,
  }));
  context.state.tracks = (context.state.tracks ?? []).map((candidate) => ({
    ...candidate,
    selected: candidate.index === index,
  }));

  return true;
}

export const handleTracks: FixtureRoute = (context, request, response) => {
  if (request.method !== "POST" || request.url === undefined) return false;

  const select = /^\/tracks\/(\d+)\/select$/.exec(request.url);

  if (select !== null) {
    const index = Number(select[1]);

    if (!selectTrack(context, index)) {
      sendJson(response, 404, { error: "track index is out of range" });
      return true;
    }

    context.trackRequests.push({ kind: "select", index });
    sendJson(response, 200, context.state);
    return true;
  }

  const toggle = /^\/tracks\/(\d+)\/fx\/(\d+)\/toggle$/.exec(request.url);

  if (toggle !== null) {
    const index = Number(toggle[1]);
    const hot = Number(toggle[2]);
    const track = context.state.tracks?.find(
      (candidate) => candidate.index === index,
    );
    const entry = track?.hotFx.find((candidate) => candidate.index === hot);

    if (track === undefined || entry === undefined) {
      sendJson(response, 404, {
        error: "track or hot FX index is out of range",
      });
      return true;
    }

    if (entry.id.length === 0) {
      sendJson(response, 400, { error: "no hot fx is assigned to that index" });
      return true;
    }

    const chain = context.chainFor(track.slot);
    const insert = chain.inserts.find((candidate) => candidate.id === entry.id);

    if (insert === undefined) {
      sendJson(response, 400, { error: "hot fx insert is not in the chain" });
      return true;
    }

    insert.bypassed = !insert.bypassed;
    recalculateChain(chain);
    entry.bypassed = insert.bypassed;
    context.trackRequests.push({ kind: "toggle", index, hot });
    sendJson(response, 200, context.state);
    return true;
  }

  const assign = /^\/tracks\/(\d+)\/fx\/(\d+)\/assign$/.exec(request.url);

  if (assign !== null) {
    const index = Number(assign[1]);
    const hot = Number(assign[2]);

    readJsonObject(request, response, (body) => {
      const track = context.state.tracks?.find(
        (candidate) => candidate.index === index,
      );
      const entry = track?.hotFx.find((candidate) => candidate.index === hot);
      const insertId = typeof body.insertId === "string" ? body.insertId : null;

      if (track === undefined || entry === undefined) {
        sendJson(response, 404, {
          error: "track or hot FX index is out of range",
        });
        return;
      }

      if (insertId === null) {
        sendJson(response, 400, { error: "insertId must be a string" });
        return;
      }

      if (insertId.length === 0) {
        entry.id = "";
        entry.name = "";
        entry.bypassed = false;
        context.trackRequests.push({
          kind: "assign",
          index,
          hot,
          insertId,
        });
        sendJson(response, 200, context.state);
        return;
      }

      const chain = context.chainFor(track.slot);
      const insert = chain.inserts.find(
        (candidate) => candidate.id === insertId,
      );

      if (insert === undefined) {
        sendJson(response, 400, { error: "insert is not in the live chain" });
        return;
      }

      track.hotFx = track.hotFx.map((candidate) =>
        candidate.index !== hot && candidate.id === insertId
          ? { ...candidate, id: "", name: "", bypassed: false }
          : candidate,
      );
      const target = track.hotFx.find((candidate) => candidate.index === hot);

      if (target !== undefined) {
        target.id = insert.id;
        target.name = insert.name;
        target.bypassed = insert.bypassed;
      }

      context.trackRequests.push({
        kind: "assign",
        index,
        hot,
        insertId,
      });
      sendJson(response, 200, context.state);
    });

    return true;
  }

  const volume = /^\/tracks\/(\d+)\/volume$/.exec(request.url);

  if (volume !== null) {
    const index = Number(volume[1]);

    readJsonObject(request, response, (body) => {
      const track = context.state.tracks?.find(
        (candidate) => candidate.index === index,
      );
      const step = body.step;

      if (track === undefined) {
        sendJson(response, 404, { error: "track index is out of range" });
        return;
      }

      if (
        typeof step !== "number" ||
        !Number.isInteger(step) ||
        step < 0 ||
        step > 5
      ) {
        sendJson(response, 400, { error: "step must be between 0 and 5" });
        return;
      }

      track.volumeStep = step;
      track.volumeDb = stepDb(step);
      track.gain = stepGain(step);
      context.trackRequests.push({ kind: "volume", index, step });
      sendJson(response, 200, context.state);
    });

    return true;
  }

  return false;
};
