/*
 * mod-forever-paladin
 *
 * Every paladin learns Holy Strike at level 6, whatever their spec, as in WoW Forever. It's an
 * instant weapon strike on the global cooldown, like Crusader Strike: 40% of weapon damage plus
 * 1.8 per level plus 42.9% of Holy spell power, all Holy damage, then a 12 second cooldown. Like
 * Crusader Strike, it costs 5% of base mana.
 *
 * The spell is Holy Strike (13953), a spell the 3.3.5 client already has: Blizzard only ever gave
 * it to NPCs (Scarlet Crusade and others), and no item, trainer or talent teaches it. The game
 * data makes it a next-swing attack; the module makes it instant, so players need the client
 * patch (tools/build_patch.py), which does the same, gives it a tooltip that says what it does and
 * puts it in the Holy tab of the spellbook. Without the patch the client treats the button as a
 * next-swing attack.
 *
 * NPCs that cast Holy Strike keep their damage, cooldown and cost; they strike instantly too.
 *
 * Shield Specialization's mana return: in WoW Forever that talent also gives blocks a 33% chance
 * to restore 6% of maximum mana, at most once every 3 seconds. Here it's 6% of base mana, so
 * Intellect doesn't make it bigger. 3.3.5 has no Shield Specialization
 * (its block bonus became part of Redoubt), so here Redoubt gets the mana return, with the chance
 * set per Redoubt rank. Every paladin carries a hidden aura, an unused server-side stub (67553),
 * that procs when they block; the proc only pays out while they have Redoubt, so talent resets
 * and dual spec need no extra handling. The combat log credits the mana to Redoubt.
 *
 * Seal of Fury: WoW Forever's tanking seal. Paladins learn it at level 10. Each melee hit deals
 * extra Holy damage, like Seal of Righteousness, and with a shield equipped also grants Fury Ward,
 * an absorb worth 50% of that Holy damage. Judging it deals Holy damage and taunts the target for
 * 4 seconds. It's four new spells (90080-90083) that the server gets from spell_dbc and the client
 * from patch-P (tools/build_patch.py); the core's own Judgement script casts Judgement of Fury,
 * because the seal names it in its third effect like every other seal.
 *
 * Glyph of Seal of Fury, a major glyph like Glyph of Seal of Command: each Judgement you use with
 * Seal of Fury active returns 8% of your base mana. Scribes make it with the same recipe, trainer
 * and materials as Glyph of Seal of Command. The glyph (90084), its item's spell (90085), the
 * recipe (90086), the mana (90087) and the glyph item (37550, Blizzard's unused "Deprecated Test
 * Glyph 2") come from the SQL, and for the client from patch-P, which also carries its
 * GlyphProperties.dbc and Item.dbc rows.
 *
 * Blessings and Judgements, as in WoW Forever: Blessings and Greater Blessings last 1 hour, and
 * Judgement debuffs last 40 seconds, except Judgement of Justice. The server changes the spells'
 * durations when it starts. With 1 hour blessings, the glyphs that make Blessing of Might and
 * Blessing of Wisdom last 20 minutes longer on yourself would be pointless, so they now make those
 * blessings 50% cheaper instead, like Glyph of Blessing of Kings already does.
 *
 * Released under the MIT License.
 */

#include "Config.h"
#include "DBCStores.h"
#include "Log.h"
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
#include <unordered_map>

namespace
{
    // Must match the SQL and tools/build_patch.py.
    constexpr uint32 SPELL_HOLY_STRIKE = 13953;

    // New spells: must match the SQL and tools/build_patch.py.
    constexpr uint32 SPELL_SEAL_OF_FURY = 90080;
    constexpr uint32 SPELL_JUDGEMENT_OF_FURY = 90081;
    constexpr uint32 SPELL_SEAL_OF_FURY_DAMAGE = 90082;
    constexpr uint32 SPELL_FURY_WARD = 90083;
    constexpr uint32 SPELL_GLYPH_OF_SEAL_OF_FURY = 90084;
    // The glyph's mana, like 68082 for Glyph of Seal of Command. The glyph aura is hidden, so the
    // client leaves mana credited to it out of the combat log; this one shows.
    constexpr uint32 SPELL_GLYPH_OF_SEAL_OF_FURY_MANA = 90087;

    // First ranks; the later ranks are found through the spell chains.
    constexpr std::array<uint32, 8> SPELL_BLESSINGS = {
        19740, // Blessing of Might
        19742, // Blessing of Wisdom
        20217, // Blessing of Kings
        20911, // Blessing of Sanctuary
        25782, // Greater Blessing of Might
        25894, // Greater Blessing of Wisdom
        25898, // Greater Blessing of Kings
        25899, // Greater Blessing of Sanctuary
    };

    // The debuffs Judgements leave on the target. Judgement of Justice (20184) keeps 20 seconds.
    constexpr std::array<uint32, 6> SPELL_JUDGEMENT_DEBUFFS = {
        20185,               // Judgement of Light
        20186,               // Judgement of Wisdom
        21183, 54498, 54499, // Heart of the Crusader
        68055,               // Judgements of the Just
    };

    // The glyph auras, with the family flag of the blessing they make cheaper.
    constexpr uint32 SPELL_GLYPH_OF_BLESSING_OF_MIGHT = 57958;
    constexpr uint32 SPELL_GLYPH_OF_BLESSING_OF_WISDOM = 57979;
    constexpr uint32 FAMILY_FLAG_BLESSING_OF_MIGHT = 0x00000002;
    constexpr uint32 FAMILY_FLAG_BLESSING_OF_WISDOM = 0x00010000;

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
        float weaponPercent = 40.0f;
        float damagePerLevel = 1.8f;
        float attackPowerCoefficient = 0.0f;
        float spellPowerCoefficient = 0.429f;
        uint32 manaCostPercent = 5;

        bool shieldManaEnabled = true;
        std::array<float, 3> shieldManaChance = { 33.0f, 66.0f, 100.0f };
        float shieldManaPercent = 6.0f;
        uint32 shieldManaCooldown = 3000;

        bool sealOfFuryEnabled = true;
        uint8 sealOfFuryLevel = 10;
        float sealOfFuryAttackPowerCoefficient = 0.022f;
        float sealOfFurySpellPowerCoefficient = 0.044f;
        float furyWardPercent = 50.0f;
        float glyphManaPercent = 8.0f;

        uint32 blessingDuration = 3600000;
        uint32 judgementDuration = 40000;
        int32 glyphCostReduction = 50;
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

    // Make Holy Strike an instant strike on the global cooldown, like Crusader Strike, instead of
    // a next-swing attack. The client patch makes the same change. This runs after the core's
    // spell_cooldown_overrides row for 13953 (which sets no global cooldown), so it wins.
    void ApplyInstant()
    {
        SpellInfo* spellInfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_HOLY_STRIKE));
        if (!spellInfo)
            return;

        spellInfo->Attributes &= ~(SPELL_ATTR0_ON_NEXT_SWING | SPELL_ATTR0_ON_NEXT_SWING_NO_DAMAGE);
        spellInfo->StartRecoveryCategory = 133; // the global cooldown, as on Crusader Strike
        spellInfo->StartRecoveryTime = 1500;
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

    // The game data's durations, saved the first time the module changes a spell, so that a
    // setting of 0 or a config reload can put them back.
    std::unordered_map<uint32, SpellDurationEntry const*> stockDurations;

    // The SpellDuration.dbc entry for a fixed duration in milliseconds. nullptr for 0 (keep the
    // game data's), or when the client has no such entry, which is logged.
    SpellDurationEntry const* FindDurationEntry(uint32 durationMs, char const* setting)
    {
        if (!durationMs)
            return nullptr;

        for (uint32 i = 0; i < sSpellDurationStore.GetNumRows(); ++i)
        {
            SpellDurationEntry const* entry = sSpellDurationStore.LookupEntry(i);
            if (entry && entry->Duration[0] == int32(durationMs) && entry->Duration[1] == 0 && entry->Duration[2] == int32(durationMs))
                return entry;
        }

        LOG_ERROR("module", "mod-forever-paladin: {} = {} isn't a duration SpellDuration.dbc has; keeping the game data's durations.", setting, durationMs);
        return nullptr;
    }

    void SetDurationEntry(uint32 spellId, SpellDurationEntry const* entry)
    {
        SpellInfo* spellInfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));
        if (!spellInfo)
            return;

        SpellDurationEntry const* stock = stockDurations.try_emplace(spellId, spellInfo->DurationEntry).first->second;
        spellInfo->DurationEntry = entry ? entry : stock;
    }

    // Blessings last 1 hour and Judgement debuffs 40 seconds, as in WoW Forever. Auras cast
    // before a config reload keep the duration they were cast with.
    void ApplyDurations()
    {
        // The first config load comes before the spells and DBCs are loaded.
        if (!sSpellMgr->GetSpellInfo(SPELL_BLESSINGS[0]))
            return;

        SpellDurationEntry const* blessing = FindDurationEntry(config.blessingDuration, "ForeverPaladin.Blessings.Duration");
        for (uint32 firstRank : SPELL_BLESSINGS)
            for (SpellInfo const* rank = sSpellMgr->GetSpellInfo(firstRank); rank; rank = rank->GetNextRankSpell())
                SetDurationEntry(rank->Id, blessing);

        SpellDurationEntry const* judgement = FindDurationEntry(config.judgementDuration, "ForeverPaladin.Judgements.Duration");
        for (uint32 spellId : SPELL_JUDGEMENT_DEBUFFS)
            SetDurationEntry(spellId, judgement);
    }

    struct StockGlyph
    {
        int32 durationMinutes;       // effect 1's base points
        SpellEffectInfo emptyEffect; // effect 2, unused in the game data
    };

    std::unordered_map<uint32, StockGlyph> stockGlyphs;

    // Glyph of Blessing of Might and Glyph of Blessing of Wisdom make the blessing last 20
    // minutes longer when you cast it on yourself, which means little once blessings last an
    // hour. Make them cut its mana cost instead, like Glyph of Blessing of Kings: effect 2
    // becomes the same cost modifier Kings' glyph has, for the glyph's own blessing.
    void ApplyGlyph(uint32 glyphId, uint32 blessingFamilyFlag)
    {
        SpellInfo* glyph = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(glyphId));
        if (!glyph)
            return;

        StockGlyph const& stock = stockGlyphs.try_emplace(glyphId,
            StockGlyph{ glyph->Effects[EFFECT_0].BasePoints, glyph->Effects[EFFECT_1] }).first->second;

        SpellEffectInfo& durationEffect = glyph->Effects[EFFECT_0];
        SpellEffectInfo& costEffect = glyph->Effects[EFFECT_1];

        if (config.glyphCostReduction <= 0)
        {
            durationEffect.BasePoints = stock.durationMinutes;
            costEffect = stock.emptyEffect;
            return;
        }

        // The core adds effect 1's amount, in minutes, to self-cast blessings (Object.cpp,
        // CalcSpellDuration). Its amount is BasePoints + 1, so this makes it add nothing.
        durationEffect.BasePoints = -1;

        costEffect.Effect = SPELL_EFFECT_APPLY_AURA;
        costEffect.ApplyAuraName = SPELL_AURA_ADD_PCT_MODIFIER;
        costEffect.MiscValue = SPELLMOD_COST;
        costEffect.DieSides = 1;
        costEffect.BasePoints = -config.glyphCostReduction - 1;
        costEffect.TargetA = durationEffect.TargetA;
        costEffect.SpellClassMask = flag96(blessingFamilyFlag, 0, 0);
    }

    void ApplyGlyphs()
    {
        ApplyGlyph(SPELL_GLYPH_OF_BLESSING_OF_MIGHT, FAMILY_FLAG_BLESSING_OF_MIGHT);
        ApplyGlyph(SPELL_GLYPH_OF_BLESSING_OF_WISDOM, FAMILY_FLAG_BLESSING_OF_WISDOM);
    }

    // Teach or remove a spell so it matches the paladin's level and its Enable setting.
    // Turning a spell off takes it away again at the next login.
    void UpdateSpell(Player* player, uint32 spellId, bool enabled, uint8 level)
    {
        bool const shouldKnow = enabled && player->GetLevel() >= level;
        bool const knows = player->HasSpell(spellId);

        if (shouldKnow && !knows)
            player->learnSpell(spellId);
        else if (!shouldKnow && knows)
            player->removeSpell(spellId, SPEC_MASK_ALL, false);
    }

    void UpdateSpells(Player* player)
    {
        if (player->getClass() != CLASS_PALADIN)
            return;

        UpdateSpell(player, SPELL_HOLY_STRIKE, config.enabled, config.level);
        UpdateSpell(player, SPELL_SEAL_OF_FURY, config.sealOfFuryEnabled, config.sealOfFuryLevel);
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

    // WoW Forever's Holy Strike: a share of weapon damage plus a flat amount (12 at rank 1,
    // 108 at level 60; here 1.8 per level, so 144 at 80) plus 42.9% of spell power. The game data
    // hits for full weapon damage, so scale that down and add the rest. This runs before crits
    // and resistances are worked out, so the whole hit can crit.
    void SetDamage()
    {
        Player* player = GetPlayerCaster();
        if (!player || GetHitDamage() <= 0)
            return;

        float const weapon = CalculatePct(float(GetHitDamage()), config.weaponPercent);
        float const attackPower = player->GetTotalAttackPowerValue(BASE_ATTACK);
        float const spellPower = float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY));
        float const bonus = player->GetLevel() * config.damagePerLevel
            + attackPower * config.attackPowerCoefficient + spellPower * config.spellPowerCoefficient;

        SetHitDamage(std::max<int32>(1, int32(weapon + bonus)));
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
        OnHit += SpellHitFn(spell_holy_strike::SetDamage);
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

// 90080 - Seal of Fury. Works like Seal of Righteousness: each melee hit deals extra Holy damage
// based on weapon speed, attack power and Holy spell power. Judging it casts Judgement of Fury
// (90081), named in the seal's third effect; that's the core's Judgement script.
class spell_pal_forever_seal_of_fury : public AuraScript
{
    PrepareAuraScript(spell_pal_forever_seal_of_fury);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SEAL_OF_FURY_DAMAGE, SPELL_JUDGEMENT_OF_FURY });
    }

    // A melee hit that did damage, not one of the seal's own Holy hits (those are triggered by
    // this aura).
    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* target = eventInfo.GetProcTarget();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();

        return target && target->IsAlive() && damageInfo && damageInfo->GetDamage() && !eventInfo.GetTriggerAuraSpell();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* paladin = GetTarget();
        Unit* victim = eventInfo.GetProcTarget();

        float const attackPower = paladin->GetTotalAttackPowerValue(BASE_ATTACK);
        int32 const holy = paladin->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY) + victim->SpellBaseDamageBonusTaken(SPELL_SCHOOL_MASK_HOLY);
        float const weaponSpeed = paladin->GetAttackTime(BASE_ATTACK) / 1000.0f;

        int32 const damage = std::max<int32>(0, int32((attackPower * config.sealOfFuryAttackPowerCoefficient
            + holy * config.sealOfFurySpellPowerCoefficient) * weaponSpeed));

        paladin->CastCustomSpell(SPELL_SEAL_OF_FURY_DAMAGE, SPELLVALUE_BASE_POINT0, damage, victim, true, nullptr, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_pal_forever_seal_of_fury::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_pal_forever_seal_of_fury::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 90081 - Judgement of Fury. With Glyph of Seal of Fury, each Judgement that hits returns a share
// of base mana, like Glyph of Seal of Command does for Judgement of Command. The glyph's aura is a
// plain dummy; this script does the work.
class spell_pal_forever_judgement_of_fury : public SpellScript
{
    PrepareSpellScript(spell_pal_forever_judgement_of_fury);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GLYPH_OF_SEAL_OF_FURY, SPELL_GLYPH_OF_SEAL_OF_FURY_MANA });
    }

    // Shows in the combat log as "You gain 121 Mana from Glyph of Seal of Fury."
    void ReturnMana()
    {
        Unit* caster = GetCaster();
        if (!caster || config.glyphManaPercent <= 0.0f || !caster->HasAura(SPELL_GLYPH_OF_SEAL_OF_FURY))
            return;

        uint32 const mana = uint32(std::lround(caster->GetCreateMana() * config.glyphManaPercent / 100.0f));
        if (mana)
            caster->EnergizeBySpell(caster, SPELL_GLYPH_OF_SEAL_OF_FURY_MANA, mana, POWER_MANA);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_pal_forever_judgement_of_fury::ReturnMana);
    }
};

// 90082 - Seal of Fury, the Holy damage. With a shield equipped, the paladin gets a Fury Ward
// (90083) that absorbs a share of the damage this hit really did. Wards don't stack: a new one
// only replaces the current one if it's bigger, and either way the ward lasts 10 seconds more.
class spell_pal_forever_seal_of_fury_damage : public SpellScript
{
    PrepareSpellScript(spell_pal_forever_seal_of_fury_damage);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FURY_WARD });
    }

    void GrantFuryWard()
    {
        Unit* caster = GetCaster();
        Player* paladin = caster ? caster->ToPlayer() : nullptr;
        if (!paladin || !paladin->GetShield(true) || config.furyWardPercent <= 0.0f)
            return;

        int32 const absorb = int32(CalculatePct(float(GetHitDamage()), config.furyWardPercent));
        if (absorb <= 0)
            return;

        if (AuraEffect* ward = paladin->GetAuraEffect(SPELL_FURY_WARD, EFFECT_0, paladin->GetGUID()))
        {
            if (ward->GetAmount() < absorb)
                ward->ChangeAmount(absorb);
            ward->GetBase()->RefreshDuration();
            return;
        }

        paladin->CastCustomSpell(SPELL_FURY_WARD, SPELLVALUE_BASE_POINT0, absorb, paladin, true);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_pal_forever_seal_of_fury_damage::GrantFuryWard);
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
        config.weaponPercent          = sConfigMgr->GetOption<float>("ForeverPaladin.HolyStrike.WeaponPercent", 40.0f);
        config.damagePerLevel         = sConfigMgr->GetOption<float>("ForeverPaladin.HolyStrike.DamagePerLevel", 1.8f);
        config.attackPowerCoefficient = sConfigMgr->GetOption<float>("ForeverPaladin.HolyStrike.AttackPowerCoefficient", 0.0f);
        config.spellPowerCoefficient  = sConfigMgr->GetOption<float>("ForeverPaladin.HolyStrike.SpellPowerCoefficient", 0.429f);
        config.manaCostPercent        = sConfigMgr->GetOption<uint32>("ForeverPaladin.HolyStrike.ManaCostPercent", 5);

        config.shieldManaEnabled      = sConfigMgr->GetOption<bool>("ForeverPaladin.ShieldMana.Enable", true);
        config.shieldManaChance[0]    = sConfigMgr->GetOption<float>("ForeverPaladin.ShieldMana.ChanceRank1", 33.0f);
        config.shieldManaChance[1]    = sConfigMgr->GetOption<float>("ForeverPaladin.ShieldMana.ChanceRank2", 66.0f);
        config.shieldManaChance[2]    = sConfigMgr->GetOption<float>("ForeverPaladin.ShieldMana.ChanceRank3", 100.0f);
        config.shieldManaPercent      = sConfigMgr->GetOption<float>("ForeverPaladin.ShieldMana.BaseManaPercent", 6.0f);
        config.shieldManaCooldown     = sConfigMgr->GetOption<uint32>("ForeverPaladin.ShieldMana.Cooldown", 3000);

        config.sealOfFuryEnabled                = sConfigMgr->GetOption<bool>("ForeverPaladin.SealOfFury.Enable", true);
        config.sealOfFuryLevel                  = uint8(sConfigMgr->GetOption<uint32>("ForeverPaladin.SealOfFury.Level", 10));
        config.sealOfFuryAttackPowerCoefficient = sConfigMgr->GetOption<float>("ForeverPaladin.SealOfFury.AttackPowerCoefficient", 0.022f);
        config.sealOfFurySpellPowerCoefficient  = sConfigMgr->GetOption<float>("ForeverPaladin.SealOfFury.SpellPowerCoefficient", 0.044f);
        config.furyWardPercent                  = sConfigMgr->GetOption<float>("ForeverPaladin.SealOfFury.WardPercent", 50.0f);
        config.glyphManaPercent                 = sConfigMgr->GetOption<float>("ForeverPaladin.SealOfFury.GlyphManaPercent", 8.0f);

        config.blessingDuration   = sConfigMgr->GetOption<uint32>("ForeverPaladin.Blessings.Duration", 3600000);
        config.judgementDuration  = sConfigMgr->GetOption<uint32>("ForeverPaladin.Judgements.Duration", 40000);
        config.glyphCostReduction = sConfigMgr->GetOption<int32>("ForeverPaladin.Glyphs.BlessingCostReduction", 50);

        // At startup the spells aren't loaded yet; OnBeforeWorldInitialized does it then.
        ApplySpellChanges();
    }

    void OnBeforeWorldInitialized() override
    {
        ApplySpellChanges();
    }

private:
    static void ApplySpellChanges()
    {
        ApplyManaCost();
        ApplyInstant();
        ApplyShieldManaCooldown();
        ApplyDurations();
        ApplyGlyphs();
    }
};

class ForeverPaladinPlayerScript : public PlayerScript
{
public:
    ForeverPaladinPlayerScript() : PlayerScript("ForeverPaladinPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        UpdateSpells(player);

        // The Shield Specialization aura is passive, so it stays through death and isn't saved;
        // each login adds it again. With ShieldMana.Enable = 0 it does nothing.
        if (player->getClass() == CLASS_PALADIN && !player->HasAura(SPELL_SHIELD_MANA))
            player->AddAura(SPELL_SHIELD_MANA, player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        UpdateSpells(player);
    }
};

void AddForeverPaladinScripts()
{
    new ForeverPaladinWorldScript();
    new ForeverPaladinPlayerScript();
    RegisterSpellScript(spell_holy_strike);
    RegisterSpellScript(spell_pal_forever_shield_mana);
    RegisterSpellScript(spell_pal_forever_seal_of_fury);
    RegisterSpellScript(spell_pal_forever_judgement_of_fury);
    RegisterSpellScript(spell_pal_forever_seal_of_fury_damage);
}
