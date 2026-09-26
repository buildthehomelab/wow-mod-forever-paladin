# Forever Paladin

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that brings WoW Forever's
paladin changes to a 3.3.5 server, without a client patch. For now that's Holy Strike: every
paladin learns it at level 6, whatever their spec.

## Holy Strike

- Paladins learn **Holy Strike** at level 6. Existing paladins get it at their next login.
- It's a **next-swing attack**, like Heroic Strike: press it and your next melee swing deals
  Holy damage instead of a normal hit. Holy damage ignores armor.
- That swing does your weapon damage, the spell's small built-in bonus, and **20% of your attack
  power plus 20% of your Holy spell power** on top.
- Then it goes on a **12 second cooldown**, which shows on the action bar.
- It costs 75 mana, needs a melee weapon, and you have to face the target.

It isn't tied to a spec or a talent, so dual spec and talent resets don't affect it. It's in the
**General** tab of the spellbook.

## Why a next-swing attack and not an instant strike

The spell is Holy Strike (13953), which the 3.3.5 client already has. Blizzard only ever gave it to
NPCs, like the Scarlet Crusade. No item, trainer or talent teaches it to players, so reusing it
changes nothing players could already get. Its name, icon and tooltip are already in every client.

The client knows it as a next-swing attack, so that's how it works here. Making it instant, like
in WoW Forever, would need a patched Spell.dbc for every player.

NPCs that cast Holy Strike are unchanged: the module only changes casts by players.

## Install

Clone it into your AzerothCore `modules` folder **as `mod-forever-paladin`**, without the repo's
`wow-` prefix. AzerothCore finds the module's entry point from the folder name.

```bash
cd <azerothcore>/modules
git clone https://github.com/buildthehomelab/wow-mod-forever-paladin.git mod-forever-paladin
```

Rebuild the worldserver, then copy `conf/mod_forever_paladin.conf.dist` to your config folder as
`mod_forever_paladin.conf`. The spell script binding is added to the world database on the next start.

## Settings

| Setting | Default | What it does |
|---------|---------|--------------|
| `ForeverPaladin.HolyStrike.Enable` | `1` | Master switch. With `0`, paladins lose Holy Strike at their next login. |
| `ForeverPaladin.HolyStrike.Level` | `6` | Level at which paladins learn it. |
| `ForeverPaladin.HolyStrike.Cooldown` | `12000` | Cooldown in milliseconds. `0` for none. |
| `ForeverPaladin.HolyStrike.AttackPowerCoefficient` | `0.2` | Extra damage as a share of attack power. |
| `ForeverPaladin.HolyStrike.SpellPowerCoefficient` | `0.2` | Extra damage as a share of Holy spell power. |

## Limits

- The tooltip doesn't list the cooldown, since the client's copy of the spell has none. The
  cooldown still shows on the action bar.
- Playerbots paladins learn it but don't press it. Their strategies would need a Holy Strike
  action.
- WoW Forever's per-spec Holy Strike talents aren't included: changing talent trees needs a client
  patch.
