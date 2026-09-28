# Forever Paladin

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that brings WoW Forever's
paladin changes to a 3.3.5 server:

- **Holy Strike:** every paladin learns it at level 6, whatever their spec.
- **Shield Specialization's mana return:** blocks can restore 6% of base mana.
- **Seal of Fury:** a tanking seal whose Judgement taunts. Needs the client patch.
- **1 hour Blessings** and **40 second Judgements**, with the Blessing of Might and Wisdom glyphs
  reworked to match.

Holy Strike, the mana return and the durations work without a client patch; the optional patch
updates their tooltips. Seal of Fury is four new spells, so players need the patch for it.

Some of WoW Forever's paladin changes are already how 3.3.5 works, so the module leaves them
alone: Judgement doesn't use up the seal, Blessing of Kings is trained (level 20), and the Fire,
Frost and Shadow Resistance Auras already reach the whole raid.

## Holy Strike

- Paladins learn **Holy Strike** at level 6. Existing paladins get it at their next login.
- It's a **next-swing attack**, like Heroic Strike: press it and your next melee swing deals
  Holy damage instead of a normal hit. Holy damage ignores armor.
- That swing does your weapon damage, the spell's small built-in bonus, and **20% of your attack
  power plus 20% of your Holy spell power** on top.
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

## Why a next-swing attack and not an instant strike

The spell is Holy Strike (13953), which the 3.3.5 client already has. Blizzard only ever gave it to
NPCs, like the Scarlet Crusade. No item, trainer or talent teaches it to players, so reusing it
changes nothing players could already get, and every client already has its name and icon.

The client knows it as a next-swing attack, so that's how it works here. Making it instant, like
in WoW Forever, would mean every player needs the client patch, or their button misbehaves.

NPCs that cast Holy Strike are unchanged: same damage, same cooldown, same 75 mana.

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
| `ForeverPaladin.HolyStrike.AttackPowerCoefficient` | `0.2` | Extra damage as a share of attack power. |
| `ForeverPaladin.HolyStrike.SpellPowerCoefficient` | `0.2` | Extra damage as a share of Holy spell power. |
| `ForeverPaladin.HolyStrike.ManaCostPercent` | `5` | Mana cost as a whole percentage of base mana. `0` keeps the flat 75. |
| `ForeverPaladin.ShieldMana.Enable` | `1` | Master switch for the mana return on block. |
| `ForeverPaladin.ShieldMana.ChanceRank1` / `2` / `3` | `33` / `66` / `100` | Chance in percent with 1, 2 or 3 points in Redoubt. |
| `ForeverPaladin.ShieldMana.BaseManaPercent` | `6` | Mana returned, as a percentage of base mana. |
| `ForeverPaladin.ShieldMana.Cooldown` | `3000` | Minimum milliseconds between two mana returns. |
| `ForeverPaladin.SealOfFury.Enable` | `1` | Master switch. With `0`, paladins lose Seal of Fury at their next login. |
| `ForeverPaladin.SealOfFury.Level` | `10` | Level at which paladins learn it. |
| `ForeverPaladin.SealOfFury.AttackPowerCoefficient` / `SpellPowerCoefficient` | `0.022` / `0.044` | Holy damage per hit, per second of weapon speed. |
| `ForeverPaladin.SealOfFury.WardPercent` | `50` | Fury Ward's share of that Holy damage. `0` for no ward. |
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
    the script bindings and Seal of Fury's spells.
  - `data/sql/uninstall/mod_forever_paladin_uninstall_characters.sql` on the characters database
    takes Holy Strike and Seal of Fury off every character, their action bars, saved cooldowns
    and auras. Without it, paladins who didn't log in keep the unscripted NPC version of Holy
    Strike.

  If you shipped the client patch, take the module's changes out of it too, or the tooltips and
  spellbook entries stay.

AzerothCore never runs the `uninstall` folder by itself; it only runs the module's `db-world`
folder.

## Client patch

Seal of Fury needs it. For everything else it's optional: without it, Holy Strike shows the old
NPC tooltip ("Consecrates the caster's weapon..."), a flat 75 Mana and no cooldown and sits in the
General tab, and blessing, Judgement and glyph tooltips show the old durations and text. All of
that still works the same.

`tools/build_patch.py` changes two client files:

- **Spell.dbc:** Holy Strike's tooltip, cooldown and cost; Seal of Fury's four new spells;
  blessing and Judgement durations (for the tooltips); the two glyph tooltips.
- **SkillLineAbility.dbc:** Holy Strike in the Holy tab and Seal of Fury in the Protection tab.

Only the newest client patch's Spell.dbc is used, so start from the patch that already ships one
(patch-P on this realm) and the script keeps its other changes:

```bash
tools/build_patch.py --from-mpq patch-P.MPQ --out patch-P.MPQ.new
tools/build_patch.py --dbc Spell.dbc --sla SkillLineAbility.dbc --out-dir DBFilesClient
tools/build_patch.py --sql --dbc Spell.dbc   # the spell_dbc rows in data/sql
```

It needs StormLib for the MPQ (`STORMLIB` if it isn't `/usr/local/lib/libstorm.dylib`). Running it
again on its own output gives the same result. Players who get the new patch should delete their
`Cache/` folder.

## Limits

- Without the client patch, the client greys the button out below 75 mana even when Holy Strike
  costs less, and above level 60, where it costs more than 75, the button can look ready when you
  can't afford it ("Not enough mana").
- Playerbots paladins learn Holy Strike and Seal of Fury but don't use them. Their strategies
  would need actions for them.
- Changing `SealOfFury` or `Glyphs` settings and reloading the config affects auras cast after
  the reload; glyphs change at the next login.
- WoW Forever's per-spec Holy Strike talents aren't included: changing talent trees needs a bigger
  client patch.
