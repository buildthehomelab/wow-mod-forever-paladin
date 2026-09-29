#!/usr/bin/env python3
"""
mod-forever-paladin: write the module's client changes into a 3.3.5a (12340) client's Spell.dbc,
SkillLineAbility.dbc, GlyphProperties.dbc and Item.dbc, and pack them into an MPQ.

The 3.3.5 client only casts spells in its own Spell.dbc, and a patch MPQ replaces the whole file,
so the changes have to go into the Spell.dbc your realm patch already ships (patch-P on this
server). Start from that patch so its other spell changes are kept:

    python3 build_patch.py --from-mpq patch-P.MPQ --stock-dbc dbc --out patch-P.MPQ.new
    python3 build_patch.py --dbc Spell.dbc --sla SkillLineAbility.dbc --glyph GlyphProperties.dbc \
        --item Item.dbc --out-dir DBFilesClient
    python3 build_patch.py --from-mpq patch-P.MPQ --sql [ID ...]   # the server's spell_dbc SQL

--stock-dbc is a folder with the client's own GlyphProperties.dbc and Item.dbc (a client's
DBFilesClient, or AzerothCore's data/dbc), for when the MPQ doesn't ship them yet. --sql reads the
stock rows it copies from; give it --dbc or --from-mpq, and spell ids to print only those.

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
- Glyph of Seal of Fury: 90084 is the glyph (a copy of Glyph of Seal of Command, 54925, as a
  plain dummy aura; the server does the work), 90085 the glyph item's spell (a copy of 55109) and
  90086 the Inscription recipe (a copy of 57033). GlyphProperties.dbc gets glyph 912, a major
  glyph, and SkillLineAbility.dbc puts the recipe in Inscription. The item is 37550, Blizzard's
  unused "Deprecated Test Glyph 2"; its Item.dbc row becomes a paladin glyph with Glyph of Seal
  of Command's icon.

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

SPELL_GLYPH_OF_SEAL_OF_FURY = 90084
SPELL_GLYPH_OF_SEAL_OF_FURY_ITEM = 90085
SPELL_GLYPH_OF_SEAL_OF_FURY_RECIPE = 90086
GLYPH_OF_SEAL_OF_FURY = 912            # GlyphProperties.dbc row
ITEM_GLYPH_OF_SEAL_OF_FURY = 37550     # "Deprecated Test Glyph 2", unobtainable
GLYPH_MANA_PERCENT = 8

# Glyph of Seal of Command: the glyph, its item's spell, the recipe, the item and its glyph row.
SPELL_GLYPH_OF_SEAL_OF_COMMAND = 54925
SPELL_GLYPH_OF_SEAL_OF_COMMAND_ITEM = 55109
SPELL_GLYPH_OF_SEAL_OF_COMMAND_RECIPE = 57033
ITEM_GLYPH_OF_SEAL_OF_COMMAND = 41094
GLYPH_OF_SEAL_OF_COMMAND = 184

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
F_EFFECT_ITEM_TYPE = 107
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
SPELL_AURA_DUMMY = 4
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
SLA_SEAL_OF_FURY = 90080       # fixed row ids, so rebuilding finds them again
SLA_GLYPH_OF_SEAL_OF_FURY_RECIPE = 90086

# --- GlyphProperties.dbc and Item.dbc --------------------------------------------------------
GLYPH_FIELDS = 4               # ID, SpellID, GlyphSlotFlags, SpellIconID
ITEM_FIELDS = 8                # ID, Class, Subclass, SoundOverrideSubclass, Material, DisplayInfoID,
                               # InventoryType, SheatheType

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
    SPELL_GLYPH_OF_SEAL_OF_FURY: ("Glyph of Seal of Fury",
        f"You gain {GLYPH_MANA_PERCENT}% of your base mana each time you use a Judgement with Seal of "
        "Fury active.", None),
    SPELL_GLYPH_OF_SEAL_OF_FURY_ITEM: ("Glyph of Seal of Fury",
        f"You gain {GLYPH_MANA_PERCENT}% of your base mana each time you use a Judgement with Seal of "
        "Fury active.", None),
    SPELL_GLYPH_OF_SEAL_OF_FURY_RECIPE: ("Glyph of Seal of Fury", None, None),
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

    glyph = list(find(rows, SPELL_GLYPH_OF_SEAL_OF_COMMAND))
    glyph[F_ID] = SPELL_GLYPH_OF_SEAL_OF_FURY
    glyph[F_PROC_FLAGS] = 0
    glyph[F_PROC_CHANCE] = 101
    clear_effect(glyph, 0)                                          # it triggered 68082 on Judgement of Command
    glyph[F_EFFECT] = SPELL_EFFECT_APPLY_AURA
    glyph[F_EFFECT_AURA] = SPELL_AURA_DUMMY
    glyph[F_EFFECT_TARGET_A] = TARGET_UNIT_CASTER
    glyph[F_EFFECT_DIE_SIDES] = 1
    glyph[F_EFFECT_BASE_POINTS] = GLYPH_MANA_PERCENT - 1           # only for the tooltip's $s1
    glyph[F_EFFECT_MULTIPLE_VALUE] = f32(1.0)

    glyph_item = list(find(rows, SPELL_GLYPH_OF_SEAL_OF_COMMAND_ITEM))
    glyph_item[F_ID] = SPELL_GLYPH_OF_SEAL_OF_FURY_ITEM
    glyph_item[F_EFFECT_MISC_VALUE] = GLYPH_OF_SEAL_OF_FURY          # the glyph it inscribes

    recipe = list(find(rows, SPELL_GLYPH_OF_SEAL_OF_COMMAND_RECIPE))
    recipe[F_ID] = SPELL_GLYPH_OF_SEAL_OF_FURY_RECIPE
    recipe[F_EFFECT_ITEM_TYPE] = ITEM_GLYPH_OF_SEAL_OF_FURY

    return [seal, judgement, damage, ward, glyph, glyph_item, recipe]


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
    print(f"{dst}: Holy Strike (instant), Seal of Fury and its glyph ({', '.join(str(s[F_ID]) for s in added)}), "
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

    # The recipe: Inscription, learned and skilled up like Glyph of Seal of Command's.
    stock = next((r for r in rows if r[2] == SPELL_GLYPH_OF_SEAL_OF_COMMAND_RECIPE), None)
    if stock is None:
        sys.exit(f"{src}: no row for {SPELL_GLYPH_OF_SEAL_OF_COMMAND_RECIPE}")
    recipe = list(stock)
    recipe[0] = SLA_GLYPH_OF_SEAL_OF_FURY_RECIPE
    recipe[2] = SPELL_GLYPH_OF_SEAL_OF_FURY_RECIPE
    rows = [r for r in rows if r[0] != SLA_GLYPH_OF_SEAL_OF_FURY_RECIPE and r[2] != SPELL_GLYPH_OF_SEAL_OF_FURY_RECIPE]
    rows.append(recipe)

    write_dbc(dst, rows, strings, SLA_FIELDS)
    print(f"{dst}: Holy Strike in the Holy tab, Seal of Fury in the Protection tab, "
          "Glyph of Seal of Fury in Inscription")


def copy_row(src, dst, field_count, stock_id, new_id, change, what):
    """Replace row new_id with a copy of row stock_id, then change it."""
    rows, strings = read_dbc(src, field_count)
    stock = next((r for r in rows if r[0] == stock_id), None)
    if stock is None:
        sys.exit(f"{src}: no row {stock_id}")
    row = list(stock)
    row[0] = new_id
    change(row)
    rows = [r for r in rows if r[0] != new_id] + [row]
    write_dbc(dst, rows, strings, field_count)
    print(f"{dst}: {what}")


def patch_glyph_dbc(src, dst):
    def change(row):
        row[1] = SPELL_GLYPH_OF_SEAL_OF_FURY      # same slot type (major) and rune icon as Seal of Command's
    copy_row(src, dst, GLYPH_FIELDS, GLYPH_OF_SEAL_OF_COMMAND, GLYPH_OF_SEAL_OF_FURY, change,
             f"glyph {GLYPH_OF_SEAL_OF_FURY}, Glyph of Seal of Fury")


def patch_item_dbc(src, dst):
    # Class, subclass, material and icon of Glyph of Seal of Command. The server must agree
    # (item_dbc in the SQL), or it resets item_template to the stock values.
    copy_row(src, dst, ITEM_FIELDS, ITEM_GLYPH_OF_SEAL_OF_COMMAND, ITEM_GLYPH_OF_SEAL_OF_FURY, lambda row: None,
             f"item {ITEM_GLYPH_OF_SEAL_OF_FURY}, Glyph of Seal of Fury")


# --- SQL ------------------------------------------------------------------------------------

def sql_value(field, value):
    if field in STRING_FIELDS:
        return "''"
    if field in FLOAT_FIELDS:
        return repr(round(struct.unpack("<f", struct.pack("<I", value))[0], 6))
    return str(struct.unpack("<i", struct.pack("<I", value))[0])


def print_sql(dbc, ids):
    rows, _ = read_dbc(dbc, FIELDS)
    added = [s for s in new_spells(rows) if not ids or s[F_ID] in ids]
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


def mpq_or_stock(files, name, stock_dir, folder):
    """The MPQ's copy of DBFilesClient\\<name>, or else the stock file, added to the MPQ."""
    found = mpq_file(files, f"DBFilesClient\\{name}")
    if found:
        return found
    stock = os.path.join(stock_dir, name) if stock_dir else None
    if not stock or not os.path.exists(stock):
        sys.exit(f"the MPQ has no {name}; give --stock-dbc a folder with the client's own {name}")
    dst = os.path.join(folder, "DBFilesClient", name)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copyfile(stock, dst)
    files.append((dst, f"DBFilesClient\\{name}"))
    return files[-1]


def build_mpq(src_mpq, out, stock_dir):
    folder = tempfile.mkdtemp(prefix="forever-paladin-")
    try:
        files = extract_all(src_mpq, folder)
        spell = mpq_file(files, "DBFilesClient\\Spell.dbc")
        sla = mpq_file(files, "DBFilesClient\\SkillLineAbility.dbc")
        if spell is None or sla is None:
            sys.exit(f"{src_mpq} needs DBFilesClient\\Spell.dbc and SkillLineAbility.dbc; "
                     "use --dbc and --sla with the client's own files")
        glyph = mpq_or_stock(files, "GlyphProperties.dbc", stock_dir, folder)
        item = mpq_or_stock(files, "Item.dbc", stock_dir, folder)
        patch_spell_dbc(spell[0], spell[0])
        patch_sla_dbc(sla[0], sla[0])
        patch_glyph_dbc(glyph[0], glyph[0])
        patch_item_dbc(item[0], item[0])
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
    parser.add_argument("--stock-dbc", help="folder with the stock GlyphProperties.dbc and Item.dbc, "
                        "for an MPQ that doesn't ship them")
    parser.add_argument("--dbc", help="a Spell.dbc to start from")
    parser.add_argument("--sla", help="a SkillLineAbility.dbc to start from (with --dbc)")
    parser.add_argument("--glyph", help="a GlyphProperties.dbc to start from (with --dbc)")
    parser.add_argument("--item", help="an Item.dbc to start from (with --dbc)")
    parser.add_argument("--out-dir", help="where to write the DBCs (with --dbc, --sla, --glyph and --item)")
    parser.add_argument("--sql", nargs="*", type=int, metavar="ID",
                        help="print the server's spell_dbc SQL, for these new spells or all of them")
    args = parser.parse_args()

    if args.sql is not None:
        dbc = args.dbc
        folder = None
        if not dbc and args.from_mpq:
            folder = tempfile.mkdtemp(prefix="forever-paladin-")
            dbc = mpq_file(extract_all(args.from_mpq, folder), "DBFilesClient\\Spell.dbc")[0]
        if not dbc:
            sys.exit("--sql needs --dbc or --from-mpq")
        print_sql(dbc, set(args.sql))
        if folder:
            shutil.rmtree(folder)
    elif args.from_mpq and args.out:
        build_mpq(args.from_mpq, args.out, args.stock_dbc)
    elif args.dbc and args.sla and args.glyph and args.item and args.out_dir:
        os.makedirs(args.out_dir, exist_ok=True)
        patch_spell_dbc(args.dbc, os.path.join(args.out_dir, "Spell.dbc"))
        patch_sla_dbc(args.sla, os.path.join(args.out_dir, "SkillLineAbility.dbc"))
        patch_glyph_dbc(args.glyph, os.path.join(args.out_dir, "GlyphProperties.dbc"))
        patch_item_dbc(args.item, os.path.join(args.out_dir, "Item.dbc"))
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
