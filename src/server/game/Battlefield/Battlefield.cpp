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

#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "Battleground.h"
#include "BattlegroundPackets.h"
#include "CellImpl.h"
#include "CreatureTextMgr.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "MiscPackets.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "WorldPacket.h"
#include "WorldStatePackets.h"
#include <G3D/g3dmath.h>

Battlefield::Battlefield()
{
    m_Timer = 0;
    m_IsEnabled = true;
    m_isActive = false;
    m_DefenderTeam = TEAM_NEUTRAL;

    m_TypeId = 0;
    m_BattleId = 0;
    m_ZoneId = 0;
    m_MapId = 0;
    m_Map = nullptr;
    m_MaxPlayer = 0;
    m_MinPlayer = 0;

    for (int8 i = 0; i < PVP_TEAMS_COUNT; i++)
        m_freeslots[i] = 0;

    m_MinLevel = 0;
    m_BattleTime = 0;
    m_NoWarBattleTime = 0;
    m_TimeForAcceptInvite = 20;

    m_LastResurrectTimer = 30 * IN_MILLISECONDS;
    m_StartGroupingTimer = 0;
    m_StartGrouping = false;
}

Battlefield::~Battlefield()
{
    StalkerGuid.Clear();
    m_Timer = 0;
    m_IsEnabled = false;
    m_isActive = false;
    m_DefenderTeam = TEAM_ALLIANCE;

    m_PlayerMap.clear();

    m_TypeId                = 0;
    m_BattleId              = 0;
    m_ZoneId                = 0;
    m_MapId                 = 0;
    m_Map = nullptr;
    m_MaxPlayer             = 0;
    m_MinPlayer             = 0;
    for (int8 i = 0; i < PVP_TEAMS_COUNT; i++)
        m_freeslots[i]      = 0;
    m_TimeForAcceptInvite   = 0;
    m_MinLevel              = 0;
    m_BattleTime            = 0;
    m_NoWarBattleTime       = 0;
    m_StartGroupingTimer    = 0;
    m_LastResurrectTimer    = 0;

    KickPosition.Relocate(0, 0, 0, 0);
    KickPosition.m_mapId = 0;

    m_StartGrouping = false;
    for (int8 i = 0; i < PVP_TEAMS_COUNT; i++)
        m_Groups[i].clear();

    m_Data64.clear();
    m_Data32.clear();
}

void Battlefield::Update(uint32 diff)
{
    if (m_Timer <= diff)
    {
        // Battlefield ends on time
        if (IsWarTime())
            EndBattle(true);
        else // Time to start a new battle!
            StartBattle();
    }
    else
        m_Timer -= diff;

    // Invite players a few minutes before the battle's beginning
    if (!IsWarTime() && !m_StartGrouping && m_Timer <= m_StartGroupingTimer)
    {
        m_StartGrouping = true;
        InvitePlayersInZoneToQueue();
        OnStartGrouping();
    }

    time_t now = GameTime::GetGameTime();
    std::set<ObjectGuid>_playerkick;
    for (PlayerHolderContainer::iterator itr = m_PlayerMap.begin(); itr != m_PlayerMap.end(); ++itr)
    {
        // check estimation time for awaiting war and waiting to kick
        if (itr->second.isWaitingWar || itr->second.isWaitingKick)
        {
            if (itr->second.time <= now)
                _playerkick.insert(itr->first);
        }
    }

    for (auto itr = _playerkick.begin(); itr != _playerkick.end(); ++itr)
        KickPlayerFromBattlefield(*itr);
    _playerkick.clear();

    if (m_LastResurrectTimer <= diff)
    {
        for (uint8 i = 0; i < m_GraveyardList.size(); i++)
            if (GetGraveyardById(i))
                m_GraveyardList[i]->Resurrect();
        m_LastResurrectTimer = RESURRECTION_INTERVAL;
    }
    else
        m_LastResurrectTimer -= diff;

    if (IsWarTime())
    {
        for (BfCapturePointMap::iterator itr = m_capturePoints.begin(); itr != m_capturePoints.end(); ++itr)
            itr->second->Update(diff);
    }
}

void Battlefield::InitStalker(uint32 entry, Position const& pos)
{
    if (Creature* creature = SpawnCreature(entry, pos))
        StalkerGuid = creature->GetGUID();
    else
        TC_LOG_ERROR("bg.battlefield", "Battlefield::InitStalker: Could not spawn Stalker (Creature entry {}), zone messages will be unavailable!", entry);
}

void Battlefield::SendWarning(uint8 id, WorldObject const* target /*= nullptr*/)
{
    if (Creature* stalker = GetCreature(StalkerGuid))
        sCreatureTextMgr->SendChat(stalker, id, target);
}

void Battlefield::StartBattle()
{
    if (m_isActive)
        return;

    for (uint8 team = 0; team < PVP_TEAMS_COUNT; ++team)
        m_Groups[team].clear();

    m_Timer = m_BattleTime;
    m_isActive = true;

    InvitePlayersInQueueToWar();
    TryInvitePlayersNotInQueueToWarOrKickThem();

    OnBattleStart();
}

void Battlefield::EndBattle(bool endByTimer)
{
    if (!m_isActive)
        return;

    m_isActive = false;

    m_StartGrouping = false;

    if (!endByTimer)
        SetDefenderTeam(GetAttackerTeam());

    for (PlayerHolderContainer::iterator itr = m_PlayerMap.begin(); itr != m_PlayerMap.end(); ++itr)
    {
        itr->second.inQueue = false;
        itr->second.inWar = false;
        itr->second.isWaitingQueue = false;
        itr->second.isWaitingWar = false;
        itr->second.isWaitingKick = false;
        itr->second.time = 0;
    }

    for (uint8 i = 0; i < PVP_TEAMS_COUNT; i++)
        m_freeslots[i] = m_MaxPlayer;

    OnBattleEnd(endByTimer);
    InvitePlayersInZoneToQueue();

    // Reset bat0tlefield timer
    m_Timer = m_NoWarBattleTime;
    SendInitWorldStatesToAll();
}

<<<<<<< HEAD
=======
void Battlefield::DoPlaySoundToAll(uint32 soundID)
{
    BroadcastPacketToWar(WorldPackets::Misc::PlaySound(soundID).Write());
}

bool Battlefield::HasPlayer(Player* player) const
{
    return m_players[player->GetTeamId()].find(player->GetGUID()) != m_players[player->GetTeamId()].end();
}

// Called in WorldSession::HandleBfQueueInviteResponse
void Battlefield::PlayerAcceptInviteToQueue(Player* player)
{
    // Add player in queue
    m_PlayersInQueue[player->GetTeamId()].insert(player->GetGUID());
    // Send notification
    player->GetSession()->SendBfQueueInviteResponse(m_BattleId, m_ZoneId);
}

// Called in WorldSession::HandleBfQueueExitRequest
void Battlefield::AskToLeaveQueue(Player* player)
{
    // Remove player from queue
    m_PlayersInQueue[player->GetTeamId()].erase(player->GetGUID());
}

// Called in WorldSession::HandleHearthAndResurrect
void Battlefield::PlayerAskToLeave(Player* player)
{
    // Player leaving Wintergrasp, teleport to Dalaran.
    // ToDo: confirm teleport destination.
    player->TeleportTo(571, 5804.1499f, 624.7710f, 647.7670f, 1.6400f);
}

// Called in WorldSession::HandleBfEntryInviteResponse
void Battlefield::PlayerAcceptInviteToWar(Player* player)
{
    if (!IsWarTime())
        return;

    if (AddOrSetPlayerToCorrectBfGroup(player))
    {
        player->GetSession()->SendBfEntered(m_BattleId);
        m_PlayersInWar[player->GetTeamId()].insert(player->GetGUID());
        m_InvitedPlayers[player->GetTeamId()].erase(player->GetGUID());

        if (player->isAFK())
            player->ToggleAFK();

        OnPlayerJoinWar(player);                               //for scripting
    }
}

void Battlefield::TeamCastSpell(TeamId team, int32 spellId)
{
    if (spellId > 0)
    {
        for (auto itr = m_PlayersInWar[team].begin(); itr != m_PlayersInWar[team].end(); ++itr)
            if (Player* player = ObjectAccessor::FindPlayer(*itr))
                player->CastSpell(player, uint32(spellId), true);
    }
    else
    {
        for (auto itr = m_PlayersInWar[team].begin(); itr != m_PlayersInWar[team].end(); ++itr)
            if (Player* player = ObjectAccessor::FindPlayer(*itr))
                player->RemoveAuraFromStack(uint32(-spellId));
    }
}

void Battlefield::BroadcastPacketToZone(WorldPacket const* data) const
{
    for (uint8 team = 0; team < PVP_TEAMS_COUNT; ++team)
        for (auto itr = m_players[team].begin(); itr != m_players[team].end(); ++itr)
            if (Player* player = ObjectAccessor::FindPlayer(*itr))
                player->SendDirectMessage(data);
}

void Battlefield::BroadcastPacketToQueue(WorldPacket const* data) const
{
    for (uint8 team = 0; team < PVP_TEAMS_COUNT; ++team)
        for (auto itr = m_PlayersInQueue[team].begin(); itr != m_PlayersInQueue[team].end(); ++itr)
            if (Player* player = ObjectAccessor::FindConnectedPlayer(*itr))
                player->SendDirectMessage(data);
}

void Battlefield::BroadcastPacketToWar(WorldPacket const* data) const
{
    for (uint8 team = 0; team < PVP_TEAMS_COUNT; ++team)
        for (auto itr = m_PlayersInWar[team].begin(); itr != m_PlayersInWar[team].end(); ++itr)
            if (Player* player = ObjectAccessor::FindPlayer(*itr))
                player->SendDirectMessage(data);
}

void Battlefield::SendWarning(uint8 id, WorldObject const* target /*= nullptr*/)
{
    if (Creature* stalker = GetCreature(StalkerGuid))
        sCreatureTextMgr->SendChat(stalker, id, target);
}

void Battlefield::SendInitWorldStatesTo(Player* player)
{
    WorldPackets::WorldState::InitWorldStates packet;
    packet.MapID = m_MapId;
    packet.AreaID = m_ZoneId;
    packet.SubareaID = player->GetAreaId();
    FillInitialWorldStates(packet);

    player->SendDirectMessage(packet.Write());
}

void Battlefield::SendUpdateWorldState(uint32 variable, uint32 value)
{
    WorldPackets::WorldState::UpdateWorldState worldstate;
    worldstate.VariableID = variable;
    worldstate.Value = value;
    BroadcastPacketToZone(worldstate.Write());
}

>>>>>>> upstream/3.3.5
void Battlefield::RegisterZone(uint32 zoneId)
{
    sBattlefieldMgr->AddZone(zoneId, this);
}

void Battlefield::HideNpc(Creature* creature)
{
    creature->CombatStop();
    creature->SetReactState(REACT_PASSIVE);
    creature->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
    creature->SetRespawnDelay(RESPAWN_ONE_DAY);
    creature->DisappearAndDie();
    creature->SetVisible(false);
}

void Battlefield::ShowNpc(Creature* creature, bool aggressive)
{
    creature->SetVisible(true);
    creature->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
    if (!creature->IsAlive())
        creature->Respawn(true);
    if (aggressive)
        creature->SetReactState(REACT_AGGRESSIVE);
    else
    {
        creature->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        creature->SetReactState(REACT_PASSIVE);
    }
}

// ****************************************************
// ******************* Group System *******************
// ****************************************************
Group* Battlefield::GetFreeBfRaid(TeamId TeamId)
{
    for (auto itr = m_Groups[TeamId].begin(); itr != m_Groups[TeamId].end(); ++itr)
        if (Group* group = sGroupMgr->GetGroupByGUID(*itr))
            if (!group->IsFull())
                return group;

    return nullptr;
}

Group* Battlefield::GetGroupPlayer(ObjectGuid guid, TeamId TeamId)
{
    for (auto itr = m_Groups[TeamId].begin(); itr != m_Groups[TeamId].end(); ++itr)
        if (Group* group = sGroupMgr->GetGroupByGUID(*itr))
            if (group->IsMember(guid))
                return group;

    return nullptr;
}

bool Battlefield::AddOrSetPlayerToCorrectBfGroup(Player* player)
{
    if (!player->IsInWorld())
        return false;

    if (Group* group = player->GetGroup())
        group->RemoveMember(player->GetGUID());

    Group* group = GetFreeBfRaid(player->GetTeamId());
    if (!group)
    {
        group = new Group;
        group->SetBattlefieldGroup(this);
        group->Create(player);
        sGroupMgr->AddGroup(group);
        m_Groups[player->GetTeamId()].insert(group->GetGUID());
    }
    else if (group->IsMember(player->GetGUID()))
    {
        uint8 subgroup = group->GetMemberGroup(player->GetGUID());
        player->SetBattlegroundOrBattlefieldRaid(group, subgroup);
    }
    else
        group->AddMember(player);

    return true;
}

//***************End of Group System*******************

//*****************************************************
//***************Spirit Guide System*******************
//*****************************************************

//--------------------
//-Battlefield Method-
//--------------------
BfGraveyard* Battlefield::GetGraveyardById(uint32 id) const
{
    if (id < m_GraveyardList.size())
    {
        if (BfGraveyard* graveyard = m_GraveyardList.at(id))
            return graveyard;
        else
            TC_LOG_ERROR("bg.battlefield", "Battlefield::GetGraveyardById Id:{} does not exist.", id);
    }
    else
        TC_LOG_ERROR("bg.battlefield", "Battlefield::GetGraveyardById Id:{} could not be found.", id);

    return nullptr;
}

WorldSafeLocsEntry const* Battlefield::GetClosestGraveyard(Player* player)
{
    BfGraveyard* closestGY = nullptr;
    float maxdist = -1;
    for (uint8 i = 0; i < m_GraveyardList.size(); i++)
    {
        if (m_GraveyardList[i])
        {
            if (m_GraveyardList[i]->GetControlTeamId() != player->GetTeamId())
                continue;

            float dist = m_GraveyardList[i]->GetDistance(player);
            if (dist < maxdist || maxdist < 0)
            {
                closestGY = m_GraveyardList[i];
                maxdist = dist;
            }
        }
    }

    if (closestGY)
        return sDBCMgr->GetWorldSafeLocsEntry(closestGY->GetGraveyardId());

    return nullptr;
}

void Battlefield::AddPlayerToResurrectQueue(ObjectGuid npcGuid, ObjectGuid playerGuid)
{
    for (uint8 i = 0; i < m_GraveyardList.size(); i++)
    {
        if (!m_GraveyardList[i])
            continue;

        if (m_GraveyardList[i]->HasNpc(npcGuid))
        {
            m_GraveyardList[i]->AddPlayer(playerGuid);
            break;
        }
    }
}

void Battlefield::RemovePlayerFromResurrectQueue(ObjectGuid playerGuid)
{
    for (uint8 i = 0; i < m_GraveyardList.size(); i++)
    {
        if (!m_GraveyardList[i])
            continue;

        if (m_GraveyardList[i]->HasPlayer(playerGuid))
        {
            m_GraveyardList[i]->RemovePlayer(playerGuid);
            break;
        }
    }
}

<<<<<<< HEAD
=======
void Battlefield::SendAreaSpiritHealerQueryOpcode(Player* player, ObjectGuid guid)
{
    WorldPackets::Battleground::AreaSpiritHealerTime areaSpiritHealerTime;
    areaSpiritHealerTime.HealerGuid = guid;
    areaSpiritHealerTime.TimeLeft = m_LastResurrectTimer;
    player->SendDirectMessage(areaSpiritHealerTime.Write());
}

// ----------------------
// - BfGraveyard Method -
// ----------------------
BfGraveyard::BfGraveyard(Battlefield* bf)
{
    m_Bf = bf;
    m_GraveyardId = 0;
    m_ControlTeam = TEAM_NEUTRAL;
}

BfGraveyard::~BfGraveyard() = default;

void BfGraveyard::Initialize(TeamId startControl, uint32 graveyardId)
{
    m_ControlTeam = startControl;
    m_GraveyardId = graveyardId;
}

void BfGraveyard::SetSpirit(Creature* spirit, TeamId team)
{
    if (!spirit)
    {
        TC_LOG_ERROR("bg.battlefield", "BfGraveyard::SetSpirit: Invalid Spirit.");
        return;
    }

    m_SpiritGuide[team] = spirit->GetGUID();
    spirit->SetReactState(REACT_PASSIVE);
}

float BfGraveyard::GetDistance(Player* player)
{
    WorldSafeLocsEntry const* safeLoc = sWorldSafeLocsStore.LookupEntry(m_GraveyardId);
    return player->GetDistance2d(safeLoc->Loc.X, safeLoc->Loc.Y);
}

void BfGraveyard::AddPlayer(ObjectGuid playerGuid)
{
    if (!m_ResurrectQueue.count(playerGuid))
    {
        m_ResurrectQueue.insert(playerGuid);

        if (Player* player = ObjectAccessor::FindPlayer(playerGuid))
            player->CastSpell(player, SPELL_WAITING_FOR_RESURRECT, true);
    }
}

void BfGraveyard::RemovePlayer(ObjectGuid playerGuid)
{
    m_ResurrectQueue.erase(m_ResurrectQueue.find(playerGuid));

    if (Player* player = ObjectAccessor::FindPlayer(playerGuid))
        player->RemoveAurasDueToSpell(SPELL_WAITING_FOR_RESURRECT);
}

void BfGraveyard::Resurrect()
{
    if (m_ResurrectQueue.empty())
        return;

    for (GuidSet::const_iterator itr = m_ResurrectQueue.begin(); itr != m_ResurrectQueue.end(); ++itr)
    {
        // Get player object from his guid
        Player* player = ObjectAccessor::FindPlayer(*itr);
        if (!player)
            continue;

        // Check if the player is in world and on the good graveyard
        if (player->IsInWorld())
            if (Creature* spirit = m_Bf->GetCreature(m_SpiritGuide[m_ControlTeam]))
                spirit->CastSpell(spirit, SPELL_SPIRIT_HEAL, true);

        // Resurrect player
        player->CastSpell(player, SPELL_RESURRECTION_VISUAL, true);
        player->ResurrectPlayer(1.0f);
        player->CastSpell(player, 6962, true);
        player->CastSpell(player, SPELL_SPIRIT_HEAL_MANA, true);

        player->SpawnCorpseBones(false);
    }

    m_ResurrectQueue.clear();
}

// For changing graveyard control
void BfGraveyard::GiveControlTo(TeamId team)
{
    // Guide switching
    // Note: Visiblity changes are made by phasing
    /*if (m_SpiritGuide[1 - team])
        m_SpiritGuide[1 - team]->SetVisible(false);
    if (m_SpiritGuide[team])
        m_SpiritGuide[team]->SetVisible(true);*/

    m_ControlTeam = team;
    // Teleport to other graveyard, player witch were on this graveyard
    RelocateDeadPlayers();
}

void BfGraveyard::RelocateDeadPlayers()
{
    WorldSafeLocsEntry const* closestGrave = nullptr;
    for (GuidSet::const_iterator itr = m_ResurrectQueue.begin(); itr != m_ResurrectQueue.end(); ++itr)
    {
        Player* player = ObjectAccessor::FindPlayer(*itr);
        if (!player)
            continue;

        if (closestGrave)
            player->TeleportTo(player->GetMapId(), closestGrave->Loc.X, closestGrave->Loc.Y, closestGrave->Loc.Z, player->GetOrientation());
        else
        {
            closestGrave = m_Bf->GetClosestGraveyard(player);
            if (closestGrave)
                player->TeleportTo(player->GetMapId(), closestGrave->Loc.X, closestGrave->Loc.Y, closestGrave->Loc.Z, player->GetOrientation());
        }
    }
}

bool BfGraveyard::HasNpc(ObjectGuid guid)
{
    if (!m_SpiritGuide[TEAM_ALLIANCE] || !m_SpiritGuide[TEAM_HORDE])
        return false;

    if (!m_Bf->GetCreature(m_SpiritGuide[TEAM_ALLIANCE]) ||
        !m_Bf->GetCreature(m_SpiritGuide[TEAM_HORDE]))
        return false;

    return (m_SpiritGuide[TEAM_ALLIANCE] == guid || m_SpiritGuide[TEAM_HORDE] == guid);
}

>>>>>>> upstream/3.3.5
// *******************************************************
// *************** End Spirit Guide system ***************
// *******************************************************
// ********************** Misc ***************************
// *******************************************************

Creature* Battlefield::SpawnCreature(uint32 entry, Position const& pos)
{
    //Get map object
    if (!m_Map)
    {
        TC_LOG_ERROR("bg.battlefield", "Battlefield::SpawnCreature: Can't create creature entry: {}, map not found.", entry);
        return nullptr;
    }

    Creature* creature = new Creature();
    if (!creature->Create(m_Map->GenerateLowGuid<HighGuid::Unit>(), m_Map, PHASEMASK_NORMAL, entry, pos))
    {
        TC_LOG_ERROR("bg.battlefield", "Battlefield::SpawnCreature: Can't create creature entry: {}", entry);
        delete creature;
        return nullptr;
    }

    creature->SetHomePosition(pos);

    // Set creature in world
    m_Map->AddToMap(creature);
    creature->setActive(true);
    creature->SetFarVisible(true);

    return creature;
}

// Method for spawning gameobject on map
GameObject* Battlefield::SpawnGameObject(uint32 entry, Position const& pos, QuaternionData const& rot)
{
    //Get map object
    if (!m_Map)
    {
        TC_LOG_ERROR("bg.battlefield", "Battlefield::SpawnGameObject: Can't create gameobject entry: {}, map not found.", entry);
        return nullptr;
    }

    // Create gameobject
    GameObject* go = new GameObject;
    if (!go->Create(m_Map->GenerateLowGuid<HighGuid::GameObject>(), entry, m_Map, PHASEMASK_NORMAL, pos, rot, 255, GO_STATE_READY))
    {
        TC_LOG_ERROR("bg.battlefield", "Battlefield::SpawnGameObject: Gameobject template {} could not be found in the database! Battlefield has not been created!", entry);
        delete go;
        return nullptr;
    }

    // Add to world
    m_Map->AddToMap(go);
    go->setActive(true);
    go->SetFarVisible(true);

    return go;
}

Creature* Battlefield::GetCreature(ObjectGuid guid)
{
    if (!m_Map)
        return nullptr;
    return m_Map->GetCreature(guid);
}

GameObject* Battlefield::GetGameObject(ObjectGuid guid)
{
    if (!m_Map)
        return nullptr;
    return m_Map->GetGameObject(guid);
}
<<<<<<< HEAD
=======

// *******************************************************
// ******************* CapturePoint **********************
// *******************************************************

BfCapturePoint::BfCapturePoint(Battlefield* bf) : m_Bf(bf), m_capturePointGUID()
{
    m_team = TEAM_NEUTRAL;
    m_value = 0;
    m_minValue = 0.0f;
    m_maxValue = 0.0f;
    m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_NEUTRAL;
    m_OldState = BF_CAPTUREPOINT_OBJECTIVESTATE_NEUTRAL;
    m_capturePointEntry = 0;
    m_neutralValuePct = 0;
    m_maxSpeed = 0;
}

BfCapturePoint::~BfCapturePoint() = default;

bool BfCapturePoint::HandlePlayerEnter(Player* player)
{
    if (!m_capturePointGUID.IsEmpty())
    {
        if (GameObject* capturePoint = m_Bf->GetGameObject(m_capturePointGUID))
        {
            player->SendUpdateWorldState(capturePoint->GetGOInfo()->capturePoint.worldState1, 1);
            player->SendUpdateWorldState(capturePoint->GetGOInfo()->capturePoint.worldstate2, uint32(ceil((m_value + m_maxValue) / (2 * m_maxValue) * 100.0f)));
            player->SendUpdateWorldState(capturePoint->GetGOInfo()->capturePoint.worldstate3, m_neutralValuePct);
        }
    }

    return m_activePlayers[player->GetTeamId()].insert(player->GetGUID()).second;
}

GuidSet::iterator BfCapturePoint::HandlePlayerLeave(Player* player)
{
    if (!m_capturePointGUID.IsEmpty())
        if (GameObject* capturePoint = m_Bf->GetGameObject(m_capturePointGUID))
            player->SendUpdateWorldState(capturePoint->GetGOInfo()->capturePoint.worldState1, 0);

    GuidSet::iterator current = m_activePlayers[player->GetTeamId()].find(player->GetGUID());

    if (current == m_activePlayers[player->GetTeamId()].end())
        return current; // return end()

    m_activePlayers[player->GetTeamId()].erase(current++);
    return current;
}

void BfCapturePoint::SendChangePhase()
{
    if (!m_capturePointGUID)
        return;

    if (GameObject* capturePoint = m_Bf->GetGameObject(m_capturePointGUID))
    {
        // send this too, sometimes the slider disappears, dunno why :(
        SendUpdateWorldState(capturePoint->GetGOInfo()->capturePoint.worldState1, 1);
        // send these updates to only the ones in this objective
        SendUpdateWorldState(capturePoint->GetGOInfo()->capturePoint.worldstate2, (uint32) std::ceil((m_value + m_maxValue) / (2 * m_maxValue) * 100.0f));
        // send this too, sometimes it resets :S
        SendUpdateWorldState(capturePoint->GetGOInfo()->capturePoint.worldstate3, m_neutralValuePct);
    }
}

bool BfCapturePoint::SetCapturePointData(GameObject* capturePoint)
{
    ASSERT(capturePoint);

    TC_LOG_DEBUG("bg.battlefield", "Creating capture point {}", capturePoint->GetEntry());

    m_capturePointGUID = capturePoint->GetGUID();

    // check info existence
    GameObjectTemplate const* goinfo = capturePoint->GetGOInfo();
    if (goinfo->type != GAMEOBJECT_TYPE_CAPTURE_POINT)
    {
        TC_LOG_ERROR("misc", "OutdoorPvP: GO {} is not a capture point!", capturePoint->GetEntry());
        return false;
    }

    // get the needed values from goinfo
    m_maxValue = goinfo->capturePoint.maxTime;
    m_maxSpeed = m_maxValue / (goinfo->capturePoint.minTime ? goinfo->capturePoint.minTime : 60);
    m_neutralValuePct = goinfo->capturePoint.neutralPercent;
    m_minValue = m_maxValue * goinfo->capturePoint.neutralPercent / 100;
    m_capturePointEntry = capturePoint->GetEntry();
    if (m_team == TEAM_ALLIANCE)
    {
        m_value = m_maxValue;
        m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_ALLIANCE;
    }
    else
    {
        m_value = -m_maxValue;
        m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_HORDE;
    }

    return true;
}

GameObject* BfCapturePoint::GetCapturePointGo()
{
    return m_Bf->GetGameObject(m_capturePointGUID);
}

bool BfCapturePoint::DelCapturePoint()
{
    if (!m_capturePointGUID.IsEmpty())
    {
        if (GameObject* capturePoint = m_Bf->GetGameObject(m_capturePointGUID))
        {
            capturePoint->SetRespawnTime(0);                  // not save respawn time
            capturePoint->Delete();
            capturePoint = nullptr;
        }
        m_capturePointGUID.Clear();
    }

    return true;
}

bool BfCapturePoint::Update(uint32 diff)
{
    if (!m_capturePointGUID)
        return false;

    if (GameObject* capturePoint = m_Bf->GetGameObject(m_capturePointGUID))
    {
        float radius = capturePoint->GetGOInfo()->capturePoint.radius;

        for (uint8 team = 0; team < PVP_TEAMS_COUNT; ++team)
        {
            for (GuidSet::iterator itr = m_activePlayers[team].begin(); itr != m_activePlayers[team].end();)
            {
                if (Player* player = ObjectAccessor::FindPlayer(*itr))
                {
                    if (!capturePoint->IsWithinDistInMap(player, radius) || !player->IsOutdoorPvPActive())
                        itr = HandlePlayerLeave(player);
                    else
                        ++itr;
                }
                else
                    ++itr;
            }
        }

        std::list<Player*> players;
        Trinity::AnyPlayerInObjectRangeCheck checker(capturePoint, radius);
        Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(capturePoint, players, checker);
        Cell::VisitWorldObjects(capturePoint, searcher, radius);

        for (std::list<Player*>::iterator itr = players.begin(); itr != players.end(); ++itr)
            if ((*itr)->IsOutdoorPvPActive())
                if (m_activePlayers[(*itr)->GetTeamId()].insert((*itr)->GetGUID()).second)
                    HandlePlayerEnter(*itr);
    }

    // get the difference of numbers
    float fact_diff = ((float) m_activePlayers[TEAM_ALLIANCE].size() - (float) m_activePlayers[TEAM_HORDE].size()) * diff / float(BATTLEFIELD_OBJECTIVE_UPDATE_INTERVAL);
    if (G3D::fuzzyEq(fact_diff, 0.0f))
        return false;

    uint32 Challenger = 0;
    float maxDiff = m_maxSpeed * diff;

    if (fact_diff < 0)
    {
        // horde is in majority, but it's already horde-controlled -> no change
        if (m_State == BF_CAPTUREPOINT_OBJECTIVESTATE_HORDE && m_value <= -m_maxValue)
            return false;

        if (fact_diff < -maxDiff)
            fact_diff = -maxDiff;

        Challenger = HORDE;
    }
    else
    {
        // ally is in majority, but it's already ally-controlled -> no change
        if (m_State == BF_CAPTUREPOINT_OBJECTIVESTATE_ALLIANCE && m_value >= m_maxValue)
            return false;

        if (fact_diff > maxDiff)
            fact_diff = maxDiff;

        Challenger = ALLIANCE;
    }

    float oldValue = m_value;
    TeamId oldTeam = m_team;

    m_OldState = m_State;

    m_value += fact_diff;

    if (m_value < -m_minValue)                              // red
    {
        if (m_value < -m_maxValue)
            m_value = -m_maxValue;
        m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_HORDE;
        m_team = TEAM_HORDE;
    }
    else if (m_value > m_minValue)                          // blue
    {
        if (m_value > m_maxValue)
            m_value = m_maxValue;
        m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_ALLIANCE;
        m_team = TEAM_ALLIANCE;
    }
    else if (oldValue * m_value <= 0)                       // grey, go through mid point
    {
        // if challenger is ally, then n->a challenge
        if (Challenger == ALLIANCE)
            m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_NEUTRAL_ALLIANCE_CHALLENGE;
        // if challenger is horde, then n->h challenge
        else if (Challenger == HORDE)
            m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_NEUTRAL_HORDE_CHALLENGE;
        m_team = TEAM_NEUTRAL;
    }
    else                                                    // grey, did not go through mid point
    {
        // old phase and current are on the same side, so one team challenges the other
        if (Challenger == ALLIANCE && (m_OldState == BF_CAPTUREPOINT_OBJECTIVESTATE_HORDE || m_OldState == BF_CAPTUREPOINT_OBJECTIVESTATE_NEUTRAL_HORDE_CHALLENGE))
            m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_HORDE_ALLIANCE_CHALLENGE;
        else if (Challenger == HORDE && (m_OldState == BF_CAPTUREPOINT_OBJECTIVESTATE_ALLIANCE || m_OldState == BF_CAPTUREPOINT_OBJECTIVESTATE_NEUTRAL_ALLIANCE_CHALLENGE))
            m_State = BF_CAPTUREPOINT_OBJECTIVESTATE_ALLIANCE_HORDE_CHALLENGE;
        m_team = TEAM_NEUTRAL;
    }

    if (G3D::fuzzyNe(m_value, oldValue))
        SendChangePhase();

    if (m_OldState != m_State)
    {
        //TC_LOG_ERROR("bg.battlefield", "{}->{}", m_OldState, m_State);
        if (oldTeam != m_team)
            ChangeTeam(oldTeam);
        return true;
    }

    return false;
}

void BfCapturePoint::SendUpdateWorldState(uint32 field, uint32 value)
{
    for (uint8 team = 0; team < PVP_TEAMS_COUNT; ++team)
        for (GuidSet::iterator itr = m_activePlayers[team].begin(); itr != m_activePlayers[team].end(); ++itr)  // send to all players present in the area
            if (Player* player = ObjectAccessor::FindPlayer(*itr))
                player->SendUpdateWorldState(field, value);
}

void BfCapturePoint::SendObjectiveComplete(uint32 id, ObjectGuid guid)
{
    uint8 team;
    switch (m_State)
    {
        case BF_CAPTUREPOINT_OBJECTIVESTATE_ALLIANCE:
            team = TEAM_ALLIANCE;
            break;
        case BF_CAPTUREPOINT_OBJECTIVESTATE_HORDE:
            team = TEAM_HORDE;
            break;
        default:
            return;
    }

    // send to all players present in the area
    for (GuidSet::iterator itr = m_activePlayers[team].begin(); itr != m_activePlayers[team].end(); ++itr)
        if (Player* player = ObjectAccessor::FindPlayer(*itr))
            player->KilledMonsterCredit(id, guid);
}

bool BfCapturePoint::IsInsideObjective(Player* player) const
{
    return m_activePlayers[player->GetTeamId()].find(player->GetGUID()) != m_activePlayers[player->GetTeamId()].end();
}
>>>>>>> upstream/3.3.5
