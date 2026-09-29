-- mod-forever-paladin: Glyph of Seal of Fury, a major glyph like Glyph of Seal of Command. Each
-- Judgement you use with Seal of Fury active returns 8% of your base mana
-- (ForeverPaladin.SealOfFury.GlyphManaPercent); spell_pal_forever_judgement_of_fury does it.
-- The client gets the same rows from patch-P (tools/build_patch.py, which also prints the
-- spell_dbc rows below with --sql 90084 90085 90086).
--
--   90084 Glyph of Seal of Fury   the glyph: copy of Glyph of Seal of Command (54925), a plain
--                                 dummy aura instead of a proc.
--   90085 Glyph of Seal of Fury   the glyph item's spell: copy of 55109, inscribes glyph 912.
--   90086 Glyph of Seal of Fury   the Inscription recipe: copy of 57033, same materials, makes
--                                 item 37550.
--   glyph 912                     GlyphProperties: a major glyph, Seal of Command's rune icon.
--   item 37550                    Blizzard's unused "Deprecated Test Glyph 2" (no loot, vendor,
--                                 quest or recipe gives it), made a copy of Glyph of Seal of
--                                 Command (41094). item_dbc gives it that glyph's class, subclass
--                                 and icon; without it the core resets the item to the old ones.
--
-- Scribes learn the recipe from every trainer that teaches Glyph of Seal of Command, for the
-- same price and skill (335), and it skills up the same way (SkillLineAbility row 90086).
--
-- The ids must match src/ForeverPaladin.cpp and tools/build_patch.py. Idempotent: safe to run again.

DELETE FROM `spell_dbc` WHERE `ID` IN (90084, 90085, 90086);
INSERT INTO `spell_dbc` VALUES
(90084, 0, 0, 0, 192, 0, 0, 67108864, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 101, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 6, 0, 0, 1, 0, 0, 0.0, 0.0, 0.0, 7, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 1.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 'Glyph of Seal of Fury', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 1.0, 1.0, 1.0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0.0, 1.0, 0.0, 0, 0),
(90085, 0, 0, 0, 268435456, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 131072, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 63, 0, 0, 0, 101, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 74, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 912, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 536870912, 0, 0, 0, 0, 0, 0, 0, 0, 12369, 12369, 3267, 0, 0, 'Glyph of Seal of Fury', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, 1.0, 1.0, 0, 0, 0, 0, 0, -1, 1, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0),
(90086, 0, 0, 0, 65568, 1024, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 0, 0, 47, 0, 0, 0, 101, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0.0, 0, 0, 0, 0, 43124, 0, 0, 0, 0, 0, 0, 39502, 1, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 24, 0, 0, 1, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 37550, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10130, 0, 2557, 0, 0, 'Glyph of Seal of Fury', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712172, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, 1.0, 1.0, 0, 0, 0, 121, 0, 0, 1, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0);

DELETE FROM `glyphproperties_dbc` WHERE `ID` = 912;
INSERT INTO `glyphproperties_dbc` (`ID`, `SpellID`, `GlyphSlotFlags`, `SpellIconID`) VALUES
(912, 90084, 0, 3121);

DELETE FROM `skilllineability_dbc` WHERE `ID` = 90086;
INSERT INTO `skilllineability_dbc` (`ID`, `SkillLine`, `Spell`, `RaceMask`, `ClassMask`, `ExcludeRace`, `ExcludeClass`, `MinSkillLineRank`, `SupercededBySpell`, `AcquireMethod`, `TrivialSkillLineRankHigh`, `TrivialSkillLineRankLow`, `CharacterPoints_1`, `CharacterPoints_2`) VALUES
(90086, 773, 90086, 0, 0, 0, 0, 1, 0, 0, 350, 340, 0, 0);

DELETE FROM `item_dbc` WHERE `ID` = 37550;
INSERT INTO `item_dbc` (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`, `DisplayInfoID`, `InventoryType`, `SheatheType`) VALUES
(37550, 16, 2, 0, 4, 58832, 0, 0);

-- The item: Glyph of Seal of Command's row, whatever this server has in it, with a new name and
-- spell. The old item's other-language names ("Testglyphe 2" and so on) go, so every client
-- shows the English name.
DELETE FROM `item_template` WHERE `entry` = 37550;
DROP TEMPORARY TABLE IF EXISTS `tmp_fp_glyph`;
CREATE TEMPORARY TABLE `tmp_fp_glyph` SELECT * FROM `item_template` WHERE `entry` = 41094;
UPDATE `tmp_fp_glyph` SET `entry` = 37550, `name` = 'Glyph of Seal of Fury', `spellid_1` = 90085;
INSERT INTO `item_template` SELECT * FROM `tmp_fp_glyph`;
DROP TEMPORARY TABLE `tmp_fp_glyph`;
DELETE FROM `item_template_locale` WHERE `ID` = 37550;

DELETE FROM `trainer_spell` WHERE `SpellId` = 90086;
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `TrainerId`, 90086, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, 0
FROM `trainer_spell` WHERE `SpellId` = 57033;

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_pal_forever_judgement_of_fury';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(90081, 'spell_pal_forever_judgement_of_fury');
