#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Creature.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include <sstream>
#include <string>

class npc_spectator : public CreatureScript
{
public:
    npc_spectator() : CreatureScript("npc_spectator") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->GetViewpoint())
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Stop Spectating", GOSSIP_SENDER_MAIN, 100);
        }
        else
        {
            // List active arenas
            for (int i = BATTLEGROUND_QUEUE_2v2; i <= BATTLEGROUND_QUEUE_5v5; ++i)
            {
                BattlegroundTypeId bgTypeId = BattlegroundMgr::BGTemplateId(BattlegroundQueueTypeId(i));
                Battleground* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);
                if (!bgTemplate)
                    continue;

                auto const& bgDataStore = sBattlegroundMgr->GetBattlegroundDataStore();
                auto itData = bgDataStore.find(bgTypeId);
                if (itData == bgDataStore.end())
                    continue;

                auto const& bgData = itData->second;
                for (auto const& pair : bgData.m_Battlegrounds)
                {
                    Battleground* bg = pair.second.get();
                    if (!bg || !bg->isArena() || bg->GetStatus() != STATUS_IN_PROGRESS)
                        continue;

                    ArenaTeam* team1 = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdByIndex(TEAM_ALLIANCE));
                    ArenaTeam* team2 = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdByIndex(TEAM_HORDE));

                    std::string team1Name = team1 ? team1->GetName() : "Unknown";
                    std::string team2Name = team2 ? team2->GetName() : "Unknown";

                    std::ostringstream oss;
                    oss << "Spectate: " << team1Name << " vs " << team2Name << " (" << bg->GetClientInstanceID() << ")";
                    AddGossipItemFor(player, GOSSIP_ICON_BATTLE, oss.str(), GOSSIP_SENDER_MAIN, 1000 + bg->GetInstanceID());
                }
            }
        }

        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 action) override
    {
        if (action == 100)
        {
            player->SetViewpoint(nullptr, false);
            CloseGossipMenuFor(player);
        }
        else if (action >= 1000)
        {
            uint32 instanceId = action - 1000;
            // This is a bit tricky as we don't know the bgTypeId here easily from instanceId alone without searching.
            // But we can search through all bg types.
            Battleground* targetBg = nullptr;
            for (auto const& dataPair : sBattlegroundMgr->GetBattlegroundDataStore())
            {
                auto it = dataPair.second.m_Battlegrounds.find(instanceId);
                if (it != dataPair.second.m_Battlegrounds.end())
                {
                    targetBg = it->second.get();
                    break;
                }
            }

            if (targetBg)
            {
                // Find a player to spectate
                if (!targetBg->GetPlayers().empty())
                {
                    ObjectGuid targetGuid = targetBg->GetPlayers().begin()->first;
                    if (Player* targetPlayer = ObjectAccessor::FindPlayer(targetGuid))
                    {
                        player->SetViewpoint(targetPlayer, true);
                    }
                }
            }
            CloseGossipMenuFor(player);
        }

        return true;
    }
};

void AddSC_npc_spectator()
{
    new npc_spectator();
}
