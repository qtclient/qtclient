/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "client/const.h"
#include "protocol/protocolcodes.h"
#include "framework/stdext/storage.h"

#include <algorithm>

namespace Proto
{
    stdext::map<uint8_t, uint8_t > messageModesMap;

    void buildMessageModesMap(const int version)
    {
        messageModesMap.clear();

        if (version >= 1094) {
            messageModesMap[tibia::client::MessageMana] = 43;
        }

        if (version >= 1055) { // might be 1054
            messageModesMap[tibia::client::MessageNone] = 0;
            messageModesMap[tibia::client::MessageSay] = 1;
            messageModesMap[tibia::client::MessageWhisper] = 2;
            messageModesMap[tibia::client::MessageYell] = 3;
            messageModesMap[tibia::client::MessagePrivateFrom] = 4;
            messageModesMap[tibia::client::MessagePrivateTo] = 5;
            messageModesMap[tibia::client::MessageChannelManagement] = 6;
            messageModesMap[tibia::client::MessageChannel] = 7;
            messageModesMap[tibia::client::MessageChannelHighlight] = 8;
            messageModesMap[tibia::client::MessageSpell] = 9;
            messageModesMap[tibia::client::MessageNpcFromStartBlock] = 10;
            messageModesMap[tibia::client::MessageNpcFrom] = 11;
            messageModesMap[tibia::client::MessageNpcTo] = 12;
            messageModesMap[tibia::client::MessageGamemasterBroadcast] = 13;
            messageModesMap[tibia::client::MessageGamemasterChannel] = 14;
            messageModesMap[tibia::client::MessageGamemasterPrivateFrom] = 15;
            messageModesMap[tibia::client::MessageGamemasterPrivateTo] = 16;
            messageModesMap[tibia::client::MessageLogin] = 17;
            messageModesMap[tibia::client::MessageWarning] = 18; // Admin
            messageModesMap[tibia::client::MessageGame] = 19;
            messageModesMap[tibia::client::MessageGameHighlight] = 20;
            messageModesMap[tibia::client::MessageFailure] = 21;
            messageModesMap[tibia::client::MessageLook] = 22;
            messageModesMap[tibia::client::MessageDamageDealed] = 23;
            messageModesMap[tibia::client::MessageDamageReceived] = 24;
            messageModesMap[tibia::client::MessageHeal] = 25;
            messageModesMap[tibia::client::MessageExp] = 26;
            messageModesMap[tibia::client::MessageDamageOthers] = 27;
            messageModesMap[tibia::client::MessageHealOthers] = 28;
            messageModesMap[tibia::client::MessageExpOthers] = 29;
            messageModesMap[tibia::client::MessageStatus] = 30;
            messageModesMap[tibia::client::MessageLoot] = 31;
            messageModesMap[tibia::client::MessageTradeNpc] = 32;
            messageModesMap[tibia::client::MessageGuild] = 33;
            messageModesMap[tibia::client::MessagePartyManagement] = 34;
            messageModesMap[tibia::client::MessageParty] = 35;
            messageModesMap[tibia::client::MessageBarkLow] = 36;
            messageModesMap[tibia::client::MessageBarkLoud] = 37;
            messageModesMap[tibia::client::MessageReport] = 38;
            messageModesMap[tibia::client::MessageHotkeyUse] = 39;
            messageModesMap[tibia::client::MessageTutorialHint] = 40;
            messageModesMap[tibia::client::MessageThankyou] = 41;
            messageModesMap[tibia::client::MessageMarket] = 42;
            messageModesMap[tibia::client::MessageMana] = 43;
            messageModesMap[tibia::client::MessageBeyondLast] = 44;
            messageModesMap[tibia::client::MessageAttention] = 48;
            messageModesMap[tibia::client::MessageBoostedCreature] = 49;
            messageModesMap[tibia::client::MessageOfflineTrainning] = 50;
            messageModesMap[tibia::client::MessageTransaction] = 51;
            messageModesMap[tibia::client::MessagePotion] = 52;
        } else if (version >= 1041) {
            messageModesMap[tibia::client::MessageNone] = 0;
            messageModesMap[tibia::client::MessageSay] = 1;
            messageModesMap[tibia::client::MessageWhisper] = 2;
            messageModesMap[tibia::client::MessageYell] = 3;
            messageModesMap[tibia::client::MessagePrivateFrom] = 4;
            messageModesMap[tibia::client::MessagePrivateTo] = 5;
            messageModesMap[tibia::client::MessageChannelManagement] = 6;
            messageModesMap[tibia::client::MessageChannel] = 7;
            messageModesMap[tibia::client::MessageChannelHighlight] = 8;
            messageModesMap[tibia::client::MessageSpell] = 9;
            messageModesMap[tibia::client::MessageNpcFromStartBlock] = 10;
            messageModesMap[tibia::client::MessageNpcFrom] = 11;
            messageModesMap[tibia::client::MessageNpcTo] = 12;
            messageModesMap[tibia::client::MessageGamemasterBroadcast] = 13;
            messageModesMap[tibia::client::MessageGamemasterChannel] = 14;
            messageModesMap[tibia::client::MessageGamemasterPrivateFrom] = 15;
            messageModesMap[tibia::client::MessageGamemasterPrivateTo] = 16;
            messageModesMap[tibia::client::MessageLogin] = 17;
            messageModesMap[tibia::client::MessageWarning] = 18; // Admin
            messageModesMap[tibia::client::MessageGame] = 19;
            messageModesMap[tibia::client::MessageFailure] = 20;
            messageModesMap[tibia::client::MessageLook] = 21;
            messageModesMap[tibia::client::MessageDamageDealed] = 22;
            messageModesMap[tibia::client::MessageDamageReceived] = 23;
            messageModesMap[tibia::client::MessageHeal] = 24;
            messageModesMap[tibia::client::MessageExp] = 25;
            messageModesMap[tibia::client::MessageDamageOthers] = 26;
            messageModesMap[tibia::client::MessageHealOthers] = 27;
            messageModesMap[tibia::client::MessageExpOthers] = 28;
            messageModesMap[tibia::client::MessageStatus] = 29;
            messageModesMap[tibia::client::MessageLoot] = 30;
            messageModesMap[tibia::client::MessageTradeNpc] = 31;
            messageModesMap[tibia::client::MessageGuild] = 32;
            messageModesMap[tibia::client::MessagePartyManagement] = 33;
            messageModesMap[tibia::client::MessageParty] = 34;
            messageModesMap[tibia::client::MessageBarkLow] = 35;
            messageModesMap[tibia::client::MessageBarkLoud] = 36;
            messageModesMap[tibia::client::MessageReport] = 37;
            messageModesMap[tibia::client::MessageHotkeyUse] = 38;
            messageModesMap[tibia::client::MessageTutorialHint] = 49;
            messageModesMap[tibia::client::MessageThankyou] = 40;
            messageModesMap[tibia::client::MessageMarket] = 41;
        } else if (version >= 1036) {
            for (int i = tibia::client::MessageNone; i <= tibia::client::MessageBeyondLast; ++i) {
                if (i >= tibia::client::MessageNpcTo)
                    messageModesMap[i] = i + 1;
                else
                    messageModesMap[i] = i;
            }
        } else if (version >= 900) {
            for (int i = tibia::client::MessageNone; i <= tibia::client::MessageBeyondLast; ++i)
                messageModesMap[i] = i;
        } else if (version >= 861) {
            messageModesMap[tibia::client::MessageNone] = 0;
            messageModesMap[tibia::client::MessageSay] = 1;
            messageModesMap[tibia::client::MessageWhisper] = 2;
            messageModesMap[tibia::client::MessageYell] = 3;
            messageModesMap[tibia::client::MessageNpcTo] = 4;
            messageModesMap[tibia::client::MessageNpcFrom] = 5;
            messageModesMap[tibia::client::MessagePrivateFrom] = 6;
            messageModesMap[tibia::client::MessagePrivateTo] = 6;
            messageModesMap[tibia::client::MessageChannel] = 7;
            messageModesMap[tibia::client::MessageChannelManagement] = 8;
            messageModesMap[tibia::client::MessageGamemasterBroadcast] = 9;
            messageModesMap[tibia::client::MessageGamemasterChannel] = 10;
            messageModesMap[tibia::client::MessageGamemasterPrivateFrom] = 11;
            messageModesMap[tibia::client::MessageGamemasterPrivateTo] = 11;
            messageModesMap[tibia::client::MessageChannelHighlight] = 12;
            messageModesMap[tibia::client::MessageMonsterSay] = 13;
            messageModesMap[tibia::client::MessageMonsterYell] = 14;
            messageModesMap[tibia::client::MessageWarning] = 15;
            messageModesMap[tibia::client::MessageGame] = 16;
            messageModesMap[tibia::client::MessageLogin] = 17;
            messageModesMap[tibia::client::MessageStatus] = 18;
            messageModesMap[tibia::client::MessageLook] = 19;
            messageModesMap[tibia::client::MessageFailure] = 20;
            messageModesMap[tibia::client::MessageBlue] = 21;
            messageModesMap[tibia::client::MessageRed] = 22;
        } else if (version >= 840) {
            messageModesMap[tibia::client::MessageNone] = 0;
            messageModesMap[tibia::client::MessageSay] = 1;
            messageModesMap[tibia::client::MessageWhisper] = 2;
            messageModesMap[tibia::client::MessageYell] = 3;
            messageModesMap[tibia::client::MessageNpcTo] = 4;
            messageModesMap[tibia::client::MessageNpcFromStartBlock] = 5;
            messageModesMap[tibia::client::MessagePrivateFrom] = 6;
            messageModesMap[tibia::client::MessagePrivateTo] = 6;
            messageModesMap[tibia::client::MessageChannel] = 7;
            messageModesMap[tibia::client::MessageChannelManagement] = 8;
            messageModesMap[tibia::client::MessageGamemasterBroadcast] = 12;
            messageModesMap[tibia::client::MessageGamemasterChannel] = 13;
            messageModesMap[tibia::client::MessageGamemasterPrivateFrom] = 14;
            messageModesMap[tibia::client::MessageGamemasterPrivateTo] = 14;
            messageModesMap[tibia::client::MessageChannelHighlight] = 15;
            // 16, 17 ??
            messageModesMap[tibia::client::MessageRed] = 18;
            messageModesMap[tibia::client::MessageMonsterSay] = 19;
            messageModesMap[tibia::client::MessageMonsterYell] = 20;
            messageModesMap[tibia::client::MessageWarning] = 21;
            messageModesMap[tibia::client::MessageGame] = 22;
            messageModesMap[tibia::client::MessageLogin] = 23;
            messageModesMap[tibia::client::MessageStatus] = 24;
            messageModesMap[tibia::client::MessageLook] = 25;
            messageModesMap[tibia::client::MessageFailure] = 26;
            messageModesMap[tibia::client::MessageBlue] = 27;
        } else if (version >= 740) {
            messageModesMap[tibia::client::MessageNone] = 0;
            messageModesMap[tibia::client::MessageSay] = 1;
            messageModesMap[tibia::client::MessageWhisper] = 2;
            messageModesMap[tibia::client::MessageYell] = 3;
            messageModesMap[tibia::client::MessagePrivateFrom] = 4;
            messageModesMap[tibia::client::MessagePrivateTo] = 4;
            messageModesMap[tibia::client::MessageChannel] = 5;
            messageModesMap[tibia::client::MessageGamemasterBroadcast] = 9;
            messageModesMap[tibia::client::MessageGamemasterChannel] = 10;
            messageModesMap[tibia::client::MessageGamemasterPrivateFrom] = 11;
            messageModesMap[tibia::client::MessageGamemasterPrivateTo] = 11;
            messageModesMap[tibia::client::MessageChannelHighlight] = 12;
            // 13, 14, 15 ??
            messageModesMap[tibia::client::MessageMonsterSay] = 16;
            messageModesMap[tibia::client::MessageMonsterYell] = 17;
            messageModesMap[tibia::client::MessageWarning] = 18;
            messageModesMap[tibia::client::MessageGame] = 19;
            messageModesMap[tibia::client::MessageLogin] = 20;
            messageModesMap[tibia::client::MessageStatus] = 21;
            messageModesMap[tibia::client::MessageLook] = 22;
            messageModesMap[tibia::client::MessageFailure] = 23;
            messageModesMap[tibia::client::MessageBlue] = 24;
            messageModesMap[tibia::client::MessageRed] = 25;
        }
    }

    tibia::client::MessageMode translateMessageModeFromServer(const uint8_t mode)
    {
        const auto it = std::ranges::find_if(messageModesMap, [=](const std::pair<uint8_t, uint8_t >& p) { return p.second == mode; });
        if (it != messageModesMap.end())
            return static_cast<tibia::client::MessageMode>(it->first);
        return tibia::client::MessageInvalid;
    }

    uint8_t translateMessageModeToServer(const tibia::client::MessageMode mode)
    {
        if (mode >= tibia::client::LastMessage)
            return tibia::client::MessageInvalid;
        if (const auto it = messageModesMap.find(mode); it != messageModesMap.end())
            return it->second;
        return tibia::client::MessageInvalid;
    }
}