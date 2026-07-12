/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "BreakingNews.h"
#include "StringFormat.h"
#include "WardenPayloadMgr.h"
#include "Log.h"
#include <mutex>

namespace
{
    std::mutex bn_Lock;
    bool bn_Enabled = false;
    std::string bn_Title = "";
    std::string bn_Body = "";
    std::string bn_Formatted = "";
}

bool TryReadFile(std::string const& path, std::string& bn_Result)
{
    std::ifstream bn_File(path);

    std::string bn_Buffer = "";
    bn_Result = "";

    if (!bn_File.is_open())
        return false;

    while (std::getline(bn_File, bn_Buffer))
        bn_Result = bn_Result + (bn_Buffer);

    bn_Result.erase(std::remove(bn_Result.begin(), bn_Result.end(), '\r'), bn_Result.cend());
    bn_Result.erase(std::remove(bn_Result.begin(), bn_Result.end(), '\n'), bn_Result.cend());

    return true;
}

bool TryReadNews(std::string& bn_Result)
{
    std::string path = sConfigMgr->GetStringDefault("BreakingNews.HtmlPath", "./breakingnews.html");
    bn_Title = sConfigMgr->GetStringDefault("BreakingNews.Title", "Breaking News");

    if (path == "")
    {
        TC_LOG_ERROR("server", "Failed to read 'BreakingNews.HtmlPath'.");
        return false;
    }

    if (!TryReadFile(path, bn_Result))
    {
        TC_LOG_ERROR("server", "Failed to read file '{}'.", path);
        return false;
    }

    return true;
}

void LoadBreakingNewsLocked()
{
    bn_Title = sConfigMgr->GetStringDefault("BreakingNews.Title", "Breaking News");

    if (!TryReadNews(bn_Body))
    {
        TC_LOG_ERROR("server", "Failed to read breaking news.");
        return;
    }

    bn_Formatted = Trinity::StringFormat(_midPayloadFmt, bn_Title, bn_Body);
}

std::vector<std::string> BreakingNewsServerScript::GetChunks(std::string s, uint8_t chunkSize)
{
    std::vector<std::string> chunks;

    for (uint32_t i = 0; i < s.size(); i += chunkSize)
        chunks.push_back(s.substr(i, chunkSize));

    return chunks;
}

void BreakingNewsServerScript::SendChunkedPayload(Warden* warden, WardenPayloadMgr* payloadMgr, std::string payload, uint32 chunkSize)
{
    bool verbose = sConfigMgr->GetBoolDefault("BreakingNews.Verbose", false);

    auto chunks = GetChunks(payload, chunkSize);

    if (!payloadMgr->GetPayloadById(_prePayloadId))
        payloadMgr->RegisterPayload(_prePayload, _prePayloadId);

    payloadMgr->QueuePayload(_prePayloadId);
    warden->ForceChecks();

    if (verbose)
        TC_LOG_INFO("server", "Sent pre-payload '{}'.", _prePayload);

    for (auto const& chunk : chunks)
    {
        auto smallPayload = "wlbuf = wlbuf .. [[" + chunk + "]];";

        payloadMgr->RegisterPayload(smallPayload, _tmpPayloadId, true);
        payloadMgr->QueuePayload(_tmpPayloadId);
        warden->ForceChecks();

        if (verbose)
            TC_LOG_INFO("server", "Sent mid-payload '{}'.", smallPayload);
    }

    if (!payloadMgr->GetPayloadById(_postPayloadId))
        payloadMgr->RegisterPayload(_postPayload, _postPayloadId);

    payloadMgr->QueuePayload(_postPayloadId);
    warden->ForceChecks();

    if (verbose)
        TC_LOG_INFO("server", "Sent post-payload '{}'.", _postPayload);
}

void BreakingNewsServerScript::OnPacketSend(WorldSession* session, WorldPacket& packet)
{
    if (packet.GetOpcode() != SMSG_CHAR_ENUM)
        return;

    bool enabled;
    std::string formattedPayload;
    bool cacheEnabled;

    {
        std::lock_guard<std::mutex> guard(bn_Lock);
        enabled = bn_Enabled;
        formattedPayload = bn_Formatted;
        cacheEnabled = sConfigMgr->GetBoolDefault("BreakingNews.Cache", true);
    }

    if (!enabled)
        return;

    Warden* warden = session->GetWarden();
    if (!warden)
        return;

    // Trying to use Warden before it has initialized,
    // so we exit.
    if (!warden->IsInitialized())
        return;

    // Load in the updated news if cache is disabled.
    if (!cacheEnabled)
    {
        std::lock_guard<std::mutex> guard(bn_Lock);
        LoadBreakingNewsLocked();
        formattedPayload = bn_Formatted;
    }

    if (formattedPayload.empty())
        return;

    auto payloadMgr = warden->GetPayloadMgr();
    if (!payloadMgr)
        return;

    // Just in-case there are some payloads in the queue, we don't want to send the incorrect payload.
    payloadMgr->ClearQueuedPayloads();

    // The client truncates warden packets to around 256 and our payload may be larger than that.
    SendChunkedPayload(warden, payloadMgr, formattedPayload, 128);
}

void BreakingNewsWorldScript::OnConfigLoad(bool /*reload*/)
{
    std::lock_guard<std::mutex> guard(bn_Lock);
    bn_Enabled = sConfigMgr->GetBoolDefault("BreakingNews.Enable", false);

    if (!bn_Enabled)
        return;

    LoadBreakingNewsLocked();
}

void AddBreakingNewsScripts()
{
    new BreakingNewsWorldScript();
    new BreakingNewsServerScript();
}
