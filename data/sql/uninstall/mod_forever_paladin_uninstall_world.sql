-- mod-forever-paladin: undo the module's world database change. Run it by hand on the world
-- database after removing the module; AzerothCore doesn't run it automatically (it only runs the
-- module's db-world folder). Run mod_forever_paladin_uninstall_characters.sql too.
--
-- Turning the module off doesn't need this: ForeverPaladin.HolyStrike.Enable = 0 is enough.
-- This is for taking the module out of the server for good, so the core doesn't log a missing
-- script.
--
-- Removes the script binding on Holy Strike (13953). NPCs that cast it are unaffected either way;
-- the core's own spell_cooldown_overrides row for it was never touched. Idempotent: safe to run
-- again.

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_holy_strike';
