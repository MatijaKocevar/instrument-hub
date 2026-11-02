# Manual acceptance: 64-pad Analog Lab V state pass

This is the human half of T131. The automated tests prove per-patch state with
64 in-repo `TestPlugin` patches, split into eight isolated batch processes, and
with two Analog Lab V patches. Loading 64 Wine-hosted Analog Lab instances
automatically would be slow, brittle and outside the warm-pool budget, so the
full 64-pad pass with the real plugin stays manual.

Do not automate this pass by spawning one process per patch. The application
itself must keep the normal warm-pool bound: at most two live Analog Lab
instances in the process at any time (the armed patch plus one neighbour). The
`ihub-multipatch-batch` regression uses one child per family (eight patches),
not one per pad.

---

## 1. What this pass proves

- Every one of the 64 Launchpad pads maps to its own patch.
- Each patch can hold a different Analog Lab V state.
- Switching away captures that patch, and switching back restores it.
- Restarting the application restores all 64 patches from the sidecar.
- No editor stays bound to the previous patch.
- The process never exceeds two live Analog Lab instances.

The 64-patch `TestPlugin` regression (`ihub-multipatch-batch`) already covers
the state half in eight isolated child processes, and
`ihub-analog-lab-state` covers the real plugin-level half with two patches.
This checklist covers the user-facing 64-pad half with Analog Lab V.

## 2. Preconditions

Record these before starting:

- [ ] Analog Lab V is installed and licensed on the test machine, either
      natively or through `vstbridge`/`yabridge`.
- [ ] The launchpad mapping and the application are built from the current
      tree (`scripts/build.sh`).
- [ ] The Launchpad Mini MK3 is connected, or the virtual Launchpad driver is
      running for a pad-only dry run.
- [ ] A writable set directory is available. Keep `set.yaml` and its
      `set.plugin-state/` sidecar together; copy both when backing up.
- [ ] The machine has enough memory for two Wine-hosted instances plus the
      application. This is not a 64-instance test.

## 3. Prepare the 64-patch set

Use the application UI. Do not hand-edit a file with 64 raw plugin blobs.

1. [ ] Start `./build/engine/instrument-hub` with the normal 64-slot set.
2. [ ] On the Grid page, confirm eight families of eight pads:
       Keys, Drums, Bass, Pads, Lead, FX, Percussion, Vox.
3. [ ] For each pad 0-63, open the Instrument page and assign **Analog Lab V**
       as the source. Give every pad a distinct, recognisable preset. A useful
       scheme is one preset per family and eight variations per family.
4. [ ] Arm each pad once after assigning it, wait until it reports `active`,
       play a few notes and confirm the sound matches the chosen preset.
5. [ ] Save the set. Confirm the save creates `set.plugin-state/` next to the
       set with a non-empty blob for every assigned patch.
6. [ ] Quit the application and reopen the set. Arm a few pads; the presets
       must come back without reassigning them.

If any pad fails at this stage, stop and record the pad, family and observed
warm state. The full pass is not meaningful until all 64 assign and restore.

## 4. Full 64-pad switch pass

Keep an eye on the process while working. On Linux, `pgrep -fa` / `htop` can
show the Wine processes; the engine's own `/state` response reports each
slot's warm state. Only the armed slot and one neighbour should be live.

For every family, work left to right:

### Keys (pads 1-8)
- [ ] Pad 1 arms, is audible and restores its preset.
- [ ] Pad 2 arms, is audible and restores its preset.
- [ ] Pad 3 arms, is audible and restores its preset.
- [ ] Pad 4 arms, is audible and restores its preset.
- [ ] Pad 5 arms, is audible and restores its preset.
- [ ] Pad 6 arms, is audible and restores its preset.
- [ ] Pad 7 arms, is audible and restores its preset.
- [ ] Pad 8 arms, is audible and restores its preset.

### Drums (pads 9-16)
- [ ] Pads 9-16 all arm, sound and restore. Note any pad that reports
      `warming` for more than a few seconds or stays `cold`.

### Bass (pads 17-24)
- [ ] Pads 17-24 all arm, sound and restore.

### Pads (pads 25-32)
- [ ] Pads 25-32 all arm, sound and restore.

### Lead (pads 33-40)
- [ ] Pads 33-40 all arm, sound and restore.

### FX (pads 41-48)
- [ ] Pads 41-48 all arm, sound and restore.

### Percussion (pads 49-56)
- [ ] Pads 49-56 all arm, sound and restore.

### Vox (pads 57-64)
- [ ] Pads 57-64 all arm, sound and restore.

After the sweep:

- [ ] No more than two Analog Lab instances are alive at once.
- [ ] No pad is silent, stuck on `warming`, or playing the previous pad's
      preset.
- [ ] Opening the editor for the armed pad shows that pad's preset; opening
      an editor for a non-armed pad is not possible or closes the old one.

## 5. A -> B -> A state round trip

Choose two patches in different families, for example Keys pad 1 and Pads
pad 25.

1. [ ] Arm patch A and note its preset name and a distinctive sound.
2. [ ] Arm patch B, confirm B's preset is audible and different.
3. [ ] Arm patch A again. It must return to A's preset, not B's.
4. [ ] Repeat with a second pair from two other families.
5. [ ] Check the sidecar files for both patches are non-empty and have
       different sizes or contents (the UI does not need to show hashes; a
       file manager or `ls -l set.plugin-state/` is enough).

## 6. Restart pass

1. [ ] Quit the application normally. Confirm no Wine host process is left
       behind for the two test patches.
2. [ ] Start the application again and load the same set.
3. [ ] Arm all 64 pads in the same order as section 4.
4. [ ] Every patch must restore its own preset, sound and state; no patch may
       fall back to Analog Lab's default preset.
5. [ ] Repeat the A -> B -> A check from section 5 after the restart.
6. [ ] Confirm the warm-pool bound still holds: at most two live instances.

## 7. Evidence to record

Write these into `HANDOVER.md` or the release notes:

- Date, machine, Analog Lab V version, bridge (`native`, `vstbridge` or
  `yabridge`) and bundle path.
- Number of patches assigned and number that restored after restart.
- Any pad that failed, with its family, slot, warm state and preset name.
- Peak live Analog Lab instance count seen during the pass.
- Sidecar location and approximate total size.
- Whether the editor-follow behaviour was correct for the armed patch.

## 8. Pass criteria

The pass is green only when:

- all 64 pads arm, sound and restore their own preset;
- every A -> B -> A round trip returns to the first patch;
- a restart restores all 64 patches from the sidecar;
- no more than two Analog Lab instances are ever live in one process;
- no editor shows the previous patch after arming a new one;
- no crash, hang or unhandled Wine page fault is observed.

If any item fails, leave the application open, capture the console output and
the failing pad's warm state, then file the result as a partial pass. Do not
turn this checklist into a 64-instance automated test to work around a
failure.
