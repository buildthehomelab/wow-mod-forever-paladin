-- mod-forever-paladin: the spell Glyph of Seal of Fury's mana is credited to, so the combat log
-- shows "You gain 66 Mana from Glyph of Seal of Fury." The glyph aura (90084) is hidden, like
-- every glyph's, and the client leaves hidden spells out of the combat log, so mana credited to it
-- never showed. 90087 is a visible copy of 68082, the spell Glyph of Seal of Command's mana uses.
-- The client gets the same row from patch-P (tools/build_patch.py --sql 90087 prints it).
--
-- The script (spell_pal_forever_judgement_of_fury) still works out the amount; 90087 only names
-- it. Must match src/ForeverPaladin.cpp. Idempotent: safe to run again.

DELETE FROM `spell_dbc` WHERE `ID` IN (90087);
INSERT INTO `spell_dbc` VALUES
(90087, 0, 0, 0, 537133056, 0, 536870916, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 6, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 30, 0, 0, 1, 0, 0, 0.0, 0.0, 0.0, 7, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11906, 0, 3017, 0, 0, 'Glyph of Seal of Fury', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712190, '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', 16712188, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, 1.0, 1.0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0.0, 1.0, 1.0, 0, 0);
