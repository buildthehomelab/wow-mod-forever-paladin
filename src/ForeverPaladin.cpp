/*
 * mod-forever-paladin
 *
 * Every paladin learns Holy Strike at level 6, whatever their spec, as in WoW Forever. It's a
 * next-swing attack: the paladin's next melee swing deals Holy damage plus a bonus from attack
 * power and spell power, then the ability goes on a 12 second cooldown. Like Crusader Strike, it
 * costs 5% of base mana.
 *
 * The spell is Holy Strike (13953), a spell the 3.3.5 client already has: Blizzard only ever gave
 * it to NPCs (Scarlet Crusade and others), and no item, trainer or talent teaches it. So it works
 * without a client patch. The client shows it as a next-swing attack, like Heroic Strike, so
 * that's how it works here too. tools/patch-forever-paladin-dbc.sh makes an optional client patch
 * that updates its tooltip and puts it in the Holy tab of the spellbook.
 *
 * NPCs that cast Holy Strike keep their damage, cooldown and cost.
 *
 * Released under the MIT License.
 */

#include "Config.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "WorldPacket.h"

namespace
{
    // Must match the SQL and tools/patch-forever-paladin-dbc.sh.
    constexpr uint32 SPELL_HOLY_STRIKE = 13953;

    struct Config
    {
        bool enabled = true;
        uint8 level = 6;
        uint32 cooldown = 12000;
        float attackPowerCoefficient = 0.2f;
        float spellPowerCoefficient = 0.2f;
        uint32 manaCostPercent = 5;
    };

    Config config;

    // The game data's flat mana cost (75), read before the module changes it. NPCs keep paying it.
    uint32 flatManaCost = 0;
    bool flatManaCostRead = false;

    // Make the server charge a share of base mana, like Crusader Strike, instead of the flat 75.
    // The core then checks and takes the right amount for players by itself. Runs once the spells
    // are loaded, and again when the config is reloaded.
    void ApplyManaCost()
    {
        SpellInfo* spellInfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_HOLY_STRIKE));
        if (!spellInfo)
            return;

        if (!flatManaCostRead)
        {
            flatManaCost = spellInfo->ManaCost;
            flatManaCostRead = true;
        }

        if (config.manaCostPercent)
        {
            spellInfo->ManaCost = 0;
            spellInfo->ManaCostPercentage = config.manaCostPercent;
        }
        else
        {
            spellInfo->ManaCost = flatManaCost;
            spellInfo->ManaCostPercentage = 0;
        }
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

// 13953 - Holy Strike
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

    // An NPC casting Holy Strike while the server charges a share of base mana. It should still
    // pay the flat 75, so the core's share is topped up below.
    Unit* GetNpcPayingFlatCost()
    {
        Unit* caster = GetCaster();
        if (!caster || caster->IsPlayer() || !config.manaCostPercent)
            return nullptr;

        if (GetSpell()->HasTriggeredCastFlag(TRIGGERED_IGNORE_POWER_AND_REAGENT_COST))
            return nullptr;

        return caster;
    }

    SpellCastResult CheckNpcManaCost()
    {
        Unit* npc = GetNpcPayingFlatCost();
        if (npc && npc->GetPower(POWER_MANA) < flatManaCost)
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    // The core has taken its share of the NPC's base mana (mana is taken in full even on a
    // miss). Take the rest of the 75.
    void TakeNpcManaCost()
    {
        if (Unit* npc = GetNpcPayingFlatCost())
            npc->ModifyPower(POWER_MANA, GetSpell()->GetPowerCost() - int32(flatManaCost));
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

    // The stock client's Spell.dbc has no cooldown for Holy Strike, and the core's own override
    // for it (spell_cooldown_overrides, 6 seconds) is meant for NPCs and isn't sent to the client.
    // So set the player's cooldown here and tell the client, which then shows it on the action
    // bar. This runs after the core has added its cooldown, and replaces it.
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
        OnCheckCast += SpellCheckCastFn(spell_holy_strike::CheckNpcManaCost);
        OnHit += SpellHitFn(spell_holy_strike::AddBonusDamage);
        AfterCast += SpellCastFn(spell_holy_strike::TakeNpcManaCost);
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
        config.manaCostPercent        = sConfigMgr->GetOption<uint32>("ForeverPaladin.HolyStrike.ManaCostPercent", 5);

        // At startup the spells aren't loaded yet; OnBeforeWorldInitialized does it then.
        ApplyManaCost();
    }

    void OnBeforeWorldInitialized() override
    {
        ApplyManaCost();
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
