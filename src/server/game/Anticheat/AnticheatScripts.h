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

<<<<<<<< HEAD:src/server/game/Anticheat/AnticheatScripts.h
#ifndef SC_ACSCRIPTS_H
#define SC_ACSCRIPTS_H

#include "ScriptMgr.h"

class AnticheatScripts: public PlayerScript
{
    public:
        AnticheatScripts();

        void OnLogout(Player* player);
        void OnLogin(Player* player,bool);
        void OnUpdate(Player* player, uint32 diff);
};

#endif
========
#ifndef TRINITYCORE_ASYNC_CALLBACK_PROCESSOR_FWD_H
#define TRINITYCORE_ASYNC_CALLBACK_PROCESSOR_FWD_H

#include <concepts>

template <typename T>
concept AsyncCallback = requires(T& t) { { InvokeAsyncCallbackIfReady(t) } -> std::convertible_to<bool>; };

template<AsyncCallback T>
class AsyncCallbackProcessor;

#endif // TRINITYCORE_ASYNC_CALLBACK_PROCESSOR_FWD_H
>>>>>>>> upstream/3.3.5:src/common/Utilities/AsyncCallbackProcessorFwd.h
