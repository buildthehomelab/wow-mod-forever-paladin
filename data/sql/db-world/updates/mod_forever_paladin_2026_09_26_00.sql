-- mod-forever-paladin: bind the script to Holy Strike (13953).
--
-- 13953 is a Holy Strike Blizzard only gave NPCs. The script leaves NPC casts alone, so the
-- creatures that use it (Scarlet Crusade and others) keep their damage and cooldown, including
-- the core's spell_cooldown_overrides row for it, which this doesn't touch.
--
-- The spell id must match SPELL_HOLY_STRIKE in src/ForeverPaladin.cpp. Idempotent: safe to run again.

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_holy_strike';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (13953, 'spell_holy_strike');
