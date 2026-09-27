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
 * Shield Specialization's mana return: in WoW Forever that talent also gives blocks a 33% chance
 * to restore 6% of maximum mana, at most once every 3 seconds. Here it's 6% of base mana, so
 * Intellect doesn't make it bigger. 3.3.5 has no Shield Specialization
 * (its block bonus became part of Redoubt), so here Redoubt gets the mana return, with the chance
 * set per Redoubt rank. Every paladin carries a hidden aura, an unused server-side stub (67553),
 * that procs when they block; the proc only pays out while they have Redoubt, so talent resets
 * and dual spec need no extra handling. The combat log credits the mana to Redoubt.
 *
 * Released under the MIT License.
 */

#include "Config.h"
#include "Opcodes.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "WorldPacket.h"

#include <array>
#include <cmath>

namespace
{
    // Must match the SQL and tools/patch-forever-paladin-dbc.sh.
    constexpr uint32 SPELL_HOLY_STRIKE = 13953;

    // Must match the SQL. 67553 is AzerothCore's empty "Pet Scaling - Master Spell 02" stub,
    // renamed; the client doesn't have it.
    constexpr uint32 SPELL_SHIELD_MANA = 67553;

    // Redoubt ranks 1 to 3 (the talents, not the block chance buff they trigger).
    constexpr std::array<uint32, 3> SPELL_REDOUBT_RANKS = { 20127, 20130, 20135 };

    struct Config
    {
        bool enabled = true;
        uint8 level = 6;
        uint32 cooldown = 12000;
        float attackPowerCoefficient = 0.2f;
        float spellPowerCoefficient = 0.2f;
        uint32 manaCostPercent = 5;

        bool shieldManaEnabled = true;
        std::array<float, 3> shieldManaChance = { 33.0f, 66.0f, 100.0f };
        float shieldManaPercent = 6.0f;
        uint32 shieldManaCooldown = 3000;
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

    // The 3 second limit is the proc cooldown in the SQL's spell_proc row. The core only starts
    // it when the proc goes through, so a block that fails the chance roll doesn't use it up.
    // Runs once the spell data is loaded, and again when the config is reloaded.
    void ApplyShieldManaCooldown()
    {
        SpellProcEntry* procEntry = const_cast<SpellProcEntry*>(sSpellMgr->GetSpellProcEntry(SPELL_SHIELD_MANA));
        if (!procEntry)
            return;

        procEntry->Cooldown = Milliseconds(config.shieldManaCooldown);
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

// 67553 - Pet Scaling - Master Spell 02, renamed Shield Specialization. Every paladin carries it,
// hidden; spell_proc makes it proc when they block.
class spell_pal_forever_shield_mana : public AuraScript
{
    PrepareAuraScript(spell_pal_forever_shield_mana);

    // The Redoubt rank the paladin has (1 to 3), or 0. Talent spells are auras on their owner.
    uint32 GetRedoubtRank(Unit* target, uint32& spellId)
    {
        for (uint32 rank = SPELL_REDOUBT_RANKS.size(); rank > 0; --rank)
        {
            if (target->HasAura(SPELL_REDOUBT_RANKS[rank - 1]))
            {
                spellId = SPELL_REDOUBT_RANKS[rank - 1];
                return rank;
            }
        }

        return 0;
    }

    // The chance is rolled here, not in spell_proc, because it depends on the Redoubt rank.
    // A failed roll returns false, so the 3 second cooldown doesn't start.
    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        Unit* target = GetTarget();
        if (!config.shieldManaEnabled || !target->IsPlayer() || target->getPowerType() != POWER_MANA)
            return false;

        uint32 redoubtSpellId = 0;
        uint32 const rank = GetRedoubtRank(target, redoubtSpellId);
        if (!rank)
            return false;

        return roll_chance_f(config.shieldManaChance[rank - 1]);
    }

    // A share of base mana, so it doesn't grow with Intellect. Shows in the combat log as "You
    // gain 264 Mana from Redoubt." and, like any energize, adds a little threat.
    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        Unit* target = GetTarget();
        uint32 redoubtSpellId = 0;
        if (!GetRedoubtRank(target, redoubtSpellId))
            return;

        uint32 const mana = uint32(std::lround(target->GetCreateMana() * config.shieldManaPercent / 100.0f));
        if (mana)
            target->EnergizeBySpell(target, redoubtSpellId, mana, POWER_MANA);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pal_forever_shield_mana::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pal_forever_shield_mana::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
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

        config.shieldManaEnabled      = sConfigMgr->GetOption<bool>("ForeverPaladin.ShieldMana.Enable", true);
        config.shieldManaChance[0]    = sConfigMgr->GetOption<float>("ForeverPaladin.ShieldMana.ChanceRank1", 33.0f);
        config.shieldManaChance[1]    = sConfigMgr->GetOption<float>("ForeverPaladin.ShieldMana.ChanceRank2", 66.0f);
        config.shieldManaChance[2]    = sConfigMgr->GetOption<float>("ForeverPaladin.ShieldMana.ChanceRank3", 100.0f);
        config.shieldManaPercent      = sConfigMgr->GetOption<float>("ForeverPaladin.ShieldMana.BaseManaPercent", 6.0f);
        config.shieldManaCooldown     = sConfigMgr->GetOption<uint32>("ForeverPaladin.ShieldMana.Cooldown", 3000);

        // At startup the spells aren't loaded yet; OnBeforeWorldInitialized does it then.
        ApplyManaCost();
        ApplyShieldManaCooldown();
    }

    void OnBeforeWorldInitialized() override
    {
        ApplyManaCost();
        ApplyShieldManaCooldown();
    }
};

class ForeverPaladinPlayerScript : public PlayerScript
{
public:
    ForeverPaladinPlayerScript() : PlayerScript("ForeverPaladinPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        UpdateHolyStrike(player);

        // The Shield Specialization aura is passive, so it stays through death and isn't saved;
        // each login adds it again. With ShieldMana.Enable = 0 it does nothing.
        if (player->getClass() == CLASS_PALADIN && !player->HasAura(SPELL_SHIELD_MANA))
            player->AddAura(SPELL_SHIELD_MANA, player);
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
    RegisterSpellScript(spell_pal_forever_shield_mana);
}
