# Texas Songbird

A Pebble watchface: a dramatically zoomed-in analog dial on a black field where
each hour position is a **Texas songbird** instead of a number. A single slender
vermilion hand sweeps toward whichever bird is "now." On the hour — and whenever
you tap your wrist — that bird's recorded call plays, and a tap also brings up a
box with the bird's name, description, and Texas range.

Built on the [Chronology 2](https://github.com/arfct/chronology-watchface)
watchface, whose magnified orbiting dial (a large off-screen layer that rotates
around the wrist, with the hand cropped to the visible rim) produces the
"glimpse only a third of the ring" effect. We keep that machinery and draw birds
where Chronology drew numerals.

## Concept

- **Solid black background**, OLED-friendly and maximal contrast.
- **One vermilion hour hand** (default `#FF5500`), no minute or second hand —
  precision comes from the magnified dial.
- **Zoomed dial:** you only ever see ~a third of the ring — one or two birds and
  their neighboring markers — so the composition stays sparse and asymmetric.
- **Minute markers:** a quiet arc of tiny light-gray dots between the hours, with
  a slightly longer white tick at each hour.
- **Birds as numerals:** twelve Texas songbirds ring the dial, mostly clean
  light silhouettes with a few full-color species for contrast.

## The birds (hour → species)

Index 0 is 12 o'clock, going clockwise. The same bird shows for the AM and PM of
each hour (12-hour dial).

| Hour | Bird | Notes |
|-----:|------|-------|
| 12 | Northern Mockingbird | Texas state bird |
| 1  | Painted Bunting | full color |
| 2  | Scissor-tailed Flycatcher | long forked tail |
| 3  | Vermilion Flycatcher | red |
| 4  | Eastern Screech-Owl | ear tufts, upright |
| 5  | Carolina Wren | cocked tail |
| 6  | Bewick's Wren | long cocked tail |
| 7  | American Robin | orange breast |
| 8  | Northern Parula | blue + yellow |
| 9  | Golden-cheeked Warbler | Texas-endemic |
| 10 | Blue Jay | crested, blue |
| 11 | Northern Cardinal | crested, red |

## Interaction

- **On the hour:** the current bird's call plays automatically. Skipped during
  the watch's Quiet Time, and when calls are muted in settings.
- **Wrist tap:** plays the current bird's call and shows its info box (name,
  description, range) for ~4 seconds. Debounced so ordinary motion doesn't
  retrigger it.
- **Settings (Clay):** hand color, and a "Mute bird calls" toggle.

## Audio

Pebble's revived SDK (4.9+) exposes a **Speaker API**; the Pebble Time 2 (Emery)
has a speaker. Calls are stored as **mono 8 kHz 8-bit signed PCM** raw resources
and played by streaming them through `speaker_stream_open/write/close`
(`src/c/main.c`).

> 🔊 **The bundled clips in `resources/audio/` are real bird recordings** fetched
> from [xeno-canto](https://xeno-canto.org/) under Creative-Commons licenses
> (CC BY-NC-SA / CC0), with per-clip credit in `resources/audio/ATTRIBUTION.md`.
> Regenerate them with `tools/fetch_audio.sh` (needs a free `XC_KEY`); see the
> licensing notes below. Audio is not audible in QEMU — confirm on real hardware.

## Platform

**Emery only** (Pebble Time 2, 200×228, 64-color) for now — it's the target with
a speaker. Watches without a speaker (Pebble 2 / diorite / aplite) are out of
scope; gabbro (round Time-series) is a possible later pass.

## Build & run

Prerequisites (Fedora shown; see
[developer.repebble.com/sdk](https://developer.repebble.com/sdk/)):

```bash
# system libs for the QEMU emulator
sudo dnf install nodejs SDL2 glib2 pixman zlib     # (node can also be user-space)

# pebble-tool + SDK (user-space)
curl -LsSf https://astral.sh/uv/install.sh | sh
uv tool install --python 3.13 pebble-tool
pebble sdk install latest
```

Build the watchface:

```bash
pebble build                       # produces build/*.pbw
```

Run in the emulator (headless environments: add `--vnc`):

```bash
pebble install --emulator emery
pebble screenshot                  # capture the current screen
pebble emu-tap                     # simulate a wrist tap (plays call + info box)
pebble emu-set-time --help         # advance the clock to test the on-the-hour call
```

Install on real hardware via the Pebble mobile app / `pebble install --phone`.

### Regenerating assets

```bash
python3 tools/make_birds.py             # 12 bird PNGs -> resources/images/birds/
export XC_KEY=...                        # from https://xeno-canto.org/account
tools/fetch_audio.sh                     # 12 real CC calls -> resources/audio/ (+ ATTRIBUTION.md)
python3 tools/make_placeholder_audio.py  # fallback: synthesized tone placeholders
```

## Repository layout

```
src/c/main.c            watchface logic (dial, hand, birds, audio, tap, info box)
src/pkjs/               Clay config (hand color, mute)
resources/images/birds/ 12 bird bitmaps (bird_00 .. bird_11)
resources/audio/        12 call clips (call_00 .. call_11, PCM s8 8kHz mono)
tools/                  asset generators
package.json, wscript   Pebble build config
```

## Licensing / attribution (audio)

The bundled clips come from **[xeno-canto.org](https://xeno-canto.org)** under
Creative-Commons licenses (CC BY-NC-SA / CC0) — redistribution-safe **with
attribution**, which `resources/audio/ATTRIBUTION.md` provides per clip.
`tools/fetch_audio.sh` deliberately **excludes No-Derivatives (ND) licenses**
because it trims and re-encodes each clip (a derivative work).

> ⚠️ Most clips are **BY-NC-SA (non-commercial)**. That's fine for a personal
> build; if you publish this watchface commercially you'd need to swap those for
> CC0 / commercial-friendly recordings.

Alternatives:

- **Macaulay Library / Cornell Lab of Ornithology** — higher quality but download
  is gated and limited to personal, non-commercial, single-copy use;
  redistribution/publishing requires Cornell's permission. Drop-in replace the
  `resources/audio/call_NN.pcm` files (mono 8 kHz 8-bit signed PCM) and update
  `ATTRIBUTION.md`.

## Credits

- Base watchface: [Chronology 2](https://github.com/arfct/chronology-watchface) (MIT), by Artifact.
- Pebble SDK tooling: [coredevices/pebble-tool](https://github.com/coredevices/pebble-tool).
