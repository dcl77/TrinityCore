#include "AccountMgr.h"
#include "Channel.h"
#include "ChannelAppenders.h"
#include "CharacterDatabase.h"
#include "Chat.h"
#include "CustomConfig.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Guild.h"
#include "Language.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Mail.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "StringConvert.h"
#include "AchievementMgr.h"
#include "Item.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "World.h"
#include "WorldSession.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include <mutex>

static std::unordered_map<ObjectGuid, uint32> sPlayerSurvivalTimers;
static std::unordered_map<ObjectGuid, uint32> sPlayerTradeTimers;
static std::mutex sHardcoreTimerMutex;

class HardcoreModePlayerScript : public PlayerScript
{
public:
    HardcoreModePlayerScript() : PlayerScript("HardcoreModePlayerScript") {}

    void OnLogin(Player* player, bool /**/) override
    {
        if (IsHardcoreEnabled() && IsHardcorePlayer(player))
        {
            ChatHandler(player->GetSession()).SendSysMessage("You are playing in HARDCORE mode! Death = Character Deletion!");
            if (HasExtraLife(player))
            {
                ChatHandler(player->GetSession()).SendSysMessage(
                    "|cff00ff00You have an extra life! Upon death, you will resurrect with 50% health, but you will lose this bonus.|r");
            }

            CheckMissingLevelRewards(player);
            CheckTimeRewards(player);

            // Add visual effects (Hardcore Aura)
            player->CastSpell(player, 61573, true);
        }
    }

    void OnLogout(Player* player) override
    {
        std::lock_guard<std::mutex> lock(sHardcoreTimerMutex);
        sPlayerSurvivalTimers.erase(player->GetGUID());
        sPlayerTradeTimers.erase(player->GetGUID());
    }

    void OnUpdate(Player* player, uint32 diff) override
    {
        if (!IsHardcoreEnabled() || !IsHardcorePlayer(player))
        {
            return;
        }

        std::lock_guard<std::mutex> lock(sHardcoreTimerMutex);
        sPlayerSurvivalTimers[player->GetGUID()] += diff;

        // Check survival time rewards every 60 seconds
        if (sPlayerSurvivalTimers[player->GetGUID()] >= 60000)
        {
            CheckTimeRewards(player);
            sPlayerSurvivalTimers[player->GetGUID()] = 0;
        }
    }

uint32 GetPlayerTotalTime(Player* player)
{
    // Если нет записи в hardcore_playtime, возвращаем базовое время
    return player->GetTotalPlayedTime();
}

    void CheckTimeRewards(Player* player)
    {
        uint32 totalTime = GetPlayerTotalTime(player);

        QueryResult rewards = CharacterDatabase.Query(
            "SELECT id, required_time, item_id, item_count, money, title_id, achievement_id, spell_id, name, experience "
            "FROM hardcore_time_rewards WHERE enabled = 1 ORDER BY required_time");

        if (!rewards)
            return;

        do
        {
            Field* fields = rewards->Fetch();
            uint32 rewardId = fields[0].GetUInt32();
            uint32 requiredTime = fields[1].GetUInt32();

            if (totalTime >= requiredTime && !HasClaimedTimeReward(player, rewardId))
            {
                GiveTimeReward(player, rewardId, fields);
            }
        }
        while (rewards->NextRow());
    }

    bool HasClaimedTimeReward(Player* player, uint32 rewardId)
    {
        QueryResult result = CharacterDatabase.PQuery(
            "SELECT 1 FROM hardcore_rewards_claimed WHERE guid = {} AND reward_id = {}",
            player->GetGUID().GetCounter(), rewardId);

        return result != nullptr;
    }

    void GiveTimeReward(Player* player, uint32 rewardId, Field* fields)
    {
        uint32 itemId = fields[2].GetUInt32();
        uint32 itemCount = fields[3].GetUInt32();
        uint32 money = fields[4].GetUInt32();
        uint32 titleId = fields[5].GetUInt32();
        uint32 achievementId = fields[6].GetUInt32();
        uint32 spellId = fields[7].GetUInt32();
        std::string name = fields[8].GetString();
		uint32 experience = fields[9].GetUInt32();

        // Выдаем предмет
        if (itemId)
        {
            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, itemCount);

            if (msg == EQUIP_ERR_OK)
            {
                Item* item = player->StoreNewItem(dest, itemId, true);
                if (item)
                {
                    player->SendNewItem(item, itemCount, true, false);
                }
            }
            else
            {
                // Отправляем по почте если инвентарь заполнен
                SendRewardByMail(player, itemId, itemCount, name);
            }
        }

        // Выдаем деньги
        if (money)
        {
            player->ModifyMoney(money);
        }

        // Выдаем титул
        if (titleId)
        {
            CharTitlesEntry const* titleInfo = sDBCMgr->GetCharTitlesEntry(titleId);
            if (titleInfo)
            {
                player->SetTitle(titleInfo);
            }
        }

        // Выдаем достижение
        if (achievementId)
        {
            AchievementEntry const* achievement = sAchievementStore.LookupEntry(achievementId);
            if (achievement)
            {
                player->CompletedAchievement(achievement);
            }
        }

        // Применяем заклинание
        if (spellId)
        {
            player->CastSpell(player, spellId, true);
        }

        if (experience)
        {
            player->GiveXP(experience, nullptr, false);
        }

        // Save survival reward claim information
        CharacterDatabase.PExecute(
            "INSERT INTO hardcore_rewards_claimed (guid, reward_id, claimed_time, playtime_when_claimed) "
            "VALUES ({}, {}, {}, {})",
            player->GetGUID().GetCounter(), rewardId, GameTime::GetGameTime(), GetPlayerTotalTime(player));

        // Notify player
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff00ff00[HARDCORE] You have received a survival time reward: %s|r", name.c_str());

        // Global announcement
        std::string announcement = "|cff00ff00Player " + player->GetName() +
            " received hardcore reward: " + name + "!|r";
        sWorld->SendServerMessage(SERVER_MSG_STRING, announcement.c_str());
    }

    void SendRewardByMail(Player* player, uint32 itemId, uint32 itemCount, const std::string& rewardName)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        Item* mailItem = Item::CreateItem(itemId, itemCount, player);

        if (mailItem)
        {
            mailItem->SaveToDB(trans);

            MailSender sender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM);
            MailDraft draft("Hardcore Time Reward", "Congratulations! You have received a reward: " + rewardName);
            draft.AddItem(mailItem);
            draft.SendMailTo(trans, MailReceiver(player, player->GetGUID().GetCounter()), sender);

            CharacterDatabase.CommitTransaction(trans);

            ChatHandler(player->GetSession()).SendSysMessage(
                "|cff00ff00[HARDCORE] Your inventory is full. Reward sent by mail.|r");
        }
    }

void CheckMissingLevelRewards(Player* player)
{
    uint8 currentLevel = player->GetLevel();

    // Получаем список всех доступных наград
    QueryResult rewards = CharacterDatabase.PQuery(
        "SELECT level FROM hardcore_level_rewards WHERE level <= {} ORDER BY level", currentLevel);

    if (!rewards)
        return;

    do
    {
        Field* fields = rewards->Fetch();
        uint8 rewardLevel = fields[0].GetUInt8();

        // Проверяем, получил ли игрок эту награду
        if (!HasReceivedLevelReward(player, rewardLevel))
        {
            // Выдаем пропущенную награду
            GiveHardcoreLevelReward(player, rewardLevel);
        }
    }
    while (rewards->NextRow());
}

void OnLevelChanged(Player* player, uint8 oldLevel) override
{
    if (!IsHardcoreEnabled() || !IsHardcorePlayer(player))
        return;

    uint8 newLevel = player->GetLevel();

    // Check if player reached a new level
    if (newLevel > oldLevel)
    {
        // Give reward for level achievement
        GiveHardcoreLevelReward(player, newLevel);

        // Notify player
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff00ff00[HARDCORE] Congratulations on reaching level %u! You received a special reward.|r",
            newLevel);

        // Milestone level announcements
        if (newLevel == 60 || newLevel == 70 || newLevel == 80)
        {
            std::string announcement = "|cffff0000[HARDCORE]|r Player |cff00ff00" + player->GetName() +
                "|r reached level " + std::to_string(newLevel) + " in Hardcore mode! Congratulations!";
            sWorld->SendServerMessage(SERVER_MSG_STRING, announcement.c_str());
        }
    }
}

void GiveHardcoreLevelReward(Player* player, uint8 level)
    {
        if (HasReceivedLevelReward(player, level))
        {
            return;
        }

        // Fetch reward info from database
    QueryResult result = CharacterDatabase.PQuery(
        "SELECT item_id, item_count, money, title_id, achievement_id, spell_id "
        "FROM hardcore_level_rewards WHERE level = {}", level);

    if (!result)
        return;

    Field* fields = result->Fetch();
    uint32 itemId = fields[0].GetUInt32();
    uint32 itemCount = fields[1].GetUInt32();
    uint32 money = fields[2].GetUInt32();
    uint32 titleId = fields[3].GetUInt32();
    uint32 achievementId = fields[4].GetUInt32();
    uint32 spellId = fields[5].GetUInt32();

    // Выдаем предмет, если указан
    if (itemId)
    {
        // Проверяем шаблон предмета
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            TC_LOG_ERROR("sql.sql","Hardcore reward: Item template %u not found!", itemId);
            return;
        }

        // Проверяем место в инвентаре
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, itemCount);
        if (msg != EQUIP_ERR_OK)
        {
            // Создаем предмет
            Item* item = player->StoreNewItem(dest, itemId, true, GenerateItemRandomPropertyId(itemId));
            if (item)
            {
                player->SendNewItem(item, itemCount, true, false);

                // Пытаемся экипировать предмет, если это возможно
                if (itemTemplate->InventoryType != INVTYPE_NON_EQUIP)
                {
                    uint16 dest;
                    InventoryResult msg = player->CanEquipItem(item->GetSlot(), dest, item, false);
                    if (msg == EQUIP_ERR_OK)
                    {
                        player->EquipItem(dest, item, true);
                    }
                }
            }
        }
        else
        {
            // Если инвентарь заполнен, создаем предмет и отправляем по почте
            uint32 guid = player->GetGUID().GetCounter();
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            Item* mailItem = Item::CreateItem(itemId, itemCount, player);
            if (mailItem)
            {
                mailItem->SaveToDB(trans);

                MailSender sender(MAIL_NORMAL, player->GetGUID().GetCounter());
                MailDraft draft("Hardcore Level Reward", "Congratulations on reaching level " + std::to_string(level) + "!");
                draft.AddItem(mailItem);
                draft.SendMailTo(trans, MailReceiver(player, guid), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));

                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cff00ff00[HARDCORE] Your inventory is full. The reward has been sent to you by mail.|r");
            }

            //// Создаем предмет
            //Item* item = player->StoreNewItem(dest, itemId, true, GenerateItemRandomPropertyId(itemId));
            //if (item)
            //{
            //    player->SendNewItem(item, itemCount, true, false);

            //    // Пытаемся экипировать предмет, если это возможно
            //    if (itemTemplate->InventoryType != INVTYPE_NON_EQUIP)
            //    {
            //        uint16 dest;
            //        InventoryResult msg = player->CanEquipItem(item->GetSlot(), dest, item, false);
            //        if (msg == EQUIP_ERR_OK)
            //        {
            //            player->EquipItem(dest, item, true);
            //        }
            //    }
            //}
        }
    }

    // Выдаем деньги
    if (money)
    {
        player->ModifyMoney(money);
    }

    // Выдаем титул
    if (titleId)
    {
        CharTitlesEntry const* titleInfo = sDBCMgr->GetCharTitlesEntry(titleId);
        if (titleInfo)
        {
            player->SetTitle(titleInfo);
        }
    }

    // Выдаем достижение
    if (achievementId)
    {
        AchievementEntry const* achievement = sAchievementStore.LookupEntry(achievementId);
        if (achievement)
        {
            player->CompletedAchievement(achievement);
        }
    }

    // Применяем заклинание (например, телепорт)
    if (spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (spellInfo)
        {
            player->CastSpell(player, spellId, true);
        }
    }

    // Сохраняем информацию о полученной награде
    SaveReceivedLevelReward(player, level);
}

bool HasReceivedLevelReward(Player* player, uint8 level)
{
    QueryResult result = CharacterDatabase.PQuery(
        "SELECT 1 FROM hardcore_rewards_received WHERE player_guid = {} AND level = {}",
        player->GetGUID().GetCounter(), level);

    return result != nullptr;
}

void SaveReceivedLevelReward(Player* player, uint8 level)
{
    CharacterDatabase.PExecute(
        "INSERT INTO hardcore_rewards_received (player_guid, level, received_at) "
        "VALUES ({}, {}, UNIX_TIMESTAMP())",
        player->GetGUID().GetCounter(), level);
}

    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        HandleHardcoreDeath(killed, killer);
    }

    void OnPVPKill(Player* killer, Player* killed) override
    {
        if (IsHardcorePlayer(killed))
        {
            HandleHardcoreDeath(killed, killer);
        }
    }

    void OnCreatureKill(Player* player, Creature* /*killed*/) override
    {
        if (IsHardcorePlayer(player))
        {
            // Дополнительные награды за убийство в hardcore режиме
            //uint32 bonusXP = killed->GetExperience(player) * 0.5f; // +50% опыта
            //player->GiveXP(bonusXP, nullptr);

            // Шанс на дополнительный лут
            if (urand(1, 100) <= 10) // 10% шанс
            {
                ChatHandler(player->GetSession()).SendSysMessage("Hardcore бонус: дополнительный лут!");
                // Добавить логику дополнительного лута
            }
        }
    }

    void OnGiveXP(Player* player, uint32& amount, Unit* /*victim*/) override
    {
        if (IsHardcorePlayer(player))
        {
            // +50% XP bonus for hardcore players
            amount = uint32(amount * 1.5f);
        }
    }

private:
    // static const uint32 HARDCORE_AURA_SPELL = 61573; // Визуальная аура
    static const uint32 HARDCORE_FLAG = 0x10000000;  // Флаг в customFlags

    bool IsHardcoreEnabled()
    {
        return sConfigMgr->GetBoolDefault("Hardcore.Enable", false);
    }

    bool IsHardcorePlayer(Player* player)
    {
        //return player->GetPlayerFlags() & HARDCORE_FLAG;
        return player->HasFlag(PLAYER_FLAGS, HARDCORE_FLAG);
    }

    bool HasExtraLife(Player* player)
    {
        QueryResult result = CharacterDatabase.PQuery(
            "SELECT has_extra_life FROM hardcore_extra_lives WHERE player_guid = {}",
            player->GetGUID().GetCounter()
        );

        return result && (*result)[0].GetBool();
    }

    void UseExtraLife(Player* player)
    {
        CharacterDatabase.PExecute(
            "UPDATE hardcore_extra_lives SET has_extra_life = 0 WHERE player_guid = {}",
            player->GetGUID().GetCounter()
        );
    }

    void HandleHardcoreDeath(Player* player, Unit* killer)
    {
        if (!IsHardcoreEnabled() || !IsHardcorePlayer(player))
            return;

        if (HasExtraLife(player))
        {
            // Resurrect player with 50% health
            player->ResurrectPlayer(0.5f);
            player->SetHealth(player->GetMaxHealth() * 0.5f);

            // Use extra life
            UseExtraLife(player);

            // Notify player
            ChatHandler(player->GetSession()).SendSysMessage(
                "|cffff0000You used an extra life! You are now resurrected, but you won't be able to use this chance again.|r");

            // Log extra life usage
            LogHardcoreDeath(player, killer);
            return;
        }

        LogHardcoreDeath(player, killer);
        std::string announcement = "Hardcore player " + player->GetName() +
            " has died! Character will be deleted.";
        sWorld->SendServerMessage(SERVER_MSG_STRING, announcement.c_str());

        DeleteHardcoreCharacter(player);

        player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST);

        ChatHandler(player->GetSession()).SendSysMessage(
            "|cffff0000Your hardcore character has died! The character will be deleted in 10 seconds.|r");
    }

    void LogHardcoreDeath(Player* player, Unit* killer)
    {
        std::string killerName = killer ? killer->GetName() : "Unknown";
        uint32 level = player->GetLevel();
        uint32 playTime = player->GetTotalPlayedTime();

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_HARDCORE_DEATH);
        stmt->setUInt32(0, player->GetGUID().GetCounter());
        stmt->setString(1, player->GetName());
        stmt->setUInt32(2, level);
        stmt->setUInt32(3, playTime);
        stmt->setString(4, killerName);
        stmt->setUInt64(5, time(nullptr));
        CharacterDatabase.Execute(stmt);
    }

    static void DeleteHardcoreCharacter(Player* player)
    {
        if (WorldSession* session = player->GetSession())
        {
            session->KickPlayer("Player died during a hardcore session.");

            uint32 guid = player->GetGUID().GetCounter();

            // Check if character deletion is enabled in custom config
            if (sGameConfig->GetBoolConfig("config.hardcore.delcharacters"))
            {
                Player::DeleteFromDB(ObjectGuid::Create<HighGuid::Player>(guid), 0, true, true);
            }
        }
    }
};

class hardcore_trade_restrictions : public PlayerScript
{
public:
    hardcore_trade_restrictions() : PlayerScript("hardcore_trade_restrictions") {}

    void OnUpdate(Player* player, uint32 diff) override
    {
        if (!IsHardcoreEnabled() || !IsHardcorePlayer(player))
            return;

        std::lock_guard<std::mutex> lock(sHardcoreTimerMutex);
        sPlayerTradeTimers[player->GetGUID()] += diff;

        if (sPlayerTradeTimers[player->GetGUID()] < 500)
            return;

        sPlayerTradeTimers[player->GetGUID()] = 0;

        if (player->GetTrader())
        {
            CancelTrade(player);
        }
    }

private:
    bool IsHardcoreEnabled()
    {
        return sConfigMgr->GetBoolDefault("Hardcore.Enable", false);
    }

    bool IsHardcorePlayer(Player* player)
    {
        return player->HasFlag(PLAYER_FLAGS, 0x10000000);
    }

    void CancelTrade(Player* player)
    {
        TradeStatusInfo info;

        if (Player* trader = player->GetTrader())
        {
            info.Status = TRADE_STATUS_TRADE_CANCELED;
            // Отменяем торговлю для обеих сторон
            player->GetSession()->SendTradeStatus(info);
            trader->GetSession()->SendTradeStatus(info);

            // Очищаем торговлю
            player->TradeCancel(false);

            // Notify players
            ChatHandler(player->GetSession()).SendSysMessage(
                "|cffff0000[HARDCORE] Trade canceled! Hardcore players cannot trade.|r");
            ChatHandler(trader->GetSession()).SendSysMessage(
                "|cffff0000Trade canceled! Your partner is playing in hardcore mode.|r");
        }
    }
};

class hardcore_group_restrictions : public GroupScript
{
public:
    hardcore_group_restrictions() : GroupScript("hardcore_group_restrictions") {}

    void OnAddMember(Group* group, ObjectGuid guid) override
    {
        if (!sConfigMgr->GetBoolDefault("Hardcore.Enable", false))
            return;

        Player* added = ObjectAccessor::FindPlayer(guid);
        if (!added)
            return;

        bool addedIsHardcore = IsHardcorePlayer(added);

        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || member == added)
                continue;

            bool memberIsHardcore = IsHardcorePlayer(member);

            if (addedIsHardcore != memberIsHardcore)
            {
                group->RemoveMember(added->GetGUID());
                ChatHandler(added->GetSession()).SendSysMessage(
                    "|cffff0000[HARDCORE] You cannot be in a group with " +
                    std::string(addedIsHardcore ? "regular" : "Hardcore") + " players!|r");

                if (Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID()))
                {
                    ChatHandler(leader->GetSession()).PSendSysMessage(
                        "|cffff0000[HARDCORE] Player %s cannot be added to the group due to Hardcore restrictions.|r",
                        added->GetName().c_str());
                }
                return;
            }
        }
    }

private:
    bool IsHardcorePlayer(Player* player)
    {
        return player->HasFlag(PLAYER_FLAGS, 0x10000000);
    }
};

class hardcore_activator_ai : public GameObjectScript
{
public:
    hardcore_activator_ai() : GameObjectScript("hardcore_activator_ai") {}

    struct hardcore_activator_aiAI : public GameObjectAI
    {
        hardcore_activator_aiAI(GameObject* go) : GameObjectAI(go) {}


        bool OnGossipHello(Player* player) override
        {
            if (!player || player->HasFlag(PLAYER_FLAGS, 0x10000000))
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Disable Hardcore Mode", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
            }
            else
            {
            if (player->GetLevel() <= 1)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Enable Hardcore Mode", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
                return true;
            }
        }
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menu_id*/, uint32 gossipListId) override
        {
            uint32 sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
            uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            return GossipSelect(player, sender, action);
        }

        bool GossipSelect(Player* player, uint32 /*sender*/, uint32 action)
        {
            player->PlayerTalkClass->ClearMenus();

            switch (action)
            {
            case GOSSIP_ACTION_INFO_DEF + 1: // off Hardcore
                player->RemoveFlag(PLAYER_FLAGS, 0x10000000);
                player->SaveToDB();
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
                //me->Whisper("Hardcore Mode has been disabled.", LANG_UNIVERSAL, player);
                ChatHandler(player->GetSession()).SendSysMessage("Hardcore Mode has been disabled.");
                CloseGossipMenuFor(player);
                break;

            case GOSSIP_ACTION_INFO_DEF + 2: // on Hardcore
                player->SetFlag(PLAYER_FLAGS, 0x10000000);
                std::ostringstream ss;
                ss << "UPDATE characters SET extra_flags = extra_flags | 1 WHERE guid = " << player->GetGUID().GetCounter();
                CharacterDatabase.Execute(ss.str().c_str());
                player->SaveToDB();
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
                // me->Whisper("Hardcore Mode has been enabled. Be careful!", LANG_UNIVERSAL, player);
                ChatHandler(player->GetSession()).SendSysMessage("Hardcore Mode has been enabled. Be careful!");
                CloseGossipMenuFor(player);
                break;
            }
            return true;
        }

    };

    GameObjectAI* GetAI(GameObject* go) const override
    {
        return new hardcore_activator_aiAI(go);
    }
};

class npc_hardcore_time_rewards : public CreatureScript
{
public:
    npc_hardcore_time_rewards() : CreatureScript("npc_hardcore_time_rewards") { }

    struct npc_hardcore_time_rewardsAI : public ScriptedAI
    {
        npc_hardcore_time_rewardsAI(Creature* me) : ScriptedAI(me) { }

    bool OnGossipHello(Player* player) override
    {
        if (!IsHardcorePlayer(player))
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                "Sorry, these rewards are for hardcore players only!",
                GOSSIP_SENDER_MAIN, 1);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
            return true;
        }

        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, "Show available rewards", GOSSIP_SENDER_MAIN, 1);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "My statistics", GOSSIP_SENDER_MAIN, 2);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Claim rewards", GOSSIP_SENDER_MAIN, 3);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Reward history", GOSSIP_SENDER_MAIN, 4);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menu_id*/, uint32 gossipListId) override
    {
        uint32 sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
        uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        return GossipSelect(player, sender, action);
    }

    bool GossipSelect(Player* player, uint32 /*sender*/, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();

        switch (action)
        {
            case 1:
                ShowAvailableRewards(player, me);
                break;
            case 2:
                ShowPlayerStats(player, me);
                break;
            case 3:
                ClaimRewards(player);
                break;
            case 4:
                ShowRewardHistory(player, me);
                break;
            default:
                OnGossipHello(player);
                break;
        }
        return true;
    }

private:
    bool IsHardcorePlayer(Player* player)
    {
        return player->HasFlag(PLAYER_FLAGS, 0x10000000);
    }

   uint32 GetPlayerTotalTime(Player* player)
{
    // Если нет записи в hardcore_playtime, возвращаем базовое время
    return player->GetTotalPlayedTime();
}

    void ShowAvailableRewards(Player* player, Creature* me)
    {
        uint32 totalTime = GetPlayerTotalTime(player);

        QueryResult rewards = CharacterDatabase.Query(
            "SELECT id, required_time, name, description FROM hardcore_time_rewards "
            "WHERE enabled = 1 ORDER BY required_time");

        if (!rewards)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "No rewards found", GOSSIP_SENDER_MAIN, 0);
        }
        else
        {
            do
            {
                Field* fields = rewards->Fetch();
                uint32 rewardId = fields[0].GetUInt32();
                uint32 requiredTime = fields[1].GetUInt32();
                std::string name = fields[2].GetString();
                [[maybe_unused]] std::string description = fields[3].GetString();

                uint32 hours = requiredTime / 3600;
                uint32 minutes = (requiredTime % 3600) / 60;

                std::string status = totalTime >= requiredTime ? "[Available]" : "[Locked]";
                bool claimed = HasClaimedReward(player, rewardId);
                if (claimed) status = "[Claimed]";

                std::string gossipText = status + " " + name + " (" +
                    std::to_string(hours) + "h " + std::to_string(minutes) + "m)";

                AddGossipItemFor(player, GOSSIP_ICON_CHAT, gossipText, GOSSIP_SENDER_MAIN, 100 + rewardId);
            }
            while (rewards->NextRow());
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, 0);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
    }

    void ShowPlayerStats(Player* player, Creature* me)
    {
        uint32 totalTime = GetPlayerTotalTime(player);
        uint32 hours = totalTime / 3600;
        uint32 minutes = (totalTime % 3600) / 60;
        uint32 seconds = totalTime % 60;

        std::string timeStr = "Survival time: " + std::to_string(hours) + "h " +
                             std::to_string(minutes) + "m " + std::to_string(seconds) + "s";

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, timeStr, GOSSIP_SENDER_MAIN, 1);

        // Show claimed survival time rewards
        QueryResult claimedCount = CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM hardcore_rewards_claimed WHERE guid = {}",
            player->GetGUID().GetCounter());

        if (claimedCount)
        {
            uint32 count = (*claimedCount)[0].GetUInt32();
            std::string rewardsStr = "Time rewards claimed: " + std::to_string(count);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, rewardsStr, GOSSIP_SENDER_MAIN, 1);
        }

        // Show claimed level rewards
        QueryResult levelRewardsCount = CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM hardcore_rewards_received WHERE player_guid = {}",
            player->GetGUID().GetCounter());

        if (levelRewardsCount)
        {
            uint32 count = (*levelRewardsCount)[0].GetUInt32();
            std::string levelRewardsStr = "Level rewards claimed: " + std::to_string(count);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, levelRewardsStr, GOSSIP_SENDER_MAIN, 1);
        }

        // Extra life status
        bool hasExtraLife = false;
        QueryResult extraLifeResult = CharacterDatabase.PQuery(
            "SELECT has_extra_life FROM hardcore_extra_lives WHERE player_guid = {}",
            player->GetGUID().GetCounter()
        );
        if (extraLifeResult && (*extraLifeResult)[0].GetBool())
            hasExtraLife = true;

        std::string extraLifeStr = "Extra life: " + std::string(hasExtraLife ? "|cff00ff00Yes|r" : "|cffff0000No|r");
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, extraLifeStr, GOSSIP_SENDER_MAIN, 1);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, 0);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
    }

    void ClaimRewards(Player* player)
    {
        uint32 totalTime = GetPlayerTotalTime(player);
        uint32 rewardsClaimed = 0;

        QueryResult rewards = CharacterDatabase.Query(
            "SELECT id, required_time, item_id, item_count, money, title_id, achievement_id, spell_id, name, experience "
            "FROM hardcore_time_rewards WHERE enabled = 1 ORDER BY required_time");

        if (!rewards)
        {
            ChatHandler(player->GetSession()).SendSysMessage("No rewards found!");
            OnGossipHello(player);
            return;
        }

        do
        {
            Field* fields = rewards->Fetch();
            uint32 rewardId = fields[0].GetUInt32();
            uint32 requiredTime = fields[1].GetUInt32();

            if (totalTime >= requiredTime && !HasClaimedReward(player, rewardId))
            {
                GiveTimeReward(player, rewardId, fields);
                rewardsClaimed++;
            }
        }
        while (rewards->NextRow());

        if (rewardsClaimed > 0)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "You received %u rewards for survival time!", rewardsClaimed);
        }
        else
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "No rewards available to claim.");
        }

        OnGossipHello(player);
    }

    void ShowRewardHistory(Player* player, Creature* me)
    {
        QueryResult history = CharacterDatabase.PQuery(
            "SELECT r.name, c.claimed_time, c.playtime_when_claimed "
            "FROM hardcore_rewards_claimed c "
            "JOIN hardcore_time_rewards r ON c.reward_id = r.id "
            "WHERE c.guid = {} ORDER BY c.claimed_time DESC LIMIT 10",
            player->GetGUID().GetCounter());

        if (!history)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Reward history is empty", GOSSIP_SENDER_MAIN, 1);
        }
        else
        {
            do
            {
                Field* fields = history->Fetch();
                std::string name = fields[0].GetString();
                [[maybe_unused]] uint32 claimed_time = fields[1].GetUInt32();
                uint32 playtimeWhen = fields[2].GetUInt32();

                uint32 hours = playtimeWhen / 3600;
                uint32 minutes = (playtimeWhen % 3600) / 60;

                std::string historyText = name + " (at " + std::to_string(hours) +
                                        "h " + std::to_string(minutes) + "m)";

                AddGossipItemFor(player, GOSSIP_ICON_CHAT, historyText, GOSSIP_SENDER_MAIN, 1);
            }
            while (history->NextRow());
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, 0);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
    }

    bool HasClaimedReward(Player* player, uint32 rewardId)
    {
        QueryResult result = CharacterDatabase.PQuery(
            "SELECT 1 FROM hardcore_rewards_claimed WHERE guid = {} AND reward_id = {}",
            player->GetGUID().GetCounter(), rewardId);

        return result != nullptr;
    }

    void GiveTimeReward(Player* player, uint32 rewardId, Field* fields)
    {
        uint32 itemId = fields[2].GetUInt32();
        uint32 itemCount = fields[3].GetUInt32();
        uint32 money = fields[4].GetUInt32();
        uint32 titleId = fields[5].GetUInt32();
        uint32 achievementId = fields[6].GetUInt32();
        uint32 spellId = fields[7].GetUInt32();
        std::string name = fields[8].GetString();
		uint32 experience = fields[9].GetUInt32();

        // Выдача предмета
        if (itemId)
        {
            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, itemCount);

            if (msg == EQUIP_ERR_OK)
            {
                Item* item = player->StoreNewItem(dest, itemId, true);
                if (item)
                {
                    player->SendNewItem(item, itemCount, true, false);
                }
            }
        }

        // Выдача денег
        if (money)
        {
            player->ModifyMoney(money);
        }

        // Выдача титула
        if (titleId)
        {
            CharTitlesEntry const* titleInfo = sDBCMgr->GetCharTitlesEntry(titleId);
            if (titleInfo)
            {
                player->SetTitle(titleInfo);
            }
        }

        // Выдача достижения
        if (achievementId)
        {
            AchievementEntry const* achievement = sAchievementStore.LookupEntry(achievementId);
            if (achievement)
            {
                player->CompletedAchievement(achievement);
            }
        }

        // Применение заклинания
        if (spellId)
        {
            player->CastSpell(player, spellId, true);
        }

        if (experience)
        {
            player->GiveXP(experience, nullptr, false);
            me->Whisper("|cff00ff00[HARDCORE]|r You received %u XP!", LANG_UNIVERSAL, player, experience);
        }
			
        // Сохранение информации о полученной награде
        CharacterDatabase.PExecute(
            "INSERT INTO hardcore_rewards_claimed (guid, reward_id, claimed_time, playtime_when_claimed) "
            "VALUES ({}, {}, {}, {})",
            player->GetGUID().GetCounter(), rewardId, GameTime::GetGameTime(), GetPlayerTotalTime(player));
    }
    };

    CreatureAI* GetAI(Creature* me) const override
    {
        return new npc_hardcore_time_rewardsAI(me);
    }
};



void AddSC_hardcore_mode()
{
    new HardcoreModePlayerScript();
    new hardcore_trade_restrictions();
    new hardcore_group_restrictions();
    new hardcore_activator_ai();
    new npc_hardcore_time_rewards();
}

/*
#
#    Hardcore.Enable
#        Description: Enable hardcore mode functionality
#        Default:     0 (disabled)
#                     1 (enabled)

Hardcore.Enable = 1
*/
