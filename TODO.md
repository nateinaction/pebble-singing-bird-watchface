# Texas Songbird — TODO

Status of the build so far, and what remains.

## Done

- [x] Nix dependency management (`flake.nix` + `.envrc`, `use flake`): dev shell
      with python3+Pillow, ffmpeg, nodejs (npm for `pebble build`), uv (pebble
      CLI), curl/jq. **Working (2026-07-04):** the Nix daemon/store blocker is
      resolved, `flake.lock` is committed, `direnv allow` run, and a full
      `pebble build` inside `nix develop` succeeds end-to-end. Uses
      `mkShellNoCC` so the host gcc doesn't shadow the SDK's ARM toolchain
      (`-mthumb` error). See `NIX_SETUP.md`.
- [x] `tools/fetch_audio.sh` written (xeno-canto **API v3**; needs an `XC_KEY`).
- [x] Project scaffolded on the Chronology 2 base (new UUID, `Texas Songbird`,
      emery-only target).
- [x] `src/c/main.c`: orbiting dial + vermilion hand kept from Chronology;
      numerals replaced with lazily-loaded bird bitmaps (current hour ± 2);
      minute dots + hour ticks; Speaker PCM playback on the hour (Quiet-Time
      aware) and on wrist tap; tap info box (name/description/range); Clay
      config for hand color + mute.
- [x] 12 bird silhouette PNGs (`tools/make_birds.py`).
- [x] 12 **real** call clips fetched from xeno-canto (CC BY-NC-SA / CC0) via
      `tools/fetch_audio.sh`; attribution in `resources/audio/ATTRIBUTION.md`.
      (`tools/make_placeholder_audio.py` remains as a fallback generator.)
- [x] `pebble build` succeeds for emery → `build/*.pbw`
      (resources ~120 KB / 256 KB, ~124 KB free heap).

## Left to do

### 1. Real bird audio (biggest item)
- [x] Write `tools/fetch_audio.sh`: pulls one Creative-Commons recording per
      species from xeno-canto, trims to the clearest ~2 s, converts to mono
      8 kHz 8-bit signed PCM. Also generates `resources/audio/ATTRIBUTION.md`.
- [x] **Ran it (2026-07-04).** API v3 works with a personal `XC_KEY`; the jq
      paths + download URLs were verified live. All 12 placeholder clips are now
      **real xeno-canto recordings** (`resources/audio/call_NN.pcm`), with
      per-clip credit in `resources/audio/ATTRIBUTION.md`.
- [x] Fixed a licensing bug: `ALLOWED_LIC`'s `by-nc` substring also matched
      `by-nc-nd` (No-Derivatives), which we can't redistribute since we trim +
      re-encode. Added a `DENY_LIC='-nd'` exclusion. All 12 are now CC BY-NC-SA
      or CC0 (redistribution-safe *with modification*).
- [x] Source decided: **xeno-canto (CC, redistribution-safe)**. Macaulay/Cornell
      (personal-use only) remains a manual-drop-in alternative — see README.
- [x] Total fits: resource pack is 195.6 KB / 256 KB (76%) with real audio.
- [ ] Tune clip volume/length on real hardware so calls sound good (needs a
      Pebble Time 2 — see §4).

### 2. Emulator verification (run in a normal terminal, not the agent)
- [ ] Run `pebble install --emulator emery` + `pebble screenshot` and eyeball the
      dial: black field, vermilion hand toward "now," 1–2 birds on the visible arc,
      quiet dot arc.
  - **npm blocker: CLEARED.** `nodejs`/`npm` come from the Nix dev shell, which
    now works; `pebble build` succeeds inside `nix develop`.
  - **Remaining blocker:** launching the QEMU emulator still fails from inside the
    Claude Code agent harness — `pebble install --emulator emery` spawns a
    QEMU/SDL GUI+audio window that is killed immediately (exit 1, no output),
    even with the tool sandbox disabled. QEMU did briefly launch once but the
    pebble tool got `[Errno 111] Connection refused` on its control socket.
    **Fix: run it yourself in a normal interactive terminal:**
    `direnv allow` (or `nix develop`), then
    `pebble install --emulator emery` and `pebble screenshot`. Use `--vnc` if
    headless. The build artifact (`build/pebble-singing-bird-watchface.pbw`) is
    up to date and ready to install.
- [ ] `pebble emu-tap` → confirm info box appears with correct text and
      auto-dismisses; check `pebble logs` for audio calls firing without crashing.
- [ ] `pebble emu-set-time` across an hour boundary → confirm the on-the-hour call
      path runs and respects Quiet Time.

### 3. Visual polish (iterate after first screenshot)
- [ ] Refine bird silhouettes in `tools/make_birds.py` (recognizability, sizing on
      the 200×228 screen), or drop in hand-drawn PNGs.
- [ ] Tune bird radial placement / size in `my_face_draw()` (the
      `hour_inset + bb.size.h - 6` offset) so birds sit nicely inside the ticks.
- [ ] Confirm bitmap transparency (`GCompOpSet`) renders cleanly over black.
- [ ] Consider a real menu icon (currently a 25×25 crop of the cardinal).

### 4. Hardware pass (needs a physical Pebble Time 2)
- [ ] Sideload the `.pbw`; confirm calls are audible and pleasant at `AUDIO_VOLUME`.
- [ ] Verify tap sensitivity / debounce (`TAP_DEBOUNCE_MS`) feels right in daily use.
- [ ] Check the on-the-hour call only fires while the watchface is foreground
      (expected Pebble limitation).

### 5. Nice-to-haves / stretch
- [ ] gabbro (round Time-series) support as a second layout pass.
- [ ] Optional "quiet hours" range in settings beyond system Quiet Time.
- [ ] Per-bird accent color for the hand, or brief note-animation on play.
- [ ] `store/` screenshots + preview GIF for App Store listing; `pebble publish`.

## Known limitations
- On-the-hour audio only plays when the watchface is the foreground app.
- Audio is not verifiable in QEMU (no sound) — needs real hardware.
- Bundled audio is real xeno-canto CC recordings (BY-NC-SA / CC0); the
  BY-NC clips are non-commercial-only, which matters if publishing to a store.
