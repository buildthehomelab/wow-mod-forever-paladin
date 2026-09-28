#!/usr/bin/env python3
"""
mod-forever-paladin: write the module's client changes into a 3.3.5a (12340) client's Spell.dbc
and SkillLineAbility.dbc, and pack them into an MPQ.

The 3.3.5 client only casts spells in its own Spell.dbc, and a patch MPQ replaces the whole file,
so the changes have to go into the Spell.dbc your realm patch already ships (patch-P on this
server). Start from that patch so its other spell changes are kept:

    python3 build_patch.py --from-mpq patch-P.MPQ --out patch-P.MPQ.new
    python3 build_patch.py --dbc Spell.dbc --sla SkillLineAbility.dbc --out-dir DBFilesClient
    python3 build_patch.py --sql                     # print the server's spell_dbc SQL

--sql reads the stock rows it copies from; give it --dbc or --from-mpq.

What it changes:

- Holy Strike (13953): an instant strike on the global cooldown instead of a next-swing attack
  (the server makes the same change), 12 sec cooldown, 5% of base mana, a tooltip that says what
  it does, and a place in the Holy tab of the spellbook.
- Seal of Fury: four new spells. 90080 is the seal (a copy of Seal of Righteousness, 21084),
  90081 Judgement of Fury (a copy of Judgement, 54158, that also taunts for 4 sec), 90082 its
  Holy damage on melee hits (a copy of 25742) and 90083 Fury Ward, the absorb (a copy of Sacred
  Shield's absorb, 58597). Seal of Fury goes in the Protection tab.
- Blessings and Greater Blessings: tooltips say 1 hour (the duration index they point at).
- Judgement of Light and Wisdom, Heart of the Crusader and Judgements of the Just: tooltips say
  40 sec. Judgement of Justice keeps 20 sec.
- Glyph of Blessing of Might and Glyph of Blessing of Wisdom: new tooltips (50% cheaper instead
  of 20 more minutes on yourself).

The client only uses these for tooltips, the spellbook and the combat log; the server decides
what the spells really do. Running it again gives the same result, so it's safe to rebuild.

Packing needs StormLib (libstorm). Point STORMLIB at it if it isn't /usr/local/lib/libstorm.dylib.

Released under the MIT License.
"""

import argparse
import ctypes
import os
import shutil
import struct
import sys
import tempfile

# Must match src/ForeverPaladin.cpp, the SQL and the defaults in conf/mod_forever_paladin.conf.dist.
# If you change those settings, change these and build the patch again.
SPELL_HOLY_STRIKE = 13953
HOLY_STRIKE_COOLDOWN_MS = 12000
HOLY_STRIKE_MANA_COST_PERCENT = 5
HOLY_STRIKE_WEAPON = 0.4
HOLY_STRIKE_PER_LEVEL = 1.8
HOLY_STRIKE_SP = 0.429

SPELL_SEAL_OF_FURY = 90080
SPELL_JUDGEMENT_OF_FURY = 90081
SPELL_SEAL_OF_FURY_DAMAGE = 90082
SPELL_FURY_WARD = 90083
SEAL_OF_FURY_LEVEL = 10
FURY_WARD_PERCENT = 50
TAUNT_SECONDS = 4

GLYPH_COST_REDUCTION_PERCENT = 50

# SpellDuration.dbc
DURATION_4_SEC = 35
DURATION_10_SEC = 1
DURATION_40_SEC = 64
DURATION_1_HOUR = 42

BLESSINGS = [
    19740, 19834, 19835, 19836, 19837, 19838, 25291, 27140, 48931, 48932,  # Blessing of Might
    19742, 19850, 19852, 19853, 19854, 25290, 27142, 48935, 48936,         # Blessing of Wisdom
    20217,                                                                 # Blessing of Kings
    20911,                                                                 # Blessing of Sanctuary
    25782, 25916, 27141, 48933, 48934,                                     # Greater Blessing of Might
    25894, 25918, 27143, 48937, 48938,                                     # Greater Blessing of Wisdom
    25898,                                                                 # Greater Blessing of Kings
    25899,                                                                 # Greater Blessing of Sanctuary
]

JUDGEMENT_DEBUFFS = [
    20185,                # Judgement of Light
    20186,                # Judgement of Wisdom
    21183, 54498, 54499,  # Heart of the Crusader
    68055,                # Judgements of the Just
]

# Glyph aura, then the glyph item's "Use:" spell, which shows the same text.
GLYPHS = {
    57958: "Might", 58243: "Might",
    57979: "Wisdom", 58244: "Wisdom",
}

# --- Spell.dbc layout (3.3.5a, build 12340) -------------------------------------------------
FIELDS = 234
F_ID = 0
F_ATTRIBUTES = 4
SPELL_ATTR0_ON_NEXT_SWING_NO_DAMAGE = 0x4
SPELL_ATTR0_ON_NEXT_SWING = 0x400
F_RECOVERY_TIME = 29
F_PROC_FLAGS = 34
F_PROC_CHANCE = 35
F_BASE_LEVEL = 38
F_SPELL_LEVEL = 39
F_DURATION_INDEX = 40
F_MANA_COST = 42
F_EFFECT = 71             # 3 each from here on
F_EFFECT_DIE_SIDES = 74
F_EFFECT_BASE_POINTS = 80
F_EFFECT_TARGET_A = 86
F_EFFECT_TARGET_B = 89
F_EFFECT_RADIUS = 92
F_EFFECT_AURA = 95
F_EFFECT_AMPLITUDE = 98
F_EFFECT_MULTIPLE_VALUE = 101
F_EFFECT_MISC_VALUE = 110
F_EFFECT_TRIGGER_SPELL = 116
F_EFFECT_CLASS_MASK = 122  # 3 words per effect
F_SPELL_ICON = 133
F_NAME = 136              # 16 locale strings, then a flags field
F_NAME_SUBTEXT = 153
F_DESCRIPTION = 170
F_AURA_DESCRIPTION = 187
F_MANA_COST_PCT = 204
F_START_RECOVERY_CATEGORY = 205
F_START_RECOVERY_TIME = 206
GCD_CATEGORY = 133             # the normal global cooldown, as on Crusader Strike
GCD_TIME = 1500
F_FAMILY_FLAGS = 209      # 3 words
F_EFFECT_BONUS = 229      # 3 floats

STRING_FIELDS = [F_NAME + i for i in range(16)] + [F_NAME_SUBTEXT + i for i in range(16)] \
    + [F_DESCRIPTION + i for i in range(16)] + [F_AURA_DESCRIPTION + i for i in range(16)]
# Speed, EffectRealPointsPerLevel, EffectPointsCombo, EffectMultipleValue, DmgMultiplier,
# EffectBonusMultiplier: the float columns of AzerothCore's spell_dbc table.
FLOAT_FIELDS = {47, 77, 78, 79, 101, 102, 103, 119, 120, 121, 216, 217, 218, 229, 230, 231}

SPELL_EFFECT_ATTACK_ME = 114
SPELL_EFFECT_APPLY_AURA = 6
SPELL_AURA_MOD_TAUNT = 11
TARGET_UNIT_CASTER = 1
TARGET_UNIT_TARGET_ENEMY = 6

SEAL_FAMILY_FLAG = 0x08000000  # word 0; a seal bit only Benediction and seal cost effects use
ICON_SEAL_OF_WRATH = 2143      # the NPC Seal of Wrath's icon; no player seal uses it

# --- SkillLineAbility.dbc -------------------------------------------------------------------
SLA_FIELDS = 14
# ID, SkillLine, Spell, RaceMask, ClassMask, ExcludeRace, ExcludeClass, MinSkillLineRank,
# SupercededBySpell, AcquireMethod, TrivialSkillLineRankHigh, TrivialSkillLineRankLow,
# CharacterPoints[2]
SKILL_HOLY = 594
SKILL_PROTECTION = 267
CLASS_MASK_PALADIN = 2
SLA_SEAL_OF_FURY = 90080       # fixed row id, so rebuilding finds it again

# --- Texts ----------------------------------------------------------------------------------
SOF_HIT = "${$MWS*(0.022*$AP+0.044*$SPH)}"
SOF_JUDGEMENT = "${1+0.2*$AP+0.32*$SPH}"

TEXTS = {
    SPELL_HOLY_STRIKE: (None,
        "An instant strike that deals "
        f"${{{HOLY_STRIKE_WEAPON}*$mw+{HOLY_STRIKE_PER_LEVEL}*$PL+{HOLY_STRIKE_SP}*$SPH}} to "
        f"${{{HOLY_STRIKE_WEAPON}*$MW+{HOLY_STRIKE_PER_LEVEL}*$PL+{HOLY_STRIKE_SP}*$SPH}} Holy damage.",
        None),
    SPELL_SEAL_OF_FURY: ("Seal of Fury",
        f"Fills the Paladin with holy fury for $d, granting each melee attack {SOF_HIT} additional "
        f"Holy damage. While a shield is equipped, each of these hits also grants a Fury Ward that "
        f"absorbs damage equal to {FURY_WARD_PERCENT}% of the Holy damage dealt.  Only one Seal can "
        "be active on the Paladin at any one time.\n\n"
        f"Unleashing this Seal's energy will cause {SOF_JUDGEMENT} Holy damage to an enemy and "
        f"taunt it to attack you for {TAUNT_SECONDS} sec.",
        f"Melee attacks cause an additional {SOF_HIT} Holy damage and, with a shield equipped, "
        "grant a Fury Ward."),
    SPELL_JUDGEMENT_OF_FURY: ("Judgement of Fury",
        f"Deals {SOF_JUDGEMENT} Holy damage and taunts the target to attack you for "
        f"{TAUNT_SECONDS} sec.",
        "Taunted."),
    SPELL_SEAL_OF_FURY_DAMAGE: ("Seal of Fury", "", ""),
    SPELL_FURY_WARD: ("Fury Ward",
        "Absorbs damage. Only the strongest Fury Ward counts; a new one replaces a weaker one.",
        "Absorbs damage."),
}

for glyph_id, blessing in GLYPHS.items():
    TEXTS[glyph_id] = (None,
        f"Reduces the mana cost of your Blessing of {blessing} and Greater Blessing of {blessing} "
        f"spells by {GLYPH_COST_REDUCTION_PERCENT}%.", None)


# --- DBC helpers ----------------------------------------------------------------------------

def read_dbc(path, field_count):
    with open(path, "rb") as f:
        data = f.read()
    magic, count, fields, size, strsize = struct.unpack_from("<4s4I", data, 0)
    if magic != b"WDBC" or fields != field_count or size != field_count * 4:
        sys.exit(f"{path}: not a 3.3.5a file ({fields} fields)")
    rows = [list(struct.unpack_from(f"<{fields}I", data, 20 + i * size)) for i in range(count)]
    strings = bytearray(data[20 + count * size:20 + count * size + strsize])
    return rows, strings


def write_dbc(path, rows, strings, field_count):
    rows = sorted(rows, key=lambda r: r[0])
    with open(path, "wb") as f:
        f.write(struct.pack("<4s4I", b"WDBC", len(rows), field_count, field_count * 4, len(strings)))
        for row in rows:
            f.write(struct.pack(f"<{field_count}I", *row))
        f.write(strings)


def add_string(strings, text):
    """Offset of text in the string block, appending it if it isn't there yet."""
    if not text:
        return 0
    encoded = text.encode("utf-8") + b"\0"
    at = strings.find(encoded)
    while at > 0 and strings[at - 1] != 0:  # must be a whole string, not the tail of another
        at = strings.find(encoded, at + 1)
    if at >= 0:
        return at
    at = len(strings)
    strings.extend(encoded)
    return at


def f32(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def i32(value):
    return value & 0xFFFFFFFF


def find(rows, spell_id):
    row = next((r for r in rows if r[F_ID] == spell_id), None)
    if row is None:
        sys.exit(f"spell {spell_id} not found in Spell.dbc")
    return row


def clear_effect(row, e):
    for base in (F_EFFECT, F_EFFECT_DIE_SIDES, F_EFFECT_BASE_POINTS, F_EFFECT_TARGET_A,
                 F_EFFECT_TARGET_B, F_EFFECT_RADIUS, F_EFFECT_AURA, F_EFFECT_AMPLITUDE,
                 F_EFFECT_MULTIPLE_VALUE, F_EFFECT_MISC_VALUE, F_EFFECT_TRIGGER_SPELL, F_EFFECT_BONUS):
        row[base + e] = 0
    row[77 + e] = 0           # EffectRealPointsPerLevel
    row[83 + e] = 0           # EffectMechanic
    row[104 + e] = 0          # EffectChainTarget
    row[107 + e] = 0          # EffectItemType
    row[113 + e] = 0          # EffectMiscValueB
    row[119 + e] = 0          # EffectPointsPerComboPoint
    for w in range(3):
        row[F_EFFECT_CLASS_MASK + e * 3 + w] = 0


def new_spells(rows):
    """The four Seal of Fury spells, copied from the stock spells named in the docstring."""
    seal = list(find(rows, 21084))
    seal[F_ID] = SPELL_SEAL_OF_FURY
    seal[F_BASE_LEVEL] = seal[F_SPELL_LEVEL] = SEAL_OF_FURY_LEVEL
    seal[F_SPELL_ICON] = ICON_SEAL_OF_WRATH
    seal[F_FAMILY_FLAGS:F_FAMILY_FLAGS + 3] = [SEAL_FAMILY_FLAG, 0, 0]
    clear_effect(seal, 1)                                          # Seal of Righteousness' 0% cost mod
    seal[F_EFFECT_BASE_POINTS + 2] = i32(SPELL_JUDGEMENT_OF_FURY - 1)  # the Judgement it casts

    judgement = list(find(rows, 54158))
    judgement[F_ID] = SPELL_JUDGEMENT_OF_FURY
    judgement[F_DURATION_INDEX] = DURATION_4_SEC
    judgement[F_SPELL_ICON] = ICON_SEAL_OF_WRATH
    clear_effect(judgement, 1)
    judgement[F_EFFECT + 1] = SPELL_EFFECT_ATTACK_ME
    judgement[F_EFFECT_TARGET_A + 1] = TARGET_UNIT_TARGET_ENEMY
    judgement[F_EFFECT_MULTIPLE_VALUE + 1] = f32(1.0)
    clear_effect(judgement, 2)
    judgement[F_EFFECT + 2] = SPELL_EFFECT_APPLY_AURA
    judgement[F_EFFECT_AURA + 2] = SPELL_AURA_MOD_TAUNT
    judgement[F_EFFECT_TARGET_A + 2] = TARGET_UNIT_TARGET_ENEMY
    judgement[F_EFFECT_MULTIPLE_VALUE + 2] = f32(1.0)

    damage = list(find(rows, 25742))
    damage[F_ID] = SPELL_SEAL_OF_FURY_DAMAGE
    damage[F_SPELL_ICON] = ICON_SEAL_OF_WRATH
    damage[F_FAMILY_FLAGS:F_FAMILY_FLAGS + 3] = [0, 0, 0]          # not Seal of Righteousness for talents

    ward = list(find(rows, 58597))
    ward[F_ID] = SPELL_FURY_WARD
    ward[F_BASE_LEVEL] = ward[F_SPELL_LEVEL] = 1
    ward[F_DURATION_INDEX] = DURATION_10_SEC
    ward[F_SPELL_ICON] = ICON_SEAL_OF_WRATH
    ward[F_FAMILY_FLAGS:F_FAMILY_FLAGS + 3] = [0, 0, 0]
    ward[F_EFFECT_TARGET_A] = TARGET_UNIT_CASTER
    ward[F_EFFECT_BASE_POINTS] = 0
    ward[F_EFFECT_BONUS] = f32(0.0)                                 # the server sets the amount
    clear_effect(ward, 1)                                           # Sacred Shield's Flash of Light crit

    return [seal, judgement, damage, ward]


def set_texts(row, strings):
    name, description, aura_description = TEXTS.get(row[F_ID], (None, None, None))
    if name is not None:
        for i in range(16):
            row[F_NAME + i] = 0
            row[F_NAME_SUBTEXT + i] = 0
        row[F_NAME] = add_string(strings, name)
    if description is not None:
        for i in range(16):
            row[F_DESCRIPTION + i] = 0
        row[F_DESCRIPTION] = add_string(strings, description)
    if aura_description is not None:
        for i in range(16):
            row[F_AURA_DESCRIPTION + i] = 0
        row[F_AURA_DESCRIPTION] = add_string(strings, aura_description)


def patch_spell_dbc(src, dst):
    rows, strings = read_dbc(src, FIELDS)
    by_id = {r[F_ID]: r for r in rows}

    holy_strike = find(rows, SPELL_HOLY_STRIKE)
    holy_strike[F_ATTRIBUTES] &= ~(SPELL_ATTR0_ON_NEXT_SWING | SPELL_ATTR0_ON_NEXT_SWING_NO_DAMAGE)
    holy_strike[F_START_RECOVERY_CATEGORY] = GCD_CATEGORY
    holy_strike[F_START_RECOVERY_TIME] = GCD_TIME
    holy_strike[F_RECOVERY_TIME] = HOLY_STRIKE_COOLDOWN_MS
    holy_strike[F_MANA_COST] = 0
    holy_strike[F_MANA_COST_PCT] = HOLY_STRIKE_MANA_COST_PERCENT
    set_texts(holy_strike, strings)

    # Built from stock rows, so read them before replacing anything.
    added = new_spells(rows)
    rows = [r for r in rows if r[F_ID] not in {s[F_ID] for s in added}]
    for spell in added:
        set_texts(spell, strings)
        rows.append(spell)

    for spell_id in BLESSINGS:
        find(rows, spell_id)[F_DURATION_INDEX] = DURATION_1_HOUR
    for spell_id in JUDGEMENT_DEBUFFS:
        find(rows, spell_id)[F_DURATION_INDEX] = DURATION_40_SEC
    for glyph_id in GLYPHS:
        set_texts(find(rows, glyph_id), strings)

    write_dbc(dst, rows, strings, FIELDS)
    print(f"{dst}: Holy Strike (instant), Seal of Fury ({', '.join(str(s[F_ID]) for s in added)}), "
          f"{len(BLESSINGS)} blessings at 1 hour, {len(JUDGEMENT_DEBUFFS)} Judgement debuffs at 40 sec, "
          f"{len(GLYPHS)} glyph tooltips")


def patch_sla_dbc(src, dst):
    rows, strings = read_dbc(src, SLA_FIELDS)

    holy_strike = [r for r in rows if r[2] == SPELL_HOLY_STRIKE]
    if holy_strike:
        for row in holy_strike:
            row[1] = SKILL_HOLY
            row[4] = CLASS_MASK_PALADIN
    else:
        rows.append([max(r[0] for r in rows) + 1, SKILL_HOLY, SPELL_HOLY_STRIKE, 0, CLASS_MASK_PALADIN,
                     0, 0, 1, 0, 0, 0, 0, 0, 0])

    rows = [r for r in rows if r[0] != SLA_SEAL_OF_FURY and r[2] != SPELL_SEAL_OF_FURY]
    rows.append([SLA_SEAL_OF_FURY, SKILL_PROTECTION, SPELL_SEAL_OF_FURY, 0, CLASS_MASK_PALADIN,
                 0, 0, 1, 0, 0, 0, 0, 0, 0])

    write_dbc(dst, rows, strings, SLA_FIELDS)
    print(f"{dst}: Holy Strike in the Holy tab, Seal of Fury in the Protection tab")


# --- SQL ------------------------------------------------------------------------------------

def sql_value(field, value):
    if field in STRING_FIELDS:
        return "''"
    if field in FLOAT_FIELDS:
        return repr(round(struct.unpack("<f", struct.pack("<I", value))[0], 6))
    return str(struct.unpack("<i", struct.pack("<I", value))[0])


def print_sql(dbc):
    rows, _ = read_dbc(dbc, FIELDS)
    added = new_spells(rows)
    print(f"DELETE FROM `spell_dbc` WHERE `ID` IN ({', '.join(str(s[F_ID]) for s in added)});")
    print("INSERT INTO `spell_dbc` VALUES")
    lines = []
    for row in added:
        values = [sql_value(i, v) for i, v in enumerate(row)]
        values[F_NAME] = "'" + TEXTS[row[F_ID]][0].replace("'", "''") + "'"
        lines.append("(" + ", ".join(values) + ")")
    print(",\n".join(lines) + ";")


# --- MPQ ------------------------------------------------------------------------------------

def stormlib():
    lib = ctypes.CDLL(os.environ.get("STORMLIB", "/usr/local/lib/libstorm.dylib"))
    handle = ctypes.c_void_p
    lib.SFileOpenArchive.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(handle)]
    lib.SFileCreateArchive.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(handle)]
    lib.SFileExtractFile.argtypes = [handle, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
    lib.SFileAddFileEx.argtypes = [handle, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
    lib.SFileCloseArchive.argtypes = [handle]

    class FindData(ctypes.Structure):
        _fields_ = [("cFileName", ctypes.c_char * 1024), ("szPlainName", ctypes.c_char_p),
                    ("dwHashIndex", ctypes.c_uint), ("dwBlockIndex", ctypes.c_uint),
                    ("dwFileSize", ctypes.c_uint), ("dwFileFlags", ctypes.c_uint),
                    ("dwCompSize", ctypes.c_uint), ("dwFileTimeLo", ctypes.c_uint),
                    ("dwFileTimeHi", ctypes.c_uint), ("lcLocale", ctypes.c_uint)]

    lib.SFileFindFirstFile.argtypes = [handle, ctypes.c_char_p, ctypes.POINTER(FindData), ctypes.c_char_p]
    lib.SFileFindFirstFile.restype = handle
    lib.SFileFindNextFile.argtypes = [handle, ctypes.POINTER(FindData)]
    lib.SFileFindClose.argtypes = [handle]
    return lib, handle, FindData


def extract_all(mpq, folder):
    lib, handle, FindData = stormlib()
    h = handle()
    if not lib.SFileOpenArchive(mpq.encode(), 0, 0x100, ctypes.byref(h)):
        sys.exit(f"can't open {mpq}")

    names = []
    found = FindData()
    search = lib.SFileFindFirstFile(h, b"*", ctypes.byref(found), None)
    while search:
        names.append(found.cFileName.decode())
        if not lib.SFileFindNextFile(search, ctypes.byref(found)):
            break
    if search:
        lib.SFileFindClose(search)

    files = []
    for name in names:
        if name in ("(listfile)", "(attributes)", "(signature)"):
            continue
        dst = os.path.join(folder, *name.split("\\"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if not lib.SFileExtractFile(h, name.encode(), dst.encode(), 0):
            sys.exit(f"can't extract {name}")
        files.append((dst, name))
    lib.SFileCloseArchive(h)
    return files


def pack(out, files):
    lib, handle, _ = stormlib()
    if os.path.exists(out):
        os.remove(out)
    h = handle()
    # MPQ v1 with a listfile and attributes; each file zlib-compressed, like the realm's patches.
    if not lib.SFileCreateArchive(out.encode(), 0x00300000, max(16, len(files) * 2), ctypes.byref(h)):
        sys.exit(f"can't create {out}")
    for src, name in files:
        if not lib.SFileAddFileEx(h, src.encode(), name.encode(), 0x80000200, 0x02, 0x02):
            sys.exit(f"can't add {name}")
    lib.SFileCloseArchive(h)


def mpq_file(files, name):
    return next((f for f in files if f[1].lower() == name.lower()), None)


def build_mpq(src_mpq, out):
    folder = tempfile.mkdtemp(prefix="forever-paladin-")
    try:
        files = extract_all(src_mpq, folder)
        spell = mpq_file(files, "DBFilesClient\\Spell.dbc")
        sla = mpq_file(files, "DBFilesClient\\SkillLineAbility.dbc")
        if spell is None or sla is None:
            sys.exit(f"{src_mpq} needs DBFilesClient\\Spell.dbc and SkillLineAbility.dbc; "
                     "use --dbc and --sla with the client's own files")
        patch_spell_dbc(spell[0], spell[0])
        patch_sla_dbc(sla[0], sla[0])
        # Keep the original order, with Spell.dbc last as the realm's patch-P has it.
        files.sort(key=lambda f: f[1].lower() == "dbfilesclient\\spell.dbc")
        pack(out, files)
        print(f"{out}: {', '.join(name for _, name in files)}")
    finally:
        shutil.rmtree(folder)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--from-mpq", help="patch MPQ that already ships Spell.dbc and SkillLineAbility.dbc")
    parser.add_argument("--out", help="MPQ to write (with --from-mpq)")
    parser.add_argument("--dbc", help="a Spell.dbc to start from")
    parser.add_argument("--sla", help="a SkillLineAbility.dbc to start from (with --dbc)")
    parser.add_argument("--out-dir", help="where to write both DBCs (with --dbc and --sla)")
    parser.add_argument("--sql", action="store_true", help="print the server's spell_dbc SQL")
    args = parser.parse_args()

    if args.sql:
        dbc = args.dbc
        folder = None
        if not dbc and args.from_mpq:
            folder = tempfile.mkdtemp(prefix="forever-paladin-")
            dbc = mpq_file(extract_all(args.from_mpq, folder), "DBFilesClient\\Spell.dbc")[0]
        if not dbc:
            sys.exit("--sql needs --dbc or --from-mpq")
        print_sql(dbc)
        if folder:
            shutil.rmtree(folder)
    elif args.from_mpq and args.out:
        build_mpq(args.from_mpq, args.out)
    elif args.dbc and args.sla and args.out_dir:
        os.makedirs(args.out_dir, exist_ok=True)
        patch_spell_dbc(args.dbc, os.path.join(args.out_dir, "Spell.dbc"))
        patch_sla_dbc(args.sla, os.path.join(args.out_dir, "SkillLineAbility.dbc"))
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
