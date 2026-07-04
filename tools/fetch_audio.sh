#!/usr/bin/env bash
#
# fetch_audio.sh — fetch real bird calls from xeno-canto and convert them to the
# format the watchface expects: mono, 8 kHz, 8-bit *signed* PCM (headerless).
#
# For each of the 12 Texas songbirds it queries the xeno-canto API for a
# Creative-Commons–licensed recording (redistribution-safe, with attribution),
# downloads the clearest short clip, trims it to ~2 s, and writes
#   resources/audio/call_NN.pcm
# It also (re)writes resources/audio/ATTRIBUTION.md with per-clip credit +
# license so the bundled recordings can be published lawfully.
#
# Requires: curl, jq, ffmpeg  (all provided by the flake dev shell).
#
# Usage:
#   tools/fetch_audio.sh            # fetch all 12
#   tools/fetch_audio.sh 4 10       # fetch only these indices (0-based)
#
# API KEY REQUIRED: xeno-canto retired the keyless API v2 (returns 404). This
# script uses API v3, which needs a personal key. Create a free xeno-canto
# account, copy your key from https://xeno-canto.org/account, then:
#   export XC_KEY=xxxxxxxxxxxxxxxx
#   tools/fetch_audio.sh
set -euo pipefail

# ---- config -----------------------------------------------------------------
RATE=8000
DUR=2.0                      # seconds of audio per clip
HP=400                       # high-pass cutoff (Hz) to cut low rumble/handling
XC_API="${XC_API:-https://xeno-canto.org/api/3/recordings}"

# Licenses we accept (all permit redistribution *with modification*, since we
# trim + re-encode the clip — a derivative work). Matched as substrings of the
# xeno-canto `lic` URL, e.g. "by-nc-sa". NOTE: No-Derivatives (ND) licenses are
# excluded separately below — "by-nc" would otherwise substring-match "by-nc-nd".
ALLOWED_LIC='publicdomain|zero|by-sa|by-nc-sa|by-nc|by'
# Reject anything that is a No-Derivatives license (by-nd, by-nc-nd): we modify
# the audio, so ND makes the clip non-redistributable.
DENY_LIC='-nd'

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/resources/audio"
ATTR="$OUT/ATTRIBUTION.md"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# index -> "Common Name|Genus|species"  (scientific names for precise queries)
SPECIES=(
  "Northern Mockingbird|Mimus|polyglottos"
  "Painted Bunting|Passerina|ciris"
  "Scissor-tailed Flycatcher|Tyrannus|forficatus"
  "Vermilion Flycatcher|Pyrocephalus|rubinus"
  "Eastern Screech-Owl|Megascops|asio"
  "Carolina Wren|Thryothorus|ludovicianus"
  "Bewick's Wren|Thryomanes|bewickii"
  "American Robin|Turdus|migratorius"
  "Northern Parula|Setophaga|americana"
  "Golden-cheeked Warbler|Setophaga|chrysoparia"
  "Blue Jay|Cyanocitta|cristata"
  "Northern Cardinal|Cardinalis|cardinalis"
)

for tool in curl jq ffmpeg; do
  command -v "$tool" >/dev/null || { echo "error: '$tool' not found (enter the flake dev shell)" >&2; exit 1; }
done

if [ -z "${XC_KEY:-}" ]; then
  echo "error: XC_KEY is not set. xeno-canto API v3 requires a personal key." >&2
  echo "       Get one at https://xeno-canto.org/account then: export XC_KEY=..." >&2
  exit 1
fi

mkdir -p "$OUT"

# Which indices to process
if [ "$#" -gt 0 ]; then
  INDICES=("$@")
else
  INDICES=($(seq 0 $((${#SPECIES[@]} - 1))))
fi

# ---- attribution file -------------------------------------------------------
# Rebuild the header only on a full run; otherwise append/keep existing.
if [ "$#" -eq 0 ] || [ ! -f "$ATTR" ]; then
  cat > "$ATTR" <<'EOF'
# Bird-call attribution

Recordings are sourced from [xeno-canto](https://xeno-canto.org/) under the
Creative-Commons licenses noted below. Each is redistributed here with
attribution as the license requires. To regenerate, run `tools/fetch_audio.sh`.

| # | Species | XC ID | Recordist | License | Source |
|---|---------|-------|-----------|---------|--------|
EOF
fi

# ---- fetch loop -------------------------------------------------------------
fetch_one() {
  local idx="$1"
  local entry="${SPECIES[$idx]}"
  local common="${entry%%|*}"
  local rest="${entry#*|}"
  local gen="${rest%%|*}"
  local sp="${rest#*|}"
  local nn; nn="$(printf '%02d' "$idx")"

  echo ">> [$nn] $common ($gen $sp)"
  local resp="$WORK/resp_$nn.json"
  # Keep the query minimal (grammar tags vary across API versions); filter and
  # rank by quality/length client-side with jq below.
  local code
  code="$(curl -sS --get "$XC_API" \
    --data-urlencode "query=gen:$gen sp:$sp len:1-30" \
    --data-urlencode "key=$XC_KEY" \
    --data-urlencode "per_page=100" \
    -o "$resp" -w '%{http_code}')" || { echo "   ! query failed (curl)"; return 1; }
  if [ "$code" != "200" ]; then
    echo "   ! query HTTP $code: $(jq -r '.message // "?"' "$resp" 2>/dev/null)"
    return 1
  fi

  # Pick the best allowed recording: highest quality (A>B), then shortest.
  local pick
  pick="$(jq -c --arg lic "$ALLOWED_LIC" --arg deny "$DENY_LIC" '
    [ .recordings[]
      | select(((.lic // "") | ascii_downcase) as $l
               | ($l | test($lic)) and (($l | test($deny)) | not))
      | { id, rec, lic, q, en, url: .url,
          file: (.file // ""),
          # length "m:ss" -> seconds, for sorting
          secs: ((.length // "0:30") | split(":") | (.[0]|tonumber)*60 + (.[1]//"0"|tonumber)) }
    ]
    | sort_by(.q, .secs)
    | .[0] // empty' "$resp")"

  if [ -z "$pick" ] || [ "$pick" = "null" ]; then
    echo "   ! no CC-licensed recording found; leaving existing call_$nn.pcm"
    return 1
  fi

  local id rec lic file en
  id="$(jq -r '.id' <<<"$pick")"
  rec="$(jq -r '.rec' <<<"$pick")"
  lic="$(jq -r '.lic' <<<"$pick")"
  file="$(jq -r '.file' <<<"$pick")"
  en="$(jq -r '.en' <<<"$pick")"
  [ "${file:0:2}" = "//" ] && file="https:$file"

  echo "   xc:$id by $rec ($lic)"
  local mp3="$WORK/xc_$nn.mp3"
  curl -sfL "$file" -o "$mp3" || { echo "   ! download failed"; return 1; }

  # Trim leading silence, take DUR seconds, high-pass, normalize loudness, and
  # emit headerless signed 8-bit PCM at 8 kHz mono.
  ffmpeg -y -loglevel error -i "$mp3" \
    -af "silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.05,highpass=f=$HP,dynaudnorm=g=5" \
    -t "$DUR" -ac 1 -ar "$RATE" -f s8 "$OUT/call_$nn.pcm" \
    || { echo "   ! ffmpeg conversion failed"; return 1; }

  local bytes; bytes="$(stat -c%s "$OUT/call_$nn.pcm")"
  echo "   wrote call_$nn.pcm (${bytes} bytes)"

  # License short code + human summary for the table.
  local code="CC"
  case "$lic" in
    *publicdomain*|*zero*) code="CC0" ;;
    *by-nc-sa*) code="CC BY-NC-SA" ;;
    *by-nc*)    code="CC BY-NC" ;;
    *by-sa*)    code="CC BY-SA" ;;
    *by*)       code="CC BY" ;;
  esac
  printf '| %s | %s | [XC%s](https://xeno-canto.org/%s) | %s | %s | %s |\n' \
    "$idx" "$common" "$id" "$id" "$rec" "$code" "$en" >> "$ATTR"
}

failures=0
for idx in "${INDICES[@]}"; do
  fetch_one "$idx" || failures=$((failures + 1))
done

echo
echo "Done. $((${#INDICES[@]} - failures))/${#INDICES[@]} clips fetched."
echo "Attribution: $ATTR"
[ "$failures" -eq 0 ] || echo "Note: $failures species had no usable clip — rerun those indices or supply manually."
