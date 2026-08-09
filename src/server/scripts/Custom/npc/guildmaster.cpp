#include "Custom/Dcl.h"
#include "GOMove.h"
#include "Chat.h"

#define MSG_GOSSIP_TELE          "Teleport to GuildHouse"
#define MSG_GOSSIP_BUY           "Buy GuildHouse"
#define MSG_GOSSIP_SELL          "Sell GuildHouse"
#define MSG_GOSSIP_NEXTPAGE      "Next -->"
#define MSG_INCOMBAT             "You are in combat and cannot be teleported to your GuildHouse."
#define MSG_NOGUILDHOUSE         "Your guild currently does not own a GuildHouse."
#define MSG_NOFREEGH             "Unfortunately, all GuildHouses are in use."
#define MSG_ALREADYHAVEGH        "Sorry, but you already own a GuildHouse ({})."
#define MSG_NOTENOUGHMONEY       "You do not have the {} gold required to purchase a GuildHouse."
#define MSG_GHOCCUPIED           "This GuildHouse is unavailable for purchase as it is currently in use."
#define MSG_CONGRATULATIONS      "Congratulations! You have successfully purchased a GuildHouse."
#define MSG_SOLD                 "You have sold your GuildHouse and have received {} gold."
#define MSG_NOTINGUILD           "You need to be in a guild before you can use a GuildHouse."
#define MSG_SELL_CONFIRM         "Are you sure you want to sell your guildhouse for half the buy price?"

#define OFFSET_GH_ID_TO_ACTION 1500
#define OFFSET_SHOWBUY_FROM 10000

#define ACTION_TELE 1001
#define ACTION_SHOW_BUYLIST 1002 //deprecated. Use (OFFSET_SHOWBUY_FROM + 0) instead
#define ACTION_SELL_GUILDHOUSE 1003
#define ACTION_SHOW_OBJECT_BUYLIST 1004
#define OFFSET_OBJECT_ID_TO_ACTION 20000

#define COST_GH_BUY 10000000 //1000 g.
#define COST_GH_SELL 5000000 //500 g.

#define GOSSIP_COUNT_MAX 10

struct GuildHouseGameObject
{
    uint32 entry;
    std::string name;
    uint32 cost; // in gold
};

static const std::vector<GuildHouseGameObject> purchasableGameObjects = {
    { 184137, "Mailbox", 10 },
    { 187293, "Guild Bank", 20 },
    { 183317, "Portal to Stormwind", 15 },
    { 183325, "Portal to Orgrimmar", 15 },
    { 191167, "Portal to Dalaran", 15 },
    { 183323, "Portal to Shattrath", 15 },
    { 191028, "Barber Chair", 10 },
    { 180053, "Anvil", 5 },
    { 1685,   "Forge", 5 }
};

class guildmaster : public CreatureScript
{
public:
    guildmaster() : CreatureScript("guildmaster") { }

    struct guildmasterAI : public ScriptedAI
    {
        guildmasterAI(Creature* me) : ScriptedAI(me) { }

        bool isPlayerGuildLeader(Player* player)
        {
            //return (player->GetRank() == 0) && (player->GetGuildId() != 0);
            return (player->GetGuildRank() == 0) && (player->GetGuildId() != 0);
        }

        bool getGuildHouseCoords(uint32 guildId, float& x, float& y, float& z, uint32& map)
        {
            if (guildId == 0)
            {
                //if player has no guild
                return false;
            }

            QueryResult result;
            result = ZynDatabase.PQuery("SELECT `x`, `y`, `z`, `map` FROM `guildhouses` WHERE `guildId` = {}", guildId);
            if (result)
            {
                Field* fields = result->Fetch();
                x = fields[0].GetFloat();
                y = fields[1].GetFloat();
                z = fields[2].GetFloat();
                map = fields[3].GetUInt32();
                return true;
            }

            return false;
        }

        void showObjectBuyList(Player* player, Creature* _creature)
        {
            for (size_t i = 0; i < purchasableGameObjects.size(); ++i)
            {
                std::string label = purchasableGameObjects[i].name + " (" + std::to_string(purchasableGameObjects[i].cost) + " Gold)";
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, label, GOSSIP_SENDER_MAIN, OFFSET_OBJECT_ID_TO_ACTION + i);
            }

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, _creature->GetGUID());
        }

        void buyGameObject(Player* player, Creature* _creature, uint32 index)
        {
            if (index >= purchasableGameObjects.size())
                return;

            const GuildHouseGameObject& ghGo = purchasableGameObjects[index];

            if (player->GetMoney() < ghGo.cost * 10000)
            {
                _creature->Whisper("You do not have enough gold to purchase this object.", LANG_UNIVERSAL, player);
                return;
            }

            float x, y, z;
            uint32 mapId;
            if (getGuildHouseCoords(player->GetGuildId(), x, y, z, mapId))
            {
                if (player->GetMapId() != mapId)
                {
                    _creature->Whisper("You must be inside your GuildHouse to purchase GameObjects.", LANG_UNIVERSAL, player);
                    return;
                }

                float spawnX = player->GetPositionX();
                float spawnY = player->GetPositionY();
                float spawnZ = player->GetPositionZ();
                float spawnO = player->GetOrientation();

                GameObject* go = GOMove::SpawnGameObject(player, spawnX, spawnY, spawnZ, spawnO, player->GetPhaseMaskForSpawn(), ghGo.entry);
                if (go)
                {
                    uint32 goLowguid = go->GetSpawnId();
                    ZynDatabase.PQuery("INSERT INTO `guildhouse_gameobjects` (`guildId`, `gameobject_guid`) VALUES ({}, {})",
                        player->GetGuildId(), goLowguid);

                    player->ModifyMoney(-(int32)(ghGo.cost * 10000));
                    _creature->Whisper("GameObject successfully added to your GuildHouse!", LANG_UNIVERSAL, player);
                }
                else
                {
                    _creature->Whisper("Failed to spawn the GameObject. Please try again.", LANG_UNIVERSAL, player);
                }
            }
            else
            {
                _creature->Whisper("Failed to locate your GuildHouse.", LANG_UNIVERSAL, player);
            }
        }

        void teleportPlayerToGuildHouse(Player* player, Creature* _creature)
        {
            if (player->GetGuildId() == 0)
            {
                //if player has no guild
                _creature->Whisper(MSG_NOTINGUILD, LANG_UNIVERSAL, player);
                return;
            }

            if (!player->getAttackers().empty())
            {
                //if player in combat
                _creature->Say(MSG_INCOMBAT, LANG_UNIVERSAL, player);
                return;
            }

            float x, y, z;
            uint32 map;

            if (getGuildHouseCoords(player->GetGuildId(), x, y, z, map))
            {
                //teleport player to the specified location
                player->TeleportTo(map, x, y, z, 0.0f);
            }
            else
                _creature->Whisper(MSG_NOGUILDHOUSE, LANG_UNIVERSAL, player);
        }

        bool showBuyList(Player* player, Creature* _creature, uint32 showFromId = 0)
        {
            //show not occupied guildhouses

            QueryResult result;
            result = ZynDatabase.PQuery("SELECT `id`, `comment` FROM `guildhouses` WHERE `guildId` = 0 AND `id` > {} ORDER BY `id` ASC LIMIT {}",
                showFromId, GOSSIP_COUNT_MAX);

            if (result)
            {
                uint32 guildhouseId = 0;
                std::string comment = "";
                do
                {
                    Field* fields = result->Fetch();
                    guildhouseId = fields[0].GetInt32();
                    comment = fields[1].GetString();

                    //send comment as a gossip item
                    //transmit guildhouseId in Action variable
                    AddGossipItemFor(player, GOSSIP_ICON_TABARD, comment, GOSSIP_SENDER_MAIN,
                        guildhouseId + OFFSET_GH_ID_TO_ACTION);
                } while (result->NextRow());

                if (result->GetRowCount() == GOSSIP_COUNT_MAX)
                {
                    //assume that we have additional page
                    //add link to next GOSSIP_COUNT_MAX items
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, MSG_GOSSIP_NEXTPAGE, GOSSIP_SENDER_MAIN,
                        guildhouseId + OFFSET_SHOWBUY_FROM);
                }

                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, _creature->GetGUID());

                return true;
            }
            else if (!result)
            {
                //all guildhouses are occupied
                _creature->Whisper(MSG_NOFREEGH, LANG_UNIVERSAL, player);
                CloseGossipMenuFor(player);
            }
            else
            {
                //this condition occurs when COUNT(guildhouses) % GOSSIP_COUNT_MAX == 0
                //just show GHs from beginning
                showBuyList(player, _creature, 0);
            }

            return false;
        }

        bool isPlayerHasGuildhouse(Player* player, Creature* _creature, bool whisper = false)
        {
            QueryResult result;
            result = ZynDatabase.PQuery("SELECT `comment` FROM `guildhouses` WHERE `guildId` = {}",
                player->GetGuildId());

            if (result)
            {
                if (whisper)
                {
                    //whisper to player "already have etc..."
                    Field* fields = result->Fetch();
                    char msg[100];

                    std::string strtext = sObjectMgr->GetTrinityStringForDBCLocale2(NOT_USED_69);
                    char const* text = strtext.c_str();

                    snprintf(msg, sizeof(msg), text, fields[0].GetCString());
                    _creature->Whisper(msg, LANG_UNIVERSAL, player);
                }

                return true;
            }

            return false;
        }

        void buyGuildhouse(Player* player, Creature* _creature, uint32 guildhouseId)
        {

            bool token = sGameConfig->GetBoolConfig("GuildHouse.TokenOrGold");
            int32 cost = sGameConfig->GetIntConfig("GuildHouse.Cost");

            if ((int32)player->GetMoney() <= (int32)cost)
            {
                //show how much money player need to buy GH (in gold)
                char msg[100];

                std::string strtext = sObjectMgr->GetTrinityStringForDBCLocale2(NOT_USED_70);
                char const* text = strtext.c_str();

                snprintf(msg, sizeof(msg), text, cost);

                _creature->Whisper(msg, LANG_UNIVERSAL, player);
                return;
            }

            if (isPlayerHasGuildhouse(player, _creature, true))
            {
                //player already have GH
                return;
            }

            QueryResult result;

            //check if somebody already occupied this GH
            result = ZynDatabase.PQuery("SELECT `id` FROM `guildhouses` WHERE `id` = {} AND `guildId` <> 0", guildhouseId);

            if (result)
            {
                _creature->Whisper(MSG_GHOCCUPIED, LANG_UNIVERSAL, player);
                return;
            }

            //update DB
            result = ZynDatabase.PQuery("UPDATE `guildhouses` SET `guildId` = {} WHERE `id` = {}",
                player->GetGuildId(), guildhouseId);
            uint32 myMoney = sGameConfig->GetIntConfig("GuildHouse.BuyCost");
            player->ModifyMoney(-(int32)myMoney * 10000);

            player->DestroyItemCount(token, cost, true);
            _creature->Say(MSG_CONGRATULATIONS, LANG_UNIVERSAL, player);

        }

        void sellGuildhouse(Player* player, Creature* _creature)
        {
            if (isPlayerHasGuildhouse(player, _creature))
            {
                // Delete purchased GameObjects
                QueryResult goResult = ZynDatabase.PQuery("SELECT `gameobject_guid` FROM `guildhouse_gameobjects` WHERE `guildId` = {}", player->GetGuildId());
                if (goResult)
                {
                    do
                    {
                        Field* fields = goResult->Fetch();
                        uint32 goLowguid = fields[0].GetUInt32();

                        GameObject* go = ChatHandler(player->GetSession()).GetObjectFromPlayerMapByDbGuid(goLowguid);
                        if (go)
                        {
                            go->Delete();
                        }
                        GameObject::DeleteFromDB(goLowguid);
                    } while (goResult->NextRow());

                    ZynDatabase.PQuery("DELETE FROM `guildhouse_gameobjects` WHERE `guildId` = {}", player->GetGuildId());
                }

                QueryResult result;
                result = ZynDatabase.PQuery("UPDATE `guildhouses` SET `guildId` = 0 WHERE `guildId` = {}",
                    player->GetGuildId());
                uint32 myMoneys = sGameConfig->GetIntConfig("GuildHouse.SellCost");

                player->ModifyMoney(myMoneys * 10000);
                //display message e.g. "here your money etc."
                char msg[255];

                std::string strtext = sObjectMgr->GetTrinityStringForDBCLocale2(NOT_USED_71);
                char const* text = strtext.c_str();

                snprintf(msg, sizeof(msg), text, myMoneys);
                _creature->Whisper(msg, LANG_UNIVERSAL, player);
            }
        }

        bool OnGossipSelect(Player* player, uint32 /*menu_id*/, uint32 gossipListId) override
        {
            uint32 sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
            uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            return GossipSelect(player, sender, action);
        }

        bool GossipSelect(Player* player, uint32 sender, uint32 action)
        {
            player->PlayerTalkClass->ClearMenus();
            if (sender != GOSSIP_SENDER_MAIN)
                return false;

            switch (action)
            {
            case ACTION_TELE:
                //teleport player to GH
                CloseGossipMenuFor(player);
                teleportPlayerToGuildHouse(player, me);
                break;
            case ACTION_SHOW_BUYLIST:
                //show list of GHs which currently not occupied
                showBuyList(player, me);
                break;
            case ACTION_SHOW_OBJECT_BUYLIST:
                showObjectBuyList(player, me);
                break;
            case ACTION_SELL_GUILDHOUSE:
                sellGuildhouse(player, me);
                CloseGossipMenuFor(player);
                break;
            default:
                if (action >= OFFSET_OBJECT_ID_TO_ACTION)
                {
                    buyGameObject(player, me, action - OFFSET_OBJECT_ID_TO_ACTION);
                    CloseGossipMenuFor(player);
                }
                else if (action > OFFSET_SHOWBUY_FROM)
                {
                    showBuyList(player, me, action - OFFSET_SHOWBUY_FROM);
                }
                else if (action > OFFSET_GH_ID_TO_ACTION)
                {
                    //player clicked on buy list
                    CloseGossipMenuFor(player);

                    //get guildhouseId from action
                    //guildhouseId = action - OFFSET_GH_ID_TO_ACTION
                    buyGuildhouse(player, me, action - OFFSET_GH_ID_TO_ACTION);
                }
                break;
            }

            return true;
        }

        bool OnGossipHello(Player* player) override
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, MSG_GOSSIP_TELE, GOSSIP_SENDER_MAIN, ACTION_TELE);

            if (isPlayerGuildLeader(player))
            {
                if (isPlayerHasGuildhouse(player, me))
                {
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Add GameObjects to GuildHouse", GOSSIP_SENDER_MAIN, ACTION_SHOW_OBJECT_BUYLIST);
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, MSG_GOSSIP_SELL, GOSSIP_SENDER_MAIN, ACTION_SELL_GUILDHOUSE, MSG_SELL_CONFIRM, 0, false);
                }
                else
                {
                    //show additional menu for guild leader
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, MSG_GOSSIP_BUY, GOSSIP_SENDER_MAIN, ACTION_SHOW_BUYLIST);
                }
            }

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
            return true;
        }

    };

    CreatureAI* GetAI(Creature* me) const override
    {
        return new guildmasterAI(me);
    }
};

void AddSC_guildmaster()
{
    ZynDatabase.DirectExecute("CREATE TABLE IF NOT EXISTS `guildhouse_gameobjects` ("
                              "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
                              "`guildId` BIGINT NOT NULL,"
                              "`gameobject_guid` INT UNSIGNED NOT NULL,"
                              "PRIMARY KEY (`id`)"
                              ") ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;");
    new guildmaster();
}
