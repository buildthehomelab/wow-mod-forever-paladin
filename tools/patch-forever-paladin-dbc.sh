#!/usr/bin/env bash
#
# Write Holy Strike's changes into a 3.3.5a (12340) client's Spell.dbc and SkillLineAbility.dbc,
# for an optional client patch. The module works without it; with it, the tooltip says what Holy
# Strike really does and it sits in the Holy tab of the spellbook instead of General.
#
# Usage: tools/patch-forever-paladin-dbc.sh <Spell.dbc> <SkillLineAbility.dbc> [output dir]
#   <Spell.dbc>             3.3.5a Spell.dbc: the client's own, or one another module's script
#                           already patched (the files may be the outputs: they're read first)
#   <SkillLineAbility.dbc>  3.3.5a SkillLineAbility.dbc, e.g. the AzerothCore data dir's
#                           dbc/SkillLineAbility.dbc
#   [output dir]            default: ./DBFilesClient (ready to pack into an MPQ)
#
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    sed -n '7,13p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
fi

python3 - "$1" "$2" "${3:-DBFilesClient}" <<'PY'
import os, struct, sys

spell_src, skill_src, out_dir = sys.argv[1:4]

# Must match src/ForeverPaladin.cpp and the defaults in conf/mod_forever_paladin.conf.dist.
# If you change those settings, change these and run the script again.
SPELL_HOLY_STRIKE = 13953
COOLDOWN_MS = 12000
MANA_COST_PERCENT = 5
ATTACK_POWER_PERCENT = 20
SPELL_POWER_PERCENT = 20

DESCRIPTION = (
    "Your next melee attack deals Holy damage equal to your weapon damage plus "
    f"{ATTACK_POWER_PERCENT}% of your attack power and {SPELL_POWER_PERCENT}% of your spell power."
)

SKILL_HOLY = 594      # SkillLine.dbc: Holy, the paladin spellbook tab
CLASS_MASK_PALADIN = 2


def read_dbc(path, field_count):
    with open(path, "rb") as f:
        data = f.read()
    magic, records, fields, record_size, string_size = struct.unpack_from("<4s4I", data, 0)
    if magic != b"WDBC" or fields != field_count or record_size != field_count * 4:
        sys.exit(f"{path}: not a 3.3.5a file (magic={magic!r} fields={fields} recordSize={record_size})")
    rows = [list(struct.unpack_from(f"<{fields}I", data, 20 + i * record_size)) for i in range(records)]
    strings = bytearray(data[20 + records * record_size:])
    assert len(strings) == string_size
    return rows, strings


def write_dbc(path, rows, strings, field_count):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<4s4I", b"WDBC", len(rows), field_count, field_count * 4, len(strings)))
        for row in rows:
            f.write(struct.pack(f"<{field_count}I", *row))
        f.write(strings)


def add_string(strings, text):
    """Offset of text in the string block, appending it if it isn't there yet."""
    encoded = text.encode("utf-8") + b"\0"
    at = strings.find(encoded)
    while at > 0 and strings[at - 1] != 0:  # must be a whole string, not the tail of another
        at = strings.find(encoded, at + 1)
    if at >= 0:
        return at
    at = len(strings)
    strings.extend(encoded)
    return at


# --- Spell.dbc: cooldown, mana cost and description ----------------------------------------
SPELL_FIELDS = 234
RECOVERY_TIME = 29        # m_recoveryTime
MANA_COST = 42            # m_manaCost
DESCRIPTION_ENUS = 170    # m_description_lang[0]
MANA_COST_PCT = 204       # m_manaCostPct

spell_rows, spell_strings = read_dbc(spell_src, SPELL_FIELDS)
holy_strike = next((row for row in spell_rows if row[0] == SPELL_HOLY_STRIKE), None)
if holy_strike is None:
    sys.exit(f"{spell_src}: spell {SPELL_HOLY_STRIKE} not found")

holy_strike[RECOVERY_TIME] = COOLDOWN_MS
holy_strike[MANA_COST] = 0
holy_strike[MANA_COST_PCT] = MANA_COST_PERCENT
holy_strike[DESCRIPTION_ENUS] = add_string(spell_strings, DESCRIPTION)
print(f"  Spell {SPELL_HOLY_STRIKE}: {COOLDOWN_MS // 1000} sec cooldown, {MANA_COST_PERCENT}% of base mana, new description")

# --- SkillLineAbility.dbc: put Holy Strike in the Holy tab ------------------------------------
SKILL_FIELDS = 14
# ID, SkillLine, Spell, RaceMask, ClassMask, ExcludeRace, ExcludeClass, MinSkillLineRank,
# SupercededBySpell, AcquireMethod, TrivialSkillLineRankHigh, TrivialSkillLineRankLow,
# CharacterPoints[2]

skill_rows, skill_strings = read_dbc(skill_src, SKILL_FIELDS)
existing = [row for row in skill_rows if row[2] == SPELL_HOLY_STRIKE]
if existing:
    for row in existing:
        row[1] = SKILL_HOLY
        row[4] = CLASS_MASK_PALADIN
    print(f"  SkillLineAbility {existing[0][0]}: Holy Strike already listed, set to the Holy tab")
else:
    new_id = max(row[0] for row in skill_rows) + 1
    skill_rows.append([new_id, SKILL_HOLY, SPELL_HOLY_STRIKE, 0, CLASS_MASK_PALADIN, 0, 0, 1, 0, 0, 0, 0, 0, 0])
    print(f"  SkillLineAbility {new_id}: Holy Strike added to the Holy tab")

write_dbc(os.path.join(out_dir, "Spell.dbc"), spell_rows, spell_strings, SPELL_FIELDS)
write_dbc(os.path.join(out_dir, "SkillLineAbility.dbc"), skill_rows, skill_strings, SKILL_FIELDS)
print(f"Wrote {out_dir}/Spell.dbc and {out_dir}/SkillLineAbility.dbc")
PY

cat <<'EOF'

Next:
  1. Pack both files into a client patch MPQ as DBFilesClient\Spell.dbc and
     DBFilesClient\SkillLineAbility.dbc. Only the newest MPQ's Spell.dbc is used, so start from
     the Spell.dbc your current patch already ships (profession and hearthstone changes) and
     put the result back in that same patch.
  2. Players who get the new patch should delete their Cache/ folder.
EOF
