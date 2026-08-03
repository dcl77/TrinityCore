/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "pit_of_saron.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "Vehicle.h"

enum Spells
{
    SPELL_FIREBALL              = 69583, //Ymirjar Flamebearer
    SPELL_HELLFIRE              = 69586,
    SPELL_TACTICAL_BLINK        = 69584,
    SPELL_FROST_BREATH          = 69527, //Iceborn Proto-Drake
    SPELL_LEAPING_FACE_MAUL     = 69504, // Geist Ambusher
};

enum Events
{
    // Ymirjar Flamebearer
    EVENT_FIREBALL              = 1,
    EVENT_TACTICAL_BLINK        = 2,
};

bool ScheduledIcicleSummons::Execute(uint64 /*time*/, uint32 /*diff*/)
{
    if (roll_chance_i(12))
    {
        _trigger->CastSpell(_trigger, SPELL_ICICLE_SUMMON, true);
        _trigger->m_Events.AddEvent(new ScheduledIcicleSummons(_trigger), _trigger->m_Events.CalculateTime(randtime(20s, 35s)));
    }
    else
        _trigger->m_Events.AddEvent(new ScheduledIcicleSummons(_trigger), _trigger->m_Events.CalculateTime(randtime(1s, 20s)));

    return true;
}

struct npc_ymirjar_flamebearer: public ScriptedAI
{
    npc_ymirjar_flamebearer(Creature* creature) : ScriptedAI(creature)
    {
    }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EVENT_FIREBALL, 4s);
        _events.ScheduleEvent(EVENT_TACTICAL_BLINK, 15s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_FIREBALL:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(target, SPELL_FIREBALL);
                    _events.RescheduleEvent(EVENT_FIREBALL, 5s);
                    break;
                case EVENT_TACTICAL_BLINK:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(target, SPELL_TACTICAL_BLINK);
                    DoCast(me, SPELL_HELLFIRE);
                    _events.RescheduleEvent(EVENT_TACTICAL_BLINK, 12s);
                    break;
                default:
                    break;
            }
        }

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

struct npc_iceborn_protodrake: public ScriptedAI
{
    npc_iceborn_protodrake(Creature* creature) : ScriptedAI(creature)
    {
        Initialize();
    }

    void Initialize()
    {
        _frostBreathCooldown = 5000;
    }

    void Reset() override
    {
        Initialize();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (Vehicle* _vehicle = me->GetVehicleKit())
            _vehicle->RemoveAllPassengers();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (_frostBreathCooldown < diff)
        {
            DoCastVictim(SPELL_FROST_BREATH);
            _frostBreathCooldown = 10000;
        }
        else
            _frostBreathCooldown -= diff;

        DoMeleeAttackIfReady();
    }

private:
    uint32 _frostBreathCooldown;
};

struct npc_geist_ambusher: public ScriptedAI
{
    npc_geist_ambusher(Creature* creature) : ScriptedAI(creature)
    {
        Initialize();
    }

    void Initialize()
    {
        _leapingFaceMaulCooldown = 9000;
    }

    void Reset() override
    {
        Initialize();
    }

    void JustEngagedWith(Unit* who) override
    {
        if (who->GetTypeId() != TYPEID_PLAYER)
            return;

        // the max range is determined by aggro range
        if (me->GetDistance(who) > 5.0f)
            DoCast(who, SPELL_LEAPING_FACE_MAUL);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (_leapingFaceMaulCooldown < diff)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 5.0f, true))
                DoCast(target, SPELL_LEAPING_FACE_MAUL);
            _leapingFaceMaulCooldown = urand(9000, 14000);
        }
        else
            _leapingFaceMaulCooldown -= diff;

        DoMeleeAttackIfReady();
    }

private:
    uint32 _leapingFaceMaulCooldown;
};

struct npc_pit_of_saron_icicle : public PassiveAI
{
    npc_pit_of_saron_icicle(Creature* creature) : PassiveAI(creature)
    {
        me->SetDisplayId(me->GetCreatureTemplate()->Modelid1);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        _summonerGUID = summoner->GetGUID();

        _scheduler.Schedule(Milliseconds(3650), [this](TaskContext /*context*/)
        {
            DoCastSelf(SPELL_ICICLE_FALL_TRIGGER, true);
            DoCastSelf(SPELL_ICICLE_FALL_VISUAL);

            if (Unit* caster = ObjectAccessor::GetUnit(*me, _summonerGUID))
                caster->RemoveDynObject(SPELL_ICICLE_SUMMON);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
    ObjectGuid _summonerGUID;
};

// 70827 - Ice Shards
class spell_pos_ice_shards : public SpellScript
{
    PrepareSpellScript(spell_pos_ice_shards);

    bool Load() override
    {
        // This script should execute only in Pit of Saron
        return InstanceHasScript(GetCaster(), PoSScriptName);
    }

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        if (GetHitPlayer())
            GetCaster()->GetInstanceScript()->SetData(DATA_ICE_SHARDS_HIT, 1);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_pos_ice_shards::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

enum TyrannusEventCavernEmote
{
    SAY_TYRANNUS_CAVERN_ENTRANCE = 3
};

class at_pit_cavern_entrance : public AreaTriggerScript
{
    public:
        at_pit_cavern_entrance() : AreaTriggerScript("at_pit_cavern_entrance") { }

        bool OnTrigger(Player* player, AreaTriggerEntry const* /*areaTrigger*/) override
        {
            if (InstanceScript* instance = player->GetInstanceScript())
            {
                if (instance->GetData(DATA_CAVERN_ACTIVE))
                    return true;

                instance->SetData(DATA_CAVERN_ACTIVE, 1);

                if (Creature* tyrannus = ObjectAccessor::GetCreature(*player, instance->GetGuidData(DATA_TYRANNUS_EVENT)))
                    tyrannus->AI()->Talk(SAY_TYRANNUS_CAVERN_ENTRANCE);
            }
            return true;
        }
};

class at_pit_cavern_end : public AreaTriggerScript
{
public:
    at_pit_cavern_end() : AreaTriggerScript("at_pit_cavern_end") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*areaTrigger*/) override
    {
        if (InstanceScript* instance = player->GetInstanceScript())
        {
            instance->SetData(DATA_CAVERN_ACTIVE, 0);

            if (!instance->GetData(DATA_ICE_SHARDS_HIT))
                instance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, SPELL_DONT_LOOK_UP_ACHIEV_CREDIT, 0, player);
        }

        return true;
    }
};

enum IntroSpells
{
    SPELL_STRANGULATING_INTRO       = 69413,
    SPELL_NECROMANTIC_POWER_INTRO   = 69753,
};

enum IntroNPCs
{
    NPC_CORRUPTED_CHAMPION_INTRO    = 36796,
};

enum IntroState
{
    INTRO_STATE_NONE        = 0,
    INTRO_STATE_TALKING     = 1,
    INTRO_STATE_COMBAT      = 2,
    INTRO_STATE_FINISHED    = 3
};

class npc_jaina_sylvanas_pos_intro : public CreatureScript
{
public:
    npc_jaina_sylvanas_pos_intro() : CreatureScript("npc_jaina_sylvanas_pos_intro") { }

    struct npc_jaina_sylvanas_pos_introAI : public ScriptedAI
    {
        npc_jaina_sylvanas_pos_introAI(Creature* creature) : ScriptedAI(creature)
        {
            _instance = me->GetInstanceScript();
            _introState = INTRO_STATE_NONE;
            _corruptedCount = 0;
        }

        void Reset() override
        {
            _scheduler.CancelAll();

            if (_instance)
            {
                if (_instance->GetData(DATA_INTRO_STATE) == DONE)
                {
                    _introState = INTRO_STATE_FINISHED;
                    me->DespawnOrUnsummon();
                    return;
                }
                else if (_introState == INTRO_STATE_TALKING || _introState == INTRO_STATE_COMBAT)
                {
                    _instance->SetData(DATA_INTRO_STATE, NOT_STARTED);
                }
            }

            _introState = INTRO_STATE_NONE;
            _corruptedCount = 0;

            for (ObjectGuid guid : _corruptedGUIDs)
            {
                if (Creature* corrupted = ObjectAccessor::GetCreature(*me, guid))
                {
                    corrupted->DespawnOrUnsummon();
                }
            }
            _corruptedGUIDs.clear();

            me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);
            me->SetVisible(true);
            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
        }

        bool OnGossipHello(Player* player) override
        {
            player->PrepareQuestMenu(me->GetGUID());
            player->SendPreparedQuest(me->GetGUID());

            if (_instance && _instance->GetData(DATA_INTRO_STATE) == NOT_STARTED && _introState == INTRO_STATE_NONE)
            {
                if (player->GetQuestStatus(24498) == QUEST_STATUS_INCOMPLETE ||
                    player->GetQuestStatus(24507) == QUEST_STATUS_INCOMPLETE ||
                    player->GetQuestStatus(24498) == QUEST_STATUS_COMPLETE ||
                    player->GetQuestStatus(24507) == QUEST_STATUS_COMPLETE)
                {
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "We are ready, let's begin.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
                }
            }

            SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            if (gossipListId == GOSSIP_ACTION_INFO_DEF + 1)
            {
                CloseGossipMenuFor(player);
                StartIntroEvent();
                return true;
            }
            return false;
        }

        void OnQuestAccept(Player* /*player*/, Quest const* quest) override
        {
            if (quest->GetQuestId() == 24498 || quest->GetQuestId() == 24507)
            {
                StartIntroEvent();
            }
        }

        void StartIntroEvent()
        {
            if (_introState != INTRO_STATE_NONE || !_instance)
                return;

            if (_instance->GetData(DATA_INTRO_STATE) != NOT_STARTED)
                return;

            _introState = INTRO_STATE_TALKING;
            _instance->SetData(DATA_INTRO_STATE, IN_PROGRESS);
            me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);

            uint32 team = _instance->GetData(DATA_TEAM_IN_INSTANCE);

            // T = 0s: Lead NPC yells to attack, champions charge!
            if (team == ALLIANCE)
                Talk(6); // Heroes of the Alliance, attack!
            else
                Talk(5); // Soldiers of the Horde, attack!

            me->HandleEmoteCommand(EMOTE_ONESHOT_SHOUT);

            // Find champions near us and command them to run towards the necrolytes
            std::list<Creature*> champions;
            if (team == ALLIANCE)
            {
                me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_1_ALLIANCE, 30.0f);
                me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_2_ALLIANCE, 30.0f);
            }
            else
            {
                me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_1_HORDE, 30.0f);
                me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_2_HORDE, 30.0f);
                me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_3_HORDE, 30.0f);
            }

            for (Creature* champion : champions)
            {
                champion->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
                champion->SetWalk(false);
                champion->GetMotionMaster()->MovePoint(1, 485.0f, 220.0f, 528.8f);
            }

            // T = 4s: Scourgelord Tyrannus on Rimefang yells
            _scheduler.Schedule(4s, [this](TaskContext /*context*/)
            {
                if (Creature* tyrannus = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(DATA_TYRANNUS_EVENT)))
                {
                    tyrannus->AI()->Talk(4); // Intruders have entered the master's domain. Signal the alarms!
                }
            });

            // T = 9s: Scourgelord Tyrannus on Rimefang yells again
            _scheduler.Schedule(9s, [this](TaskContext /*context*/)
            {
                if (Creature* tyrannus = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(DATA_TYRANNUS_EVENT)))
                {
                    tyrannus->AI()->Talk(5); // Hrmph, fodder...
                }
            });

            // T = 12s: Tyrannus chokes the champions
            _scheduler.Schedule(12s, [this, team](TaskContext /*context*/)
            {
                Creature* tyrannus = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(DATA_TYRANNUS_EVENT));
                if (tyrannus)
                {
                    tyrannus->AI()->Talk(6); // Your last waking memory...
                    tyrannus->CastSpell(tyrannus, SPELL_NECROMANTIC_POWER_INTRO, true);
                }

                std::list<Creature*> champions;
                if (team == ALLIANCE)
                {
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_1_ALLIANCE, 60.0f);
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_2_ALLIANCE, 60.0f);
                }
                else
                {
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_1_HORDE, 60.0f);
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_2_HORDE, 60.0f);
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_3_HORDE, 60.0f);
                }

                for (Creature* champion : champions)
                {
                    champion->CastSpell(champion, SPELL_STRANGULATING_INTRO, true);
                }
            });

            // T = 18s: Kill the champions and summon Corrupted Champions!
            _scheduler.Schedule(18s, [this, team](TaskContext /*context*/)
            {
                Creature* tyrannus = ObjectAccessor::GetCreature(*me, _instance->GetGuidData(DATA_TYRANNUS_EVENT));
                if (tyrannus)
                {
                    tyrannus->AI()->Talk(7); // Minions, destroy these interlopers!
                    tyrannus->GetMotionMaster()->MovePoint(1, 539.634f, 222.141f, 580.0f);
                    tyrannus->DespawnOrUnsummon(5s);
                }

                if (team == ALLIANCE)
                {
                    Talk(7); // NO! YOU MONSTER!
                    _scheduler.Schedule(2s, [this](TaskContext /*context*/)
                    {
                        Talk(8); // I do what I must. Please forgive me, noble soldiers.
                    });
                }
                else
                    Talk(6); // Pathetic weaklings.

                std::list<Creature*> champions;
                if (team == ALLIANCE)
                {
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_1_ALLIANCE, 60.0f);
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_2_ALLIANCE, 60.0f);
                }
                else
                {
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_1_HORDE, 60.0f);
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_2_HORDE, 60.0f);
                    me->GetCreatureListWithEntryInGrid(champions, NPC_CHAMPION_3_HORDE, 60.0f);
                }

                for (Creature* champion : champions)
                {
                    Position pos = champion->GetPosition();
                    champion->RemoveAurasDueToSpell(SPELL_STRANGULATING_INTRO);
                    champion->DespawnOrUnsummon();

                    if (Creature* corrupted = me->SummonCreature(NPC_CORRUPTED_CHAMPION_INTRO, pos, TEMPSUMMON_MANUAL_DESPAWN))
                    {
                        if (corrupted->AI())
                            corrupted->AI()->DoZoneInCombat();
                        _corruptedGUIDs.push_back(corrupted->GetGUID());
                        _corruptedCount++;
                    }
                }

                _introState = INTRO_STATE_COMBAT;
            });
        }

        void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
        {
            if (summon->GetEntry() == NPC_CORRUPTED_CHAMPION_INTRO)
            {
                if (_corruptedCount > 0)
                    _corruptedCount--;

                if (_corruptedCount == 0 && _introState == INTRO_STATE_COMBAT)
                {
                    FinishIntroEvent();
                }
            }
        }

        void FinishIntroEvent()
        {
            _introState = INTRO_STATE_FINISHED;

            uint32 team = _instance ? _instance->GetData(DATA_TEAM_IN_INSTANCE) : ALLIANCE;

            // Dialogue ending sequence
            if (team == ALLIANCE)
            {
                Talk(9); // You will have to make your way across this quarry on your own.
                _scheduler.Schedule(6s, [this](TaskContext /*context*/)
                {
                    Talk(10); // Free any Alliance slaves...
                });
            }
            else
            {
                Talk(7); // You will have to battle your way through this cesspit on your own.
                _scheduler.Schedule(6s, [this](TaskContext /*context*/)
                {
                    Talk(8); // Free any Horde slaves...
                });
            }

            // Despawn Jaina/Sylvanas after dialogue finishes (12s total)
            _scheduler.Schedule(12s, [this](TaskContext /*context*/)
            {
                if (_instance)
                {
                    _instance->SetData(DATA_INTRO_STATE, DONE);
                }
                me->DespawnOrUnsummon();
            });
        }

        void UpdateAI(uint32 diff) override
        {
            _scheduler.Update(diff);

            if (_introState == INTRO_STATE_COMBAT)
            {
                // Help players in combat by casting frostbolt or autoattacking nearby enemies
                if (!me->IsInCombat())
                {
                    for (ObjectGuid guid : _corruptedGUIDs)
                    {
                        if (Creature* corrupted = ObjectAccessor::GetCreature(*me, guid))
                        {
                            if (corrupted->IsAlive())
                            {
                                AttackStart(corrupted);
                                break;
                            }
                        }
                    }
                }
            }

            if (me->IsInCombat() && _introState == INTRO_STATE_COMBAT)
            {
                // Avoid moving too far or running away, just standard attack
                DoMeleeAttackIfReady();
            }
        }

    private:
        InstanceScript* _instance;
        TaskScheduler _scheduler;
        uint8 _introState;
        uint8 _corruptedCount;
        std::vector<ObjectGuid> _corruptedGUIDs;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_jaina_sylvanas_pos_introAI(creature);
    }
};

void AddSC_pit_of_saron()
{
    RegisterPitOfSaronCreatureAI(npc_ymirjar_flamebearer);
    RegisterPitOfSaronCreatureAI(npc_iceborn_protodrake);
    RegisterPitOfSaronCreatureAI(npc_geist_ambusher);
    RegisterPitOfSaronCreatureAI(npc_pit_of_saron_icicle);
    RegisterSpellScript(spell_pos_ice_shards);
    new at_pit_cavern_entrance();
    new at_pit_cavern_end();
    new npc_jaina_sylvanas_pos_intro();
}
