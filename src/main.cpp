#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;

namespace {
    constexpr int JUMP_BTN = 1;

    struct Slot {
        bool held = false;
        int sincePress = 0;
        int pendingRelease = -1;
    };
    struct Preset { const char* name; int press; int wave; };

    // 0 = Custom (from settings), 1..4 = fixed stages, 5 = Off
    constexpr Preset PRESETS[] = {
        {"Custom", 0, 0},
        {"Stage 1: 2 / 2", 2, 2},
        {"Stage 2: 4 / 4", 4, 4},
        {"Stage 3: 8 / 8", 8, 8},
        {"Stage 4: 16 / 8", 16, 8},
        {"OFF", 0, 0},
    };
    constexpr int PRESET_COUNT = 6;

    Slot g_slots[2];
    bool g_replaying = false;
    bool g_f6Down = false;
    int  g_preset = -1;   // -1 = not initialised from settings yet

    void resetState() {
        g_slots[0] = Slot{};
        g_slots[1] = Slot{};
        g_replaying = false;
    }

    int curPreset() {
        if (g_preset < 0) {
            g_preset = static_cast<int>(Mod::get()->getSettingValue<int64_t>("start-preset"));
            if (g_preset < 0 || g_preset >= PRESET_COUNT) g_preset = 0;
        }
        return g_preset;
    }

    bool on() {
        return Mod::get()->getSettingValue<bool>("enabled") && curPreset() != 5;
    }
    int pressTicks() {
        int p = curPreset();
        if (p == 0) return static_cast<int>(Mod::get()->getSettingValue<int64_t>("press-ticks"));
        return PRESETS[p].press;
    }
    int waveTicks() {
        int p = curPreset();
        if (p == 0) return static_cast<int>(Mod::get()->getSettingValue<int64_t>("wave-release-ticks"));
        return PRESETS[p].wave;
    }
    bool useP2()   { return Mod::get()->getSettingValue<bool>("player2"); }
    bool logging() { return Mod::get()->getSettingValue<bool>("log"); }

    // Re-sending a press is only harmless where a press does nothing on its own
    // (it only matters for rings/pads). In ship/wave a press is a hold, and in
    // ufo/swing a press always acts, so replaying there would misbehave.
    bool replaySafe(PlayerObject* p) {
        if (!p) return false;
        if (p->m_isShip || p->m_isDart || p->m_isBird || p->m_isSwing) return false;
        return !p->m_isOnGround; // airborne cube/ball/robot/spider
    }
}

class $modify(BufGameLayer, GJBaseGameLayer) {
    void processCommands(float dt) {
        if (PlayLayer::get() == this) {
#ifdef GEODE_IS_WINDOWS
            // F6 cycles presets (polled, so no keyboard-dispatcher signature dependency)
            bool f6 = GetActiveWindow() && (GetAsyncKeyState(VK_F6) & 0x8000);
            if (f6 && !g_f6Down) {
                g_preset = (curPreset() + 1) % PRESET_COUNT;
                Notification::create(
                    fmt::format("Input Buffer: {}", PRESETS[g_preset].name),
                    NotificationIcon::None, 1.0f)->show();
            }
            g_f6Down = f6;
#endif
            if (on()) {
                for (int i = 0; i < 2; ++i) {
                    if (i == 1 && !useP2()) continue;
                    Slot& s = g_slots[i];
                    bool isP1 = (i == 0);
                    PlayerObject* pl = isP1 ? m_player1 : m_player2;

                    if (s.held && s.sincePress < pressTicks() && replaySafe(pl)) {
                        ++s.sincePress;
                        g_replaying = true;
                        GJBaseGameLayer::handleButton(true, JUMP_BTN, isP1);
                        g_replaying = false;
                        if (logging()) log::debug("press re-sent (p{}, tick {})", i + 1, s.sincePress);
                    }

                    if (s.pendingRelease >= 0) {
                        if (s.pendingRelease == 0) {
                            s.pendingRelease = -1;
                            g_replaying = true;
                            GJBaseGameLayer::handleButton(false, JUMP_BTN, isP1);
                            g_replaying = false;
                        } else {
                            --s.pendingRelease;
                        }
                    }
                }
            }
        }
        GJBaseGameLayer::processCommands(dt);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        bool bypass = g_replaying || button != JUMP_BTN || PlayLayer::get() != this || !on();
        int i = isPlayer1 ? 0 : 1;
        if (bypass || (i == 1 && !useP2())) {
            // keep held-state accurate even when buffering is off, so toggling mid-level is safe
            if (button == JUMP_BTN && !g_replaying) g_slots[i].held = down;
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }
        Slot& s = g_slots[i];
        PlayerObject* pl = isPlayer1 ? m_player1 : m_player2;

        if (down) {
            s.held = true;
            s.sincePress = 0;
            if (s.pendingRelease >= 0) { s.pendingRelease = -1; return; }
            GJBaseGameLayer::handleButton(true, button, isPlayer1);
        } else {
            s.held = false;
            int d = (pl && pl->m_isDart) ? waveTicks() : 0; // delayed release only for wave
            if (d > 0) s.pendingRelease = d;
            else GJBaseGameLayer::handleButton(false, button, isPlayer1);
        }
    }
};

class $modify(BufPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        resetState();
    }
    void onQuit() {
        resetState();
        PlayLayer::onQuit();
    }
};
