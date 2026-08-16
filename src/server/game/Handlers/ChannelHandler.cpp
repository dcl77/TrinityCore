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

#include "WorldSession.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "ChannelPackets.h"
#include "DBCStores.h"
#include "Log.h"
#include "ObjectMgr.h"                                      // for normalizePlayerName
#include "Player.h"
#include <cctype>

static size_t const MAX_CHANNEL_PASS_STR = 31;
static size_t const MAX_CHANNEL_NAME_STR = 31;

void WorldSession::HandleJoinChannel(WorldPackets::Channel::JoinChannel& packet)
{
    TC_LOG_DEBUG("chat.system", "CMSG_JOIN_CHANNEL {} ChatChannelId: {}, CreateVoiceSession: {}, Internal: {}, ChannelName: {}, Password: {}",
        GetPlayerInfo(), packet.ChatChannelId, packet.CreateVoiceSession, packet.Internal, packet.ChannelName, packet.Password);

    AreaTableEntry const* zone = sAreaTableStore.LookupEntry(GetPlayer()->GetZoneId());
    if (packet.ChatChannelId)
    {
        ChatChannelsEntry const* channel = sChatChannelsStore.LookupEntry(packet.ChatChannelId);
        if (!channel)
            return;

        if (!zone || !GetPlayer()->CanJoinConstantChannelInZone(channel, zone))
            return;
    }

    ChannelMgr* cMgr = ChannelMgr::ForTeam(GetPlayer()->GetTeam());
    if (!cMgr)
        return;

    if (packet.ChatChannelId)
    { // system channel
        if (Channel* channel = cMgr->GetSystemChannel(packet.ChatChannelId, zone))
            channel->JoinChannel(GetPlayer());
    }
    else
    { // custom channel
        if (packet.ChannelName.empty() || isdigit((unsigned char)packet.ChannelName[0]))
        {
            WorldPackets::Channel::ChannelNotify channelNotify;
            channelNotify.Type = CHAT_INVALID_NAME_NOTICE;
            channelNotify._Channel = packet.ChannelName;
            SendPacket(channelNotify.Write());
            return;
        }

        if (utf8length(packet.ChannelName) > MAX_CHANNEL_NAME_STR)
        {
            WorldPackets::Channel::ChannelNotify channelNotify;
            channelNotify.Type = CHAT_INVALID_NAME_NOTICE;
            channelNotify._Channel = packet.ChannelName;
            SendPacket(channelNotify.Write());
            TC_LOG_ERROR("network", "Player {} tried to create a channel with a name more than {} characters long - blocked", GetPlayer()->GetGUID().ToString(), MAX_CHANNEL_NAME_STR);
            return;
        }

        if (packet.Password.length() > MAX_CHANNEL_PASS_STR)
        {
            TC_LOG_ERROR("network", "Player {} tried to create a channel with a password more than {} characters long - blocked", GetPlayer()->GetGUID().ToString(), MAX_CHANNEL_PASS_STR);
            return;
        }

        if (!DisallowHyperlinksAndMaybeKick(packet.ChannelName))
            return;

        if (Channel* channel = cMgr->GetCustomChannel(packet.ChannelName))
            channel->JoinChannel(GetPlayer(), packet.Password);
        else if (Channel* channel = cMgr->CreateCustomChannel(packet.ChannelName))
        {
            channel->SetPassword(packet.Password);
            channel->JoinChannel(GetPlayer(), packet.Password);
        }
    }
}

void WorldSession::HandleLeaveChannel(WorldPackets::Channel::LeaveChannel& packet)
{
    TC_LOG_DEBUG("chat.system", "CMSG_LEAVE_CHANNEL {} ChannelName: {}, ZoneChannelID: {}",
        GetPlayerInfo(), packet.ChannelName, packet.ZoneChannelID);

    if (packet.ChannelName.empty() && !packet.ZoneChannelID)
        return;

    AreaTableEntry const* zone = sAreaTableStore.LookupEntry(GetPlayer()->GetZoneId());
    if (packet.ZoneChannelID)
    {
        ChatChannelsEntry const* channel = sChatChannelsStore.LookupEntry(packet.ZoneChannelID);
        if (!channel)
            return;

        if (!zone || !GetPlayer()->CanJoinConstantChannelInZone(channel, zone))
            return;
    }

    if (ChannelMgr* cMgr = ChannelMgr::ForTeam(GetPlayer()->GetTeam()))
    {
        if (Channel* channel = cMgr->GetChannel(packet.ZoneChannelID, packet.ChannelName, GetPlayer(), true, zone))
            channel->LeaveChannel(GetPlayer(), true);

        if (packet.ZoneChannelID)
            cMgr->LeftChannel(packet.ZoneChannelID, zone);
    }
}

<<<<<<< HEAD
void WorldSession::HandleChannelList(WorldPackets::Channel::ChannelListRequest& packet)
=======
void WorldSession::HandleChannelCommand(WorldPackets::Channel::ChannelCommand& packet)
>>>>>>> upstream/3.3.5
{
    TC_LOG_DEBUG("chat.system", "{} {} ChannelName: {}",
        GetOpcodeNameForLogging(packet.GetOpcode()), GetPlayerInfo(), packet.ChannelName);

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(packet.ChannelName, GetPlayer()))
<<<<<<< HEAD
        channel->List(GetPlayer());
}

void WorldSession::HandleChannelPassword(WorldPacket& recvPacket)
{
    std::string channelName, password;
    recvPacket >> channelName >> password;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_PASSWORD {} Channel: {}, Password: {}",
        GetPlayerInfo(), channelName, password);

    if (password.length() > MAX_CHANNEL_PASS_STR)
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->Password(GetPlayer(), password);
}

void WorldSession::HandleChannelSetOwner(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_SET_OWNER {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->SetOwner(GetPlayer(), targetName);
}

void WorldSession::HandleChannelOwner(WorldPacket& recvPacket)
{
    std::string channelName;
    recvPacket >> channelName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_OWNER {} Channel: {}",
        GetPlayerInfo(), channelName);

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->SendWhoOwner(GetPlayer());
}

void WorldSession::HandleChannelModerator(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_MODERATOR {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->SetModerator(GetPlayer(), targetName);
}

void WorldSession::HandleChannelUnmoderator(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_UNMODERATOR {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->UnsetModerator(GetPlayer(), targetName);
}

void WorldSession::HandleChannelMute(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_MUTE {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->SetMute(GetPlayer(), targetName);
}

void WorldSession::HandleChannelUnmute(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_UNMUTE {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->UnsetMute(GetPlayer(), targetName);
}

void WorldSession::HandleChannelInvite(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_INVITE {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->Invite(GetPlayer(), targetName);
}

void WorldSession::HandleChannelKick(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_KICK {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->Kick(GetPlayer(), targetName);
}

void WorldSession::HandleChannelBan(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_BAN {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->Ban(GetPlayer(), targetName);
}

void WorldSession::HandleChannelUnban(WorldPacket& recvPacket)
{
    std::string channelName, targetName;
    recvPacket >> channelName >> targetName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_UNBAN {} Channel: {}, Target: {}",
        GetPlayerInfo(), channelName, targetName);

    if (!normalizePlayerName(targetName))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->UnBan(GetPlayer(), targetName);
}

void WorldSession::HandleChannelAnnouncements(WorldPacket& recvPacket)
{
    std::string channelName;
    recvPacket >> channelName;

    TC_LOG_DEBUG("chat.system", "CMSG_CHANNEL_ANNOUNCEMENTS {} Channel: {}",
        GetPlayerInfo(), channelName);

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
        channel->Announce(GetPlayer());
}

void WorldSession::HandleGetChannelMemberCount(WorldPacket &recvPacket)
{
    std::string channelName;
    recvPacket >> channelName;

    TC_LOG_DEBUG("chat.system", "CMSG_GET_CHANNEL_MEMBER_COUNT {} Channel: {}",
        GetPlayerInfo(), channelName);

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(channelName, GetPlayer()))
=======
>>>>>>> upstream/3.3.5
    {
        switch (packet.GetOpcode())
        {
            case CMSG_CHANNEL_ANNOUNCEMENTS:
                channel->Announce(GetPlayer());
                break;
            case CMSG_DECLINE_CHANNEL_INVITE:
                channel->DeclineInvite(GetPlayer());
                break;
            case CMSG_CHANNEL_DISPLAY_LIST:
            case CMSG_CHANNEL_LIST:
                channel->List(GetPlayer());
                break;
            case CMSG_CHANNEL_OWNER:
                channel->SendWhoOwner(GetPlayer());
                break;
            case CMSG_GET_CHANNEL_MEMBER_COUNT:
                channel->SendMemberCount(GetPlayer());
                break;
            case CMSG_CHANNEL_VOICE_ON:
                channel->VoiceOn(GetPlayer());
                break;
            case CMSG_CHANNEL_VOICE_OFF:
                channel->VoiceOff(GetPlayer());
                break;
            case CMSG_SET_CHANNEL_WATCH:
                // channel->JoinNotify(GetPlayer());
                break;
            default:
                break;
        }
    }
}

void WorldSession::HandleChannelPlayerCommand(WorldPackets::Channel::ChannelPlayerCommand& packet)
{
    if (packet.Name.length() >= MAX_CHANNEL_NAME_STR)
    {
        TC_LOG_DEBUG("chat.system", "{} {} ChannelName: {}, Name: {}, Name too long.",
            GetOpcodeNameForLogging(packet.GetOpcode()), GetPlayerInfo(), packet.ChannelName, packet.Name);
        return;
    }

    TC_LOG_DEBUG("chat.system", "{} {} ChannelName: {}, Name: {}",
        GetOpcodeNameForLogging(packet.GetOpcode()), GetPlayerInfo(), packet.ChannelName, packet.Name);

    if (!normalizePlayerName(packet.Name))
        return;

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(packet.ChannelName, GetPlayer()))
    {
        switch (packet.GetOpcode())
        {
            case CMSG_CHANNEL_BAN:
                channel->Ban(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_INVITE:
                channel->Invite(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_KICK:
                channel->Kick(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_MODERATOR:
                channel->SetModerator(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_MUTE:
                channel->SetMute(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_SET_OWNER:
                channel->SetOwner(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_SILENCE_ALL:
                channel->SilenceAll(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_SILENCE_VOICE:
                channel->SilenceVoice(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_UNBAN:
                channel->UnBan(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_UNMODERATOR:
                channel->UnsetModerator(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_UNMUTE:
                channel->UnsetMute(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_UNSILENCE_ALL:
                channel->UnsilenceAll(GetPlayer(), packet.Name);
                break;
            case CMSG_CHANNEL_UNSILENCE_VOICE:
                channel->UnsilenceVoice(GetPlayer(), packet.Name);
                break;
            default:
                break;
        }
    }
}

void WorldSession::HandleChannelPassword(WorldPackets::Channel::ChannelPassword& packet)
{
    if (packet.Password.length() > MAX_CHANNEL_PASS_STR)
    {
        TC_LOG_DEBUG("chat.system", "{} {} ChannelName: {}, Password: {}, Password too long.",
            GetOpcodeNameForLogging(packet.GetOpcode()), GetPlayerInfo(), packet.ChannelName, packet.Password);
        return;
    }

    TC_LOG_DEBUG("chat.system", "{} {} ChannelName: {}, Password: {}",
        GetOpcodeNameForLogging(packet.GetOpcode()), GetPlayerInfo(), packet.ChannelName, packet.Password);

    if (Channel* channel = ChannelMgr::GetChannelForPlayerByNamePart(packet.ChannelName, GetPlayer()))
        channel->Password(GetPlayer(), packet.Password);
}
