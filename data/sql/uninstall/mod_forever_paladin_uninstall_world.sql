-- mod-forever-paladin: undo the module's world database change. Run it by hand on the world
-- database after removing the module; AzerothCore doesn't run it automatically (it only runs the
-- module's db-world folder). Run mod_forever_paladin_uninstall_characters.sql too.
--
-- Turning the module off doesn't need this: ForeverPaladin.HolyStrike.Enable = 0 is enough.
-- This is for taking the module out of the server for good, so the core doesn't log a missing
-- script.
--
-- Removes the script binding on Holy Strike (13953). NPCs that cast it are unaffected either way;
-- the core's own spell_cooldown_overrides row for it was never touched. Also puts 67553 back to
-- the empty stub AzerothCore ships and removes its spell_proc row and script binding (Shield
-- Specialization's mana return), and removes Seal of Fury's four spells (90080-90083) with their
-- spell_proc, spell_bonus_data and script rows. Blessing and Judgement durations and the glyphs
-- are only changed in memory, so there's nothing to undo for them. Idempotent: safe to run again.

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_holy_strike', 'spell_pal_forever_shield_mana',
    'spell_pal_forever_seal_of_fury', 'spell_pal_forever_seal_of_fury_damage');

DELETE FROM `spell_dbc` WHERE `ID` IN (90080, 90081, 90082, 90083);
DELETE FROM `spell_proc` WHERE `SpellId` = 90080;
DELETE FROM `spell_bonus_data` WHERE `entry` = 90081;

DELETE FROM `spell_proc` WHERE `SpellId` = 67553;

UPDATE `spell_dbc` SET
    `Attributes` = 384,
    `Effect_1` = 0, `EffectBasePoints_1` = 0, `ImplicitTargetA_1` = 0, `EffectMultipleValue_1` = 0,
    `EffectAura_1` = 0, `EffectMiscValue_1` = 0,
    `Name_Lang_enUS` = 'Pet Scaling - Master Spell 02 - Strength, Agility, Stamina'
WHERE `ID` = 67553;
