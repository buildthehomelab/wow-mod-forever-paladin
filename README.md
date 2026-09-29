# Forever Paladin

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that brings WoW Forever's
paladin changes to a 3.3.5 server:

- **Holy Strike:** every paladin learns it at level 6, whatever their spec.
- **Shield Specialization's mana return:** blocks can restore 6% of base mana.
- **Seal of Fury:** a tanking seal whose Judgement taunts, and **Glyph of Seal of Fury** for mana
  on Judgement. Needs the client patch.
- **1 hour Blessings** and **40 second Judgements**, with the Blessing of Might and Wisdom glyphs
  reworked to match.

Players need the client patch for Holy Strike (it's instant, and the stock client thinks it's a
next-swing attack), Seal of Fury (four new spells) and its glyph. The mana return and the durations work
without it; the patch only updates their tooltips.

Some of WoW Forever's paladin changes are already how 3.3.5 works, so the module leaves them
alone: Judgement doesn't use up the seal, Blessing of Kings is trained (level 20), and the Fire,
Frost and Shadow Resistance Auras already reach the whole raid.

## Holy Strike

- Paladins learn **Holy Strike** at level 6. Existing paladins get it at their next login.
- It's an **instant strike** on the global cooldown, like Crusader Strike. All its damage is
  Holy, so it ignores armor.
- It deals WoW Forever's damage: **40% of weapon damage + 1.8 per level + 42.9% of your Holy
  spell power**. Forever gives 108 flat at level 60 (12 at rank 1, level 6); 1.8 per level
  matches both and makes it 144 at 80. With a 1293 weapon hit and 1200 spell power at level 80,
  that's about 1140; with 343 and 300 at level 60, about 374.
- Then it goes on a **12 second cooldown**, which shows on the action bar.
- It costs **5% of base mana**, like Crusader Strike: about 6 mana at level 6, 75 at 60 and 220
  at 80.
- It needs a melee weapon, and you have to face the target.

It isn't tied to a spec or a talent, so dual spec and talent resets don't affect it.

## Shield Specialization's mana return

In WoW Forever, Shield Specialization also gives blocks a 33% chance to restore 6% of your maximum
mana, at most once every 3 seconds. 3.3.5 has no Shield Specialization: Wrath folded its block
bonus into Redoubt. So here Redoubt carries the mana return:

- When you **block** a melee or ranged attack, you have a chance to get back **6% of your base
  mana**. That's about 8 mana at level 6, 91 at 60 and 264 at 80. WoW Forever uses max mana;
  base mana doesn't grow with Intellect, so gear doesn't make it bigger.
- The chance depends on your points in **Redoubt**: 33%, 66% or 100%. No Redoubt, no mana.
- It can happen **at most once every 3 seconds**. A block that doesn't return mana doesn't use up
  the 3 seconds.
- The combat log shows it as "You gain 264 Mana from Redoubt."

It follows your talents on its own: a talent reset or dual spec switch turns it on or off
straight away. Redoubt's tooltip doesn't mention it.

How it works: every paladin carries a hidden aura, made from an unused server-side spell stub
(67553) that the client doesn't have. A `spell_proc` row makes it proc on blocks, and its script
rolls the chance for your Redoubt rank and gives the mana.

## Seal of Fury

WoW Forever's tanking seal. Paladins learn it at **level 10**, whatever their spec, and it sits in
the Protection tab of the spellbook.

- **The seal** lasts 30 minutes and costs 14% of base mana, like the other seals. Each melee hit
  deals extra Holy damage, the same as Seal of Righteousness: weapon speed × (2.2% of attack
  power + 4.4% of Holy spell power). With a 2.6 speed weapon, 4000 attack power and 1200 spell
  power that's 366 per hit.
- **Fury Ward:** with a **shield equipped**, each of those hits also gives you an absorb worth
  **50% of the Holy damage it did** (183 in that example). Wards don't stack: a bigger one
  replaces the current one, and every hit keeps it up for 10 more seconds, so fast weapons keep it
  topped up better.
- **Judgement of Fury:** judging the seal deals Holy damage like Judgement of Righteousness (1 +
  32% of spell power + 20% of attack power) and **taunts the target for 4 seconds**. That's a
  third taunt next to Hand of Reckoning and Righteous Defense, on the Judgement cooldown.
  Judgement talents and glyphs apply to it, and it still puts your Judgement of Light, Wisdom or
  Justice on the target as usual.

Only one seal at a time, as always. WoW Forever's Improved Seal of Fury talent (mana when a ward
breaks) isn't included: adding a talent needs a bigger client patch.

The spells are new (90080 Seal of Fury, 90081 Judgement of Fury, 90082 its Holy damage, 90083
Fury Ward). The server gets them from `spell_dbc`, the client from the patch
(`tools/build_patch.py`). A player without the patch can't see or cast the seal.

### Glyph of Seal of Fury

A major glyph that does for Seal of Fury what Glyph of Seal of Command does for Seal of Command:
**each Judgement you use with Seal of Fury active gives you back 8% of your base mana** (121 at
level 60, 352 at 80). It counts when the Judgement hits; the combat log shows "You gain 352 Mana
from Glyph of Seal of Fury."

Scribes make it: **every trainer that teaches Glyph of Seal of Command teaches it too**, for the
same price and at the same Inscription skill (335), and it takes the same materials (Ethereal
Ink and Resilient Parchment, with a Virtuoso Inking Set) and skills up the same way (up to 350).

Under the hood: the glyph is spell 90084, the glyph item's spell 90085, the recipe 90086 and the
glyph slot entry 912 (GlyphProperties). The mana is credited to 90087, a visible copy of the spell
Glyph of Seal of Command uses (68082): glyph auras are hidden, and the client leaves hidden spells
out of the combat log. The item is 37550, Blizzard's unused "Deprecated Test Glyph
2": no loot, vendor, quest or recipe gives it, and the client already has an entry for it, so
the patch only changes its icon to Glyph of Seal of Command's instead of adding a new item. The
glyph's aura does nothing by itself; Judgement of Fury's script checks for it and gives the mana.

## Blessings, Judgements and glyphs

As in WoW Forever:

- **Blessings last 1 hour:** Might, Wisdom, Kings and Sanctuary, and their Greater Blessings
  (instead of 10 and 30 minutes).
- **Judgement debuffs last 40 seconds:** Judgement of Light, Judgement of Wisdom, Heart of the
  Crusader and Judgements of the Just (instead of 20). **Judgement of Justice keeps 20.**

Glyph of Blessing of Might and Glyph of Blessing of Wisdom made those blessings last 20 minutes
longer when cast on yourself, which means little once blessings last an hour. They now make
**Blessing of Might / Wisdom and their Greater Blessings 50% cheaper**, like Glyph of Blessing of
Kings already does for Kings. Glyph of Blessing of Kings is unchanged.

No talent changes a blessing's duration, so none needed changing. (Checked against Spell.dbc: the
talents that touch blessings are Improved Blessing of Might and Wisdom, which add to their
effect, and Benediction, which lowers their cost.)

The server changes all of this when it starts; players see the right durations on their buffs
with or without the patch. The patch updates the tooltips ("Lasts 60 min", "40 sec", the glyph
text). Blessings cast before the change keep the duration they were cast with.

## Where Holy Strike comes from

The spell is Holy Strike (13953), which the 3.3.5 client already has. Blizzard only ever gave it to
NPCs, like the Scarlet Crusade. No item, trainer or talent teaches it to players, so reusing it
changes nothing players could already get, and every client already has its name and icon.

The game data makes it a next-swing attack, like Heroic Strike. The module makes it instant on
the server, and the client patch does the same for the client; a client without the patch
treats the button as a next-swing attack, so it misbehaves.

NPCs that cast Holy Strike keep their damage, cooldown and 75 mana; they strike instantly too.

## Install

Clone it into your AzerothCore `modules` folder **as `mod-forever-paladin`**, without the repo's
`wow-` prefix. AzerothCore finds the module's entry point from the folder name.

```bash
cd <azerothcore>/modules
git clone https://github.com/buildthehomelab/wow-mod-forever-paladin.git mod-forever-paladin
```

Rebuild the worldserver, then copy `conf/mod_forever_paladin.conf.dist` to your config folder as
`mod_forever_paladin.conf`. The spell script binding is added to the world database on the next
start.

## Settings

| Setting | Default | What it does |
|---------|---------|--------------|
| `ForeverPaladin.HolyStrike.Enable` | `1` | Master switch. With `0`, paladins lose Holy Strike at their next login. |
| `ForeverPaladin.HolyStrike.Level` | `6` | Level at which paladins learn it. |
| `ForeverPaladin.HolyStrike.Cooldown` | `12000` | Cooldown in milliseconds. `0` for none. |
| `ForeverPaladin.HolyStrike.WeaponPercent` | `40` | Percent of weapon damage. |
| `ForeverPaladin.HolyStrike.DamagePerLevel` | `1.8` | Flat damage per paladin level. |
| `ForeverPaladin.HolyStrike.AttackPowerCoefficient` | `0` | Extra damage as a share of attack power. |
| `ForeverPaladin.HolyStrike.SpellPowerCoefficient` | `0.429` | Extra damage as a share of Holy spell power. |
| `ForeverPaladin.HolyStrike.ManaCostPercent` | `5` | Mana cost as a whole percentage of base mana. `0` keeps the flat 75. |
| `ForeverPaladin.ShieldMana.Enable` | `1` | Master switch for the mana return on block. |
| `ForeverPaladin.ShieldMana.ChanceRank1` / `2` / `3` | `33` / `66` / `100` | Chance in percent with 1, 2 or 3 points in Redoubt. |
| `ForeverPaladin.ShieldMana.BaseManaPercent` | `6` | Mana returned, as a percentage of base mana. |
| `ForeverPaladin.ShieldMana.Cooldown` | `3000` | Minimum milliseconds between two mana returns. |
| `ForeverPaladin.SealOfFury.Enable` | `1` | Master switch. With `0`, paladins lose Seal of Fury at their next login. |
| `ForeverPaladin.SealOfFury.Level` | `10` | Level at which paladins learn it. |
| `ForeverPaladin.SealOfFury.AttackPowerCoefficient` / `SpellPowerCoefficient` | `0.022` / `0.044` | Holy damage per hit, per second of weapon speed. |
| `ForeverPaladin.SealOfFury.WardPercent` | `50` | Fury Ward's share of that Holy damage. `0` for no ward. |
| `ForeverPaladin.SealOfFury.GlyphManaPercent` | `8` | Base mana Glyph of Seal of Fury returns per Judgement, in percent. `0` makes the glyph do nothing. |
| `ForeverPaladin.Blessings.Duration` | `3600000` | Blessing duration in milliseconds. `0` keeps 10 / 30 minutes. |
| `ForeverPaladin.Judgements.Duration` | `40000` | Judgement debuff duration in milliseconds. `0` keeps 20 seconds. |
| `ForeverPaladin.Glyphs.BlessingCostReduction` | `50` | Might and Wisdom glyph cost cut in percent. `0` keeps the old +20 minutes. |

Durations must exist in the client's SpellDuration.dbc (1 hour and 40 seconds do); the server
logs an error and keeps the stock duration otherwise.

If you change a setting the tooltips mention and use the client patch, change the matching
value at the top of `tools/build_patch.py` and rebuild the patch.

## Turning it off

- **Keep the module, switch the mana return off:** set `ForeverPaladin.ShieldMana.Enable = 0`
  and reload the config. The hidden aura stays on paladins but does nothing.
- **Keep the module, switch Holy Strike off:** set `ForeverPaladin.HolyStrike.Enable = 0`. Each
  paladin loses Holy Strike at their next login, and gets it back at login if you set it to `1`
  again.
- **Remove the module for good:** set `Enable = 0` first if you can, so paladins lose it as they
  log in. Then stop the worldserver, delete the module, rebuild, and run both uninstall files:

  - `data/sql/uninstall/mod_forever_paladin_uninstall_world.sql` on the world database removes
    the script bindings, Seal of Fury's spells and the glyph, and turns item 37550 back into the
    stock unused item.
  - `data/sql/uninstall/mod_forever_paladin_uninstall_characters.sql` on the characters database
    takes Holy Strike and Seal of Fury off every character, their action bars, saved cooldowns
    and auras, clears Glyph of Seal of Fury from glyph slots, and removes its recipe and the
    glyphs in bags and banks (not ones in the mail or on the auction house). Without it, paladins who didn't log in keep the unscripted NPC version of Holy
    Strike.

  If you shipped the client patch, take the module's changes out of it too, or the tooltips and
  spellbook entries stay.

AzerothCore never runs the `uninstall` folder by itself; it only runs the module's `db-world`
folder.

## Client patch

Holy Strike, Seal of Fury and its glyph need it. For the rest it's optional: without it, blessing,
Judgement and glyph tooltips show the old durations and text, but work the same.

`tools/build_patch.py` changes four client files:

- **Spell.dbc:** Holy Strike made instant, with its tooltip, cooldown and cost; Seal of Fury's
  four new spells and Glyph of Seal of Fury's four; blessing and Judgement durations (for the
  tooltips); the two glyph tooltips.
- **SkillLineAbility.dbc:** Holy Strike in the Holy tab, Seal of Fury in the Protection tab and the
  glyph recipe in Inscription.
- **GlyphProperties.dbc:** glyph 912, Glyph of Seal of Fury.
- **Item.dbc:** item 37550 becomes a paladin glyph with Glyph of Seal of Command's icon.

Only the newest client patch's Spell.dbc is used, so start from the patch that already ships one
(patch-P on this realm) and the script keeps its other changes:

```bash
tools/build_patch.py --from-mpq patch-P.MPQ --stock-dbc <dbc folder> --out patch-P.MPQ.new
tools/build_patch.py --dbc Spell.dbc --sla SkillLineAbility.dbc --glyph GlyphProperties.dbc --item Item.dbc --out-dir DBFilesClient
tools/build_patch.py --sql --dbc Spell.dbc   # the spell_dbc rows in data/sql
```

`--stock-dbc` is a folder with the client's own GlyphProperties.dbc and Item.dbc (the client's
`DBFilesClient`, or AzerothCore's `data/dbc`). It's only read when the MPQ doesn't ship those
files yet; once it does, the script patches the MPQ's copies.

It needs StormLib for the MPQ (`STORMLIB` if it isn't `/usr/local/lib/libstorm.dylib`). Running it
again on its own output gives the same result. Players who get the new patch should delete their
`Cache/` folder.

## Limits

- Without the client patch, Holy Strike's button acts as a next-swing attack and shows the old
  NPC tooltip, a flat 75 mana and no cooldown.
- Playerbots paladins learn Holy Strike and Seal of Fury but don't use them. Their strategies
  would need actions for them.
- Changing `SealOfFury` or `Glyphs` settings and reloading the config affects auras cast after
  the reload; glyphs change at the next login.
- WoW Forever's per-spec Holy Strike talents aren't included: changing talent trees needs a bigger
  client patch.
