/*
 * mod-forever-paladin
 *
 * Every paladin learns Holy Strike at level 6, whatever their spec, as in WoW Forever. It's a
 * next-swing attack: the paladin's next melee swing deals Holy damage plus a bonus from attack
 * power and spell power, then the ability goes on a 12 second cooldown. Like Crusader Strike, it
 * costs 5% of base mana.
 *
 * No client patch. The spell is Holy Strike (13953), a spell the 3.3.5 client already has: Blizzard
 * only ever gave it to NPCs (Scarlet Crusade and others), and no item, trainer or talent teaches
 * it, so its name, icon and tooltip are already right in every client. The client shows it as a
 * next-swing attack, like Heroic Strike, so that's how it works here too. Making it instant would
 * need a Spell.dbc patch.
 *
 * Only player casts are changed. NPCs that cast Holy Strike keep their damage, cooldown and cost.
 *
 * Released under the MIT License.
 */

#include "Config.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "WorldPacket.h"

namespace
{
    // Must match the SQL.
    constexpr uint32 SPELL_HOLY_STRIKE = 13953;

    struct Config
    {
        bool enabled = true;
        uint8 level = 6;
        uint32 cooldown = 12000;
        float attackPowerCoefficient = 0.2f;
        float spellPowerCoefficient = 0.2f;
        float manaCostPercent = 5.0f;
    };

    Config config;

    // What Holy Strike costs this player: a share of base mana, like Crusader Strike.
    int32 GetManaCost(Player* player)
    {
        return int32(player->GetCreateMana() * config.manaCostPercent / 100.0f);
    }

    // Teach or remove Holy Strike so it matches the paladin's level and the Enable setting.
    // Turning the module off takes it away again at the next login.
    void UpdateHolyStrike(Player* player)
    {
        if (player->getClass() != CLASS_PALADIN)
            return;

        bool const shouldKnow = config.enabled && player->GetLevel() >= config.level;
        bool const knows = player->HasSpell(SPELL_HOLY_STRIKE);

        if (shouldKnow && !knows)
            player->learnSpell(SPELL_HOLY_STRIKE);
        else if (!shouldKnow && knows)
            player->removeSpell(SPELL_HOLY_STRIKE, SPEC_MASK_ALL, false);
    }
}

// 13953 - Holy Strike, when a player casts it.
class spell_holy_strike : public SpellScript
{
    PrepareSpellScript(spell_holy_strike);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HOLY_STRIKE });
    }

    Player* GetPlayerCaster()
    {
        Unit* caster = GetCaster();
        return caster ? caster->ToPlayer() : nullptr;
    }

    // The client's copy of Holy Strike costs a flat 75 mana at every level, a big share of a
    // level 6 paladin's mana and nothing at 80. The core still checks and takes those 75; this
    // makes sure the paladin also has the real cost, which is more than 75 above level 60.
    SpellCastResult CheckManaCost()
    {
        Player* player = GetPlayerCaster();
        if (!player || config.manaCostPercent <= 0.0f)
            return SPELL_CAST_OK;

        if (int32(player->GetPower(POWER_MANA)) < GetManaCost(player))
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    // The core has just taken its 75 mana (mana is taken in full even on a miss). Give back the
    // difference, or take the rest.
    void AdjustManaCost()
    {
        Player* player = GetPlayerCaster();
        if (!player || config.manaCostPercent <= 0.0f || player->GetCommandStatus(CHEAT_POWER))
            return;

        // A triggered cast (a GM's .cast triggered, say) took no mana.
        Spell* spell = GetSpell();
        if (spell->HasTriggeredCastFlag(TRIGGERED_IGNORE_POWER_AND_REAGENT_COST))
            return;

        int32 const taken = spell->GetPowerCost();
        if (!taken)
            return;

        player->ModifyPower(POWER_MANA, taken - GetManaCost(player));
    }

    // The game data only adds a small bonus to the weapon hit (about 220 at level 80), which
    // would make a 12 second cooldown pointless. Add a share of attack power and Holy spell power
    // on top. This runs before crits and resistances are worked out, so the bonus crits too.
    void AddBonusDamage()
    {
        Player* player = GetPlayerCaster();
        if (!player || GetHitDamage() <= 0)
            return;

        float const attackPower = player->GetTotalAttackPowerValue(BASE_ATTACK);
        float const spellPower = float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY));
        int32 const bonus = int32(attackPower * config.attackPowerCoefficient + spellPower * config.spellPowerCoefficient);

        SetHitDamage(GetHitDamage() + bonus);
    }

    // The client's Spell.dbc has no cooldown for Holy Strike, and the core's own override for it
    // (spell_cooldown_overrides, 6 seconds) is meant for NPCs and isn't sent to the client. So set
    // the player's cooldown here and tell the client, which then shows it on the action bar.
    // This runs after the core has added its cooldown, and replaces it.
    void StartCooldown()
    {
        Player* player = GetPlayerCaster();
        if (!player)
            return;

        if (!config.cooldown)
        {
            player->RemoveSpellCooldown(SPELL_HOLY_STRIKE, true);
            return;
        }

        player->AddSpellCooldown(SPELL_HOLY_STRIKE, 0, config.cooldown, true);

        WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 1 + 4 + 4);
        data << player->GetGUID();
        data << uint8(SPELL_COOLDOWN_FLAG_NONE);
        data << uint32(SPELL_HOLY_STRIKE);
        data << uint32(config.cooldown);
        player->SendDirectMessage(&data);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_holy_strike::CheckManaCost);
        OnHit += SpellHitFn(spell_holy_strike::AddBonusDamage);
        AfterCast += SpellCastFn(spell_holy_strike::AdjustManaCost);
        AfterCast += SpellCastFn(spell_holy_strike::StartCooldown);
    }
};

class ForeverPaladinWorldScript : public WorldScript
{
public:
    ForeverPaladinWorldScript() : WorldScript("ForeverPaladinWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled                = sConfigMgr->GetOption<bool>("ForeverPaladin.HolyStrike.Enable", true);
        config.level                  = uint8(sConfigMgr->GetOption<uint32>("ForeverPaladin.HolyStrike.Level", 6));
        config.cooldown               = sConfigMgr->GetOption<uint32>("ForeverPaladin.HolyStrike.Cooldown", 12000);
        config.attackPowerCoefficient = sConfigMgr->GetOption<float>("ForeverPaladin.HolyStrike.AttackPowerCoefficient", 0.2f);
        config.spellPowerCoefficient  = sConfigMgr->GetOption<float>("ForeverPaladin.HolyStrike.SpellPowerCoefficient", 0.2f);
        config.manaCostPercent        = sConfigMgr->GetOption<float>("ForeverPaladin.HolyStrike.ManaCostPercent", 5.0f);
    }
};

class ForeverPaladinPlayerScript : public PlayerScript
{
public:
    ForeverPaladinPlayerScript() : PlayerScript("ForeverPaladinPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        UpdateHolyStrike(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        UpdateHolyStrike(player);
    }
};

void AddForeverPaladinScripts()
{
    new ForeverPaladinWorldScript();
    new ForeverPaladinPlayerScript();
    RegisterSpellScript(spell_holy_strike);
}
