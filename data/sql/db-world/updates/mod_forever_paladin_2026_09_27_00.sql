-- mod-forever-paladin: Shield Specialization's mana return. Turn an unused server-side stub into
-- the hidden aura every paladin carries, make it proc on blocks, and bind its script.
--
-- 67553 "Pet Scaling - Master Spell 02 - Strength, Agility, Stamina" is an empty stub in
-- AzerothCore's spell_dbc that nothing uses and the client doesn't have, so this needs no client
-- patch. It gets the same passive, hidden, permanent setup as mod-forever-hunter's Lone Wolf
-- (67552), with one effect:
--
--   effect 1: SPELL_AURA_DUMMY (4), on the paladin
--
-- The spell_proc row makes it proc when its owner blocks a melee or ranged attack
-- (ProcFlags 0x2A8 = taken melee and ranged auto attacks and melee/ranged spells, HitMask 64 =
-- partial or full block). Chance is 100 because the script rolls the chance itself, by Redoubt
-- rank. Cooldown 3000 is the 3 second limit; ForeverPaladin.ShieldMana.Cooldown replaces it when
-- the server starts.
--
-- The spell id must match src/ForeverPaladin.cpp. Idempotent: safe to run again.

UPDATE `spell_dbc` SET
    `Attributes` = 448,
    `Effect_1` = 6, `EffectBasePoints_1` = -1, `ImplicitTargetA_1` = 1, `EffectMultipleValue_1` = 1,
    `EffectAura_1` = 4, `EffectMiscValue_1` = 0,
    `Name_Lang_enUS` = 'Shield Specialization (mod-forever-paladin)'
WHERE `ID` = 67553;

DELETE FROM `spell_proc` WHERE `SpellId` = 67553;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(67553, 0, 0, 0, 0, 0, 0x2A8, 0, 0, 64, 0, 0, 0, 100, 3000, 0);

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_pal_forever_shield_mana';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (67553, 'spell_pal_forever_shield_mana');
