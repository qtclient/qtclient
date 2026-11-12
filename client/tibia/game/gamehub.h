#pragma once

/// Tip-bridge accessor for the private `Game` hub (`s_tipGameHub` in
/// `gamehub_impl.cpp`). Packs must not include this — use `gamesession` peels.
/// Peel translation units include this (not `gamehub_impl.h`) so the symbol
/// ratchet can descend.

class Game;

Game& tipGameHub();
