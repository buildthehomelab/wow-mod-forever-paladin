-- mod-forever-paladin: take Holy Strike (13953) and Seal of Fury (90080) away from every character. Run it by hand on the
-- characters database, with the worldserver stopped: a running server saves online characters
-- over these tables. AzerothCore doesn't run it automatically.
--
-- While the module is still installed, ForeverPaladin.HolyStrike.Enable = 0 does the same thing
-- at each paladin's next login. Once the module is gone nothing removes the spell, and paladins
-- would keep the unscripted NPC version: weak damage, a flat 75 mana, and the NPCs' cooldown,
-- which the client doesn't show. Run this
-- after removing the module (with mod_forever_paladin_uninstall_world.sql).
--
-- 13953 is an NPC-only spell that nothing else teaches players, so every character that has it
-- got it from this module (or a GM). Also clears it from action bars and saved cooldowns.
-- Idempotent: safe to run again.

DELETE FROM `character_spell` WHERE `spell` = 13953;
DELETE FROM `character_action` WHERE `action` = 13953 AND `type` = 0; -- 0 = spell button
DELETE FROM `character_spell_cooldown` WHERE `spell` = 13953;

-- Seal of Fury (90080) and its Fury Ward (90083): the spell, action bar buttons and saved auras.
DELETE FROM `character_spell` WHERE `spell` = 90080;
DELETE FROM `character_action` WHERE `action` = 90080 AND `type` = 0;
DELETE FROM `character_aura` WHERE `spell` IN (90080, 90083);
