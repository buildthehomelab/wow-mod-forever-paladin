# Forever Paladin

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that brings WoW Forever's
paladin changes to a 3.3.5 server:

- **Holy Strike:** every paladin learns it at level 6, whatever their spec.
- **Shield Specialization's mana return:** blocks can restore 6% of base mana.

Both work without a client patch; an optional one updates Holy Strike's tooltip and puts it in
the Holy tab of the spellbook.

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

If you change the cooldown, mana cost or damage settings and use the client patch, change the
matching values at the top of `tools/patch-forever-paladin-dbc.sh` and rebuild the patch.

## Turning it off

- **Keep the module, switch the mana return off:** set `ForeverPaladin.ShieldMana.Enable = 0`
  and reload the config. The hidden aura stays on paladins but does nothing.
- **Keep the module, switch Holy Strike off:** set `ForeverPaladin.HolyStrike.Enable = 0`. Each
  paladin loses Holy Strike at their next login, and gets it back at login if you set it to `1`
  again.
- **Remove the module for good:** set `Enable = 0` first if you can, so paladins lose it as they
  log in. Then stop the worldserver, delete the module, rebuild, and run both uninstall files:

  - `data/sql/uninstall/mod_forever_paladin_uninstall_world.sql` on the world database removes
    the script binding.
  - `data/sql/uninstall/mod_forever_paladin_uninstall_characters.sql` on the characters database
    takes Holy Strike off every character, their action bars and saved cooldowns. Without it,
    paladins who didn't log in keep the unscripted NPC version of the spell.

  If you shipped the optional client patch, take its Holy Strike changes out of the client patch
  too, or the tooltip and spellbook entry stay.

AzerothCore never runs the `uninstall` folder by itself; it only runs the module's `db-world`
folder.

## Optional client patch

Without it, players see the old NPC tooltip ("Consecrates the caster's weapon..."), a flat
75 Mana and no cooldown, and Holy Strike sits in the General tab of the spellbook. It still works
the same.

`tools/patch-forever-paladin-dbc.sh` changes two client files:

- **Spell.dbc:** a tooltip that describes the real damage, the 5% of base mana cost and the
  12 sec cooldown. Only Holy Strike's record changes.
- **SkillLineAbility.dbc:** adds Holy Strike to the Holy tab of the spellbook.

```bash
tools/patch-forever-paladin-dbc.sh <Spell.dbc> <SkillLineAbility.dbc> DBFilesClient
```

Only the newest client patch's Spell.dbc is used, so if another patch already ships one (for
example with mod-profession-craft-cd and mod-hearthstone-cd changes), give the script that
Spell.dbc and put the result back in that same patch. The script only touches Holy Strike, and it
can be run again on its own output. Take SkillLineAbility.dbc from the AzerothCore data folder
(`dbc/SkillLineAbility.dbc`), which matches the client's.

Pack both files into the MPQ as `DBFilesClient\Spell.dbc` and
`DBFilesClient\SkillLineAbility.dbc`. Players who get the new patch should delete their `Cache/`
folder.

## Limits

- Without the client patch, the client greys the button out below 75 mana even when Holy Strike
  costs less, and above level 60, where it costs more than 75, the button can look ready when you
  can't afford it ("Not enough mana").
- Playerbots paladins learn it but don't press it. Their strategies would need a Holy Strike
  action.
- WoW Forever's per-spec Holy Strike talents aren't included: changing talent trees needs a bigger
  client patch.
