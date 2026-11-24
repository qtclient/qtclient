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

#include "client.h"

#include <tibia/game/gamesession.h>
#include <tibia/worldmap/worldmapstorage.h>
#include <tibia/config/runtimegameconfig.h>
#include "tibia/assets/things.h"

Client g_client;

void Client::init(std::vector<std::string>& /*args*/)
{
    tibia::config::initRuntimeGameConfig();
    tibia::worldmap::TWorldMapStorage::ensureStorage()->resetAwareRange();
    tibia::game::initGameHub();
    tibia::appearances::TAppearancesManager::instance().init();
}

void Client::terminate()
{
    tibia::game::terminateGameHub();
    tibia::worldmap::TWorldMapStorage::ensureStorage()->clean();
    tibia::appearances::TAppearancesManager::instance().terminate();
    tibia::config::terminateRuntimeGameConfig();
}
