#!/bin/sh
set -eu

SRC_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CATALOG_DIR="$SRC_DIR/configs"
DEST_DIR="/etc/sensors.d"
VENDOR="auto"
X299_VARIANT="auto"
SIV_ID=""
UNINSTALL=0
HWMON_ROOT=${HWMON_ROOT:-/sys/class/hwmon}

usage() {
    cat <<'USAGE'
Usage:
  install-sensorsd.sh [--dest DIR] [--vendor auto|amd|intel]
                      [--siv-id HEXID]
                      [--x299-variant auto|kabylakex|skylakex]
  install-sensorsd.sh --uninstall [--dest DIR]

Installs only the sensor mappings for the currently detected Gigabyte SIV ID.
The active SIV ID is normally read from the raw-ID suffix on the it87 hwmon
name, for example: it8689_a0040507.

Options:
  --dest DIR            sensors.d destination (default: /etc/sensors.d)
  --vendor VALUE        auto, amd, or intel (default: auto)
  --siv-id HEXID        override automatic SIV detection; accepts 0x12345678
                        or 12345678
  --x299-variant VALUE  auto, kabylakex, or skylakex. Required only when an
                        X299 SIV has processor-dependent mappings.
  --uninstall           remove the installed Gigabyte it87 sensors.d file
USAGE
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --dest)
            [ "$#" -ge 2 ] || { echo "--dest requires a value" >&2; exit 2; }
            DEST_DIR=$2
            shift 2
            ;;
        --vendor)
            [ "$#" -ge 2 ] || { echo "--vendor requires a value" >&2; exit 2; }
            VENDOR=$(printf '%s' "$2" | tr '[:upper:]' '[:lower:]')
            shift 2
            ;;
        --siv-id)
            [ "$#" -ge 2 ] || { echo "--siv-id requires a value" >&2; exit 2; }
            SIV_ID=$(printf '%s' "$2" | sed 's/^0[xX]//' | tr '[:upper:]' '[:lower:]')
            shift 2
            ;;
        --x299-variant)
            [ "$#" -ge 2 ] || { echo "--x299-variant requires a value" >&2; exit 2; }
            X299_VARIANT=$(printf '%s' "$2" | tr '[:upper:]' '[:lower:]' | tr -d '_-')
            shift 2
            ;;
        --uninstall)
            UNINSTALL=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [ "$UNINSTALL" -eq 1 ]; then
    OUTPUT="$DEST_DIR/gigabyte-it87.conf"
    if [ -e "$OUTPUT" ] || [ -L "$OUTPUT" ]; then
        rm -f -- "$OUTPUT"
        echo "Removed $OUTPUT."
    else
        echo "No installed Gigabyte it87 sensors.d file found at $OUTPUT."
    fi
    exit 0
fi

case "$VENDOR" in
    auto|amd|intel) ;;
    *) echo "--vendor must be auto, amd, or intel" >&2; exit 2 ;;
esac

case "$X299_VARIANT" in
    auto|kabylakex|skylakex) ;;
    *) echo "--x299-variant must be auto, kabylakex, or skylakex" >&2; exit 2 ;;
esac

if [ -n "$SIV_ID" ]; then
    case "$SIV_ID" in
        *[!0-9a-f]*|'') echo "Invalid --siv-id: expected exactly 8 hexadecimal digits" >&2; exit 2 ;;
    esac
    [ "${#SIV_ID}" -eq 8 ] || {
        echo "Invalid --siv-id: expected exactly 8 hexadecimal digits" >&2
        exit 2
    }
else
    ACTIVE_IDS=$(
        for namefile in "$HWMON_ROOT"/hwmon*/name; do
            [ -r "$namefile" ] || continue
            name=$(cat "$namefile" 2>/dev/null || true)
            printf '%s\n' "$name" |
                sed -n 's/^it[0-9][0-9]*_\([0-9A-Fa-f]\{8\}\)$/\1/p' |
                tr '[:upper:]' '[:lower:]'
        done | sort -u
    )

    ID_COUNT=$(printf '%s\n' "$ACTIVE_IDS" | awk 'NF { n++ } END { print n + 0 }')
    case "$ID_COUNT" in
        0)
            echo "No raw-ID-suffixed it87 hwmon device was found under $HWMON_ROOT." >&2
            echo "Load the it87 module with the SIV-name patch, or use --siv-id." >&2
            exit 1
            ;;
        1)
            SIV_ID=$(printf '%s\n' "$ACTIVE_IDS" | awk 'NF { print; exit }')
            ;;
        *)
            echo "Multiple SIV IDs were detected:" >&2
            printf '  %s\n' $ACTIVE_IDS >&2
            echo "Refusing to combine mappings from multiple boards; use --siv-id." >&2
            exit 1
            ;;
    esac
fi

if [ "$VENDOR" = auto ]; then
    CPU_VENDOR=$(awk -F: '/^[[:space:]]*vendor_id[[:space:]]*:/ {
        gsub(/[[:space:]]/, "", $2); print $2; exit
    }' /proc/cpuinfo 2>/dev/null || true)

    case "$CPU_VENDOR" in
        AuthenticAMD) VENDOR=amd ;;
        GenuineIntel) VENDOR=intel ;;
        *)
            echo "Unable to determine CPU vendor; use --vendor amd|intel." >&2
            exit 2
            ;;
    esac
fi

X299_IDS="2005080b 2008090b 2108090b 2208090b 5008090a 5108090a"
IS_X299=0
for id in $X299_IDS; do
    if [ "$SIV_ID" = "$id" ]; then
        IS_X299=1
        break
    fi
done

case "$VENDOR:$IS_X299" in
    amd:*)
        CATALOG="$CATALOG_DIR/gigabyte-it87-amd.conf"
        ;;
    intel:0)
        CATALOG="$CATALOG_DIR/gigabyte-it87-intel.conf"
        ;;
    intel:1)
        if [ "$X299_VARIANT" = auto ]; then
            echo "SIV 0x$SIV_ID has processor-dependent X299 mappings." >&2
            echo "Re-run with --x299-variant kabylakex or skylakex." >&2
            exit 3
        fi
        CATALOG="$CATALOG_DIR/gigabyte-it87-intel-$X299_VARIANT.conf"
        ;;
esac

[ -r "$CATALOG" ] || {
    echo "Mapping catalog not found: $CATALOG" >&2
    exit 1
}

TMP=$(mktemp "${TMPDIR:-/tmp}/gigabyte-it87.XXXXXX")
trap 'rm -f "$TMP"' EXIT HUP INT TERM

# Extract every stanza that contains the active SIV, but narrow deduplicated
# chip statements and motherboard comments to that SIV only. This preserves
# all Super-I/O instances belonging to the current board without installing
# mappings for unrelated SIV IDs.
awk -v wanted="$SIV_ID" '
BEGIN {
    RS = ""
    ORS = "\n"
    wanted = tolower(wanted)
}

function pattern_siv(pattern, a, n, tail) {
    n = split(pattern, a, "_")
    if (n < 2)
        return ""
    tail = a[n]
    sub(/-\*$/, "", tail)
    tail = tolower(tail)
    if (length(tail) != 8 || tail ~ /[^0-9a-f]/)
        return ""
    return tail
}

{
    nlines = split($0, line, "\n")
    chip_idx = 0
    match_count = 0
    delete matched_pattern

    for (i = 1; i <= nlines; i++) {
        if (line[i] ~ /^chip[[:space:]]/) {
            chip_idx = i
            nparts = split(line[i], part, "\"")
            for (j = 2; j <= nparts; j += 2) {
                id = pattern_siv(part[j])
                if (id == wanted)
                    matched_pattern[++match_count] = part[j]
            }
            break
        }
    }

    if (!chip_idx || !match_count)
        next

    keep = 0
    for (i = 1; i < chip_idx; i++) {
        if (line[i] ~ /^# SIV 0x[0-9A-Fa-f]+$/) {
            id = line[i]
            sub(/^# SIV 0x/, "", id)
            keep = (tolower(id) == wanted)
        }
        if (keep)
            print line[i]
    }

    printf "chip"
    for (i = 1; i <= match_count; i++)
        printf " \"%s\"", matched_pattern[i]
    printf "\n"

    for (i = chip_idx + 1; i <= nlines; i++)
        print line[i]
    print ""
}
' "$CATALOG" > "$TMP"

CHIP_COUNT=$(grep -c '^chip ' "$TMP" || true)
if [ "$CHIP_COUNT" -eq 0 ]; then
    echo "No $VENDOR mapping was found for SIV 0x$SIV_ID." >&2
    exit 4
fi

mkdir -p "$DEST_DIR"
OUTPUT="$DEST_DIR/gigabyte-it87.conf"
INSTALL_TMP="$DEST_DIR/.gigabyte-it87.conf.$$"

{
    case "$VENDOR:$IS_X299:$X299_VARIANT" in
        intel:1:kabylakex) echo "# Gigabyte it87 sensor mappings — Intel X299 Kabylake-X" ;;
        intel:1:skylakex)  echo "# Gigabyte it87 sensor mappings — Intel X299 Skylake-X" ;;
        amd:*)              echo "# Gigabyte it87 sensor mappings — AMD" ;;
        *)                  echo "# Gigabyte it87 sensor mappings — Intel" ;;
    esac
    echo
    cat "$TMP"
} > "$INSTALL_TMP"
chmod 0644 "$INSTALL_TMP"
mv -f "$INSTALL_TMP" "$OUTPUT"
trap - EXIT HUP INT TERM
rm -f "$TMP"

echo "Installed SIV 0x$SIV_ID mappings to $OUTPUT ($CHIP_COUNT chip stanza(s))."
