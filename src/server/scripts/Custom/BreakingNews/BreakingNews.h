#ifndef MODULE_BREAKINGNEWS_H
#define MODULE_BREAKINGNEWS_H

#include "Config.h"
#include "ScriptMgr.h"
#include "Warden.h"

#include <algorithm>
#include <fstream>
#include <iostream>

constexpr char const* _prePayload = "wlbuf = '';";
constexpr char const* _postPayload = "local a,b=loadstring(wlbuf)if not a then message(b)else a()end";
constexpr char const* _midPayloadFmt = "local a=ServerAlertFrame;local b=ServerAlertText;local c=ServerAlertTitle;local d=CharacterSelect;if a~=nil or b~=nil or c~=nil or d~=nil then a:SetParent(d)ServerAlertTitle:SetText('{}')ServerAlertText:SetText('{}')a:Show()else message('ServerAlert(Frame|Text|Title)/CharacterSelectFrame is nil.')end";

constexpr uint16 _prePayloadId = 9500;
constexpr uint16 _postPayloadId = 9501;
constexpr uint16 _tmpPayloadId = 9502;

class BreakingNewsServerScript : public ServerScript
{
public:
    BreakingNewsServerScript() : ServerScript("BreakingNewsServerScript") { }

private:
    void OnPacketSend(WorldSession* session, WorldPacket& packet) override;
    std::vector<std::string> GetChunks(std::string s, uint8_t chunkSize);
    void SendChunkedPayload(Warden* warden, WardenPayloadMgr* payloadMgr, std::string payload, uint32 chunkSize);
};

class BreakingNewsWorldScript : public WorldScript
{
public:
    BreakingNewsWorldScript() : WorldScript("BreakingNewsWorldScript") { }

private:
    void OnConfigLoad(bool reload) override;
};

#endif //MODULE_BREAKINGNEWS_H
