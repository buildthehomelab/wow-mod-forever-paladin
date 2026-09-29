-- mod-forever-paladin: take Holy Strike (13953), Seal of Fury (90080) and Glyph of Seal of Fury
-- away from every character. Run it by hand on the
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

-- Glyph of Seal of Fury: the glyph (912) out of glyph slots, the recipe (90086) out of scribes'
-- spellbooks, and the glyph items (37550) out of bags and banks. Items in the mail or on the
-- auction house are left; once the uninstall world SQL has run they're the unusable stock item.
UPDATE `character_glyphs` SET `glyph1` = 0 WHERE `glyph1` = 912;
UPDATE `character_glyphs` SET `glyph2` = 0 WHERE `glyph2` = 912;
UPDATE `character_glyphs` SET `glyph3` = 0 WHERE `glyph3` = 912;
UPDATE `character_glyphs` SET `glyph4` = 0 WHERE `glyph4` = 912;
UPDATE `character_glyphs` SET `glyph5` = 0 WHERE `glyph5` = 912;
UPDATE `character_glyphs` SET `glyph6` = 0 WHERE `glyph6` = 912;
DELETE FROM `character_spell` WHERE `spell` = 90086;
DELETE `ci` FROM `character_inventory` `ci` JOIN `item_instance` `ii` ON `ii`.`guid` = `ci`.`item` WHERE `ii`.`itemEntry` = 37550;
DELETE FROM `item_instance` WHERE `itemEntry` = 37550
    AND `guid` NOT IN (SELECT `item_guid` FROM `mail_items`)
    AND `guid` NOT IN (SELECT `itemguid` FROM `auctionhouse`);
