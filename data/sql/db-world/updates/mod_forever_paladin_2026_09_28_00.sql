-- mod-forever-paladin: Seal of Fury, WoW Forever's tanking seal. Four new spells; the client gets
-- the same rows from patch-P (tools/build_patch.py, which also prints the spell_dbc rows below
-- with --sql).
--
--   90080 Seal of Fury        copy of Seal of Righteousness (21084): 30 min, 14% of base mana.
--                             Seal family flag 0x08000000, so the core treats it as a seal (one
--                             at a time, Judgement uses it). Its third effect names 90081, the
--                             spell the core's Judgement script casts.
--   90081 Judgement of Fury   copy of Judgement (54158), plus Attack Me and a 4 sec taunt. Keeps
--                             Judgement's family flags, so Judgement talents and glyphs apply.
--   90082 Seal of Fury        copy of Seal of Righteousness' Holy damage (25742), without its
--                             family flags (no Seal of Righteousness talents or glyphs).
--   90083 Fury Ward           copy of Sacred Shield's absorb (58597) on yourself, 10 sec.
--
-- Judgement of Fury does what Judgement of Righteousness does: 1 + 32% of spell power + 20% of
-- attack power. The seal's melee procs follow Seal of Righteousness' spell_proc row.
--
-- The spell ids must match src/ForeverPaladin.cpp. Idempotent: safe to run again.

DELETE FROM `spell_dbc` WHERE `ID` IN (90080, 90081, 90082, 90083);
INSERT INTO `spell_dbc` VALUES
(90080, 0, 0, 0, 327680, 0, 0, 524288, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 20, 100, 0, 7, 10, 10, 30, 0, 0, 0, 0, 0, 1, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 6, 0, 6, 1, 0, 1, 0.0, 0.0, 0.0, -1, 0, 90080, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7986, 0, 2143, 0, 0, 'Seal of Fury', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, 14, 133, 1500, 0, 10, 134217728, 0, 0, 0, 1, 1, 0, 1.0, 1.0, 1.0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0),
(90081, 0, 0, 0, 2424832, 0, 1048576, 262656, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 28, 35, 0, 0, 0, 0, 0, 7, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 2, 114, 6, 1, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0.0, 1.0, 1.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11853, 0, 2143, 0, 0, 'Judgement of Fury', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, 0, 0, 0, 0, 10, 8388608, 0, 8, 0, 2, 0, 0, 1.0, 1.0, 1.0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0.25, 0.0, 0.0, 0, 0),
(90082, 0, 0, 0, 2359296, 0, 536870916, 262144, 8388608, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 101, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 13, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 2, 0, 0, 1, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2143, 0, 0, 'Seal of Fury', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, 0, 0, 0, 0, 10, 0, 0, 0, 0, 2, 0, 0, 1.0, 1.0, 1.0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0),
(90083, 0, 1, 0, 134283264, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 101, 0, 90, 1, 1, 1, 0, 0, 0, 0, 0, 13, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 6, 0, 0, 1, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 69, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 127, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12581, 0, 2143, 0, 0, 'Fury Ward', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 1.0, 1.0, 1.0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0);

DELETE FROM `spell_proc` WHERE `SpellId` = 90080;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(90080, 0, 0, 0, 0, 0, 0, 1, 2, 0, 2, 0, 0, 0, 0, 0);

DELETE FROM `spell_bonus_data` WHERE `entry` = 90081;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`) VALUES
(90081, 0.32, 0, 0.2, 0, 'Paladin - Judgement of Fury (mod-forever-paladin)');

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_pal_forever_seal_of_fury', 'spell_pal_forever_seal_of_fury_damage');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90080, 'spell_pal_forever_seal_of_fury'),
(90082, 'spell_pal_forever_seal_of_fury_damage');
