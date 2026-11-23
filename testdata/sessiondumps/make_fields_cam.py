#!/usr/bin/env python3
"""Writes fields.cam: a synthetic session dump for the no-server tile render check.

The player stands at (100, 100, 7) on a lit field set of real appearances:
grounds 4526 and 351 (351 draws a 4x4 pattern from the field coordinate),
50 gold coins (3031, pile pattern), a white flower (102) lying on a backpack (2854)
(common objects come newest first on the wire: the flower is sent before the
backpack), the player in outfit 128 with colours, a second creature (outfit 1581
with both addons, which it draws in reverse order looking west) that steps one
field west 200 ms into the replay (done long before the shot),
and effect 209 on the field west of the player. The player's stats are full
(health and mana), so its HUD bars are full.
The objects and outfit are single-phase. The screenshot is taken 1500 ms after
world entry (--screenshot-delay=1500) while the effect is sent 1450 ms after the
replay starts; world entry takes a few hundred ms, and effect 209's first phase
lasts 999 ms, so the shot shows its first phase on every run.
Usage: make_fields_cam.py > fields.cam
"""
import struct

PLAYER = 0x10000001
WALKER = 0x40000001
X, Y, Z = 100, 100, 7
LEFT, TOP, RIGHT, BOTTOM = 8, 6, 9, 7


def u8(v): return struct.pack('<B', v)
def u16(v): return struct.pack('<H', v)
def u32(v): return struct.pack('<I', v)
def string(s): return u16(len(s)) + s.encode()
def position(x, y, z): return u16(x) + u16(y) + u8(z)
def ot_double(v): return u8(3) + u32(int(round(v * 1000)) + 0x7fffffff)


def walker_creature():
    return (u16(0x61) + u32(0) + u32(WALKER) + u8(1) + string('Walker') + u8(100) + u8(1)
            + u16(1581) + u8(94) + u8(86) + u8(10) + u8(114) + u8(3)  # outfit + colours + addons
            + u16(0)  # no mount
            + u8(0) + u8(0) + u16(2000)  # light, speed (a short step)
            + u8(0) + u8(0)  # icons, extra icons
            + u8(0) + u8(0) + u8(0)  # skull, shield, emblem
            + u8(1)  # monster
            + u8(0) + u8(0) + u8(0) + u8(0))  # icon, mark, inspection, unpassable


def player_creature():
    return (u16(0x61) + u32(0) + u32(PLAYER) + u8(0) + string('Render Check') + u8(100) + u8(2)
            + u16(128) + u8(78) + u8(69) + u8(58) + u8(76) + u8(0)  # outfit + colours + addons
            + u16(0)  # no mount
            + u8(0) + u8(0) + u16(220)  # light, speed
            + u8(0) + u8(0)  # icons, extra icons
            + u8(0) + u8(0) + u8(0)  # skull, shield, emblem
            + u8(0) + u8(1)  # player, vocation
            + u8(0) + u8(0) + u8(0) + u8(0))  # icon, mark, inspection, unpassable


def field_objects(x, y, z):
    if z != Z:
        return b''
    dx, dy = x - X, y - Y
    ground = u16(351) if -2 <= dx <= 1 and -2 <= dy <= 1 else u16(4526)
    if (dx, dy) == (0, 0):
        return ground + player_creature()
    if (dx, dy) == (1, 0):
        return ground + u16(3031) + u8(50)
    if (dx, dy) == (2, 0):
        return ground + u16(102) + u16(2854) + u8(0)
    if (dx, dy) == (-1, -2):
        return ground + walker_creature()
    return ground


def full_map():
    body = u8(0x64) + position(X, Y, Z)
    for z in range(7, -1, -1):
        offset = Z - z
        for nx in range(LEFT + RIGHT + 1):
            for ny in range(TOP + BOTTOM + 1):
                x, y = X - LEFT + nx + offset, Y - TOP + ny + offset
                body += field_objects(x, y, z) + u16(0xff00)
    return body


def login_success():
    return u8(0x17) + u32(PLAYER) + u16(50) + ot_double(857.36) + ot_double(261.29) + ot_double(-4795.01) + u8(0) + u8(0)


def player_data_current():
    # The player's stats (PlayerDataCurrent): full health and mana, so the
    # player's HUD bars (read from the player data) are full.
    return (u8(0xa0) + u32(185) + u32(185) + u32(40000)  # health, max health, free capacity
            + struct.pack('<Q', 4200) + u16(8) + u16(0)  # experience, level, level percent
            + u16(100) + u16(0) + u16(0) + u16(100)  # base xp gain, low level bonus, store boost, stamina factor
            + u32(90) + u32(90) + u8(100)  # mana, max mana, soul
            + u16(2520) + u16(220) + u16(0) + u16(0) + u16(0) + u8(0)  # stamina, base speed, regeneration, training, store boost time, purchasable
            + u32(0) + u32(0) + u16(0) + u16(0))  # mana shield, max mana shield, daily reward boost, its time


def walk():
    # The walker (stack position 1, after the ground) steps one field west.
    return u8(0x6d) + position(X - 1, Y - 2, Z) + u8(1) + position(X - 2, Y - 2, Z)


def effect():
    # Graphical effects from the field west of the player: effect 209, default source.
    return u8(0x83) + position(X - 1, Y, Z) + u8(3) + u16(209) + u8(0) + u8(0)


lines = [
    (0, login_success()),
    (1, u8(0x0f)),
    (2, u8(0x82) + u8(255) + u8(215)),
    (3, full_map()),
    (4, player_data_current()),
    (200, walk()),
    (1450, effect()),
    (8000, u8(0x05) + u8(0x05) + u8(0x00) + b'smoke'),
]
for millis, body in lines:
    print(f'< {millis} {body.hex()}')
