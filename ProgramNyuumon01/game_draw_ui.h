// ====================================================
// 画面表示（HUD・技表・ポーズ・キャラカード）
// ====================================================
#pragma once

enum HudMode { HUD_STORY, HUD_NET, HUD_TRAINING, HUD_TUTORIAL };

static void DrawInputHistory(int y) {
    int x = 8;
    for (int i = 0; i < g_inputHistCount; i++) {
        const InputHistEntry& e = g_inputHist[i];
        char buf[48] = "";
        unsigned short b = e.bits;
        if (b & IN_LEFT)  strcat_s(buf, "←");
        if (b & IN_RIGHT) strcat_s(buf, "→");
        if (b & IN_UP)    strcat_s(buf, "↑");
        if (b & IN_DOWN)  strcat_s(buf, "↓");
        if (!(b & (IN_LEFT | IN_RIGHT | IN_UP | IN_DOWN))) strcat_s(buf, "・");
        if (b & IN_J) strcat_s(buf, "J");
        if (b & IN_K) strcat_s(buf, "K");
        if (b & IN_L) strcat_s(buf, "L");
        if (b & IN_SUPER) strcat_s(buf, "I");
        if (b & IN_SPACE) strcat_s(buf, "_SP");
        if (b & IN_THROW) strcat_s(buf, "_投");
        int w = GetDrawStringWidth(buf, (int)strlen(buf));
        char fr[8];
        sprintf_s(fr, "%d", e.frames > 99 ? 99 : e.frames);
        int fw = GetDrawStringWidth(fr, (int)strlen(fr));
        if (x + w + fw + 10 > 632) {
            if (y > 450) break;
            x = 8;
            y += 18;
        }
        unsigned int col = (i == 0) ? YELLOW : ((b & (IN_J | IN_K | IN_L | IN_SPACE | IN_THROW | IN_SUPER)) ? WHITE : GRAY);
        DrawBox(x - 2, y - 1, x + w + fw + 6, y + 16, GetColor(10, 10, 20), TRUE);
        DrawString(x, y, buf, col);
        DrawString(x + w + 3, y, fr, GRAY);
        x += w + fw + 12;
    }
}

static void DrawHpBar(int x1, int x2, int y, const Fighter& f, bool fromRight) {
    DrawBox(x1, y, x2, y + 22, GetColor(50, 15, 15), TRUE);
    float r = (float)f.hp / (float)f.maxHp;
    if (r < 0) r = 0;
    unsigned int col = (r < 0.25f) ? RED : ((r < 0.5f) ? YELLOW : GREEN);
    int w = (int)((x2 - x1) * r);
    if (fromRight) DrawBox(x2 - w, y, x2, y + 22, col, TRUE);
    else DrawBox(x1, y, x1 + w, y + 22, col, TRUE);
    DrawBox(x1, y, x2, y + 22, WHITE, FALSE);
}

static void DrawSpBar(int x, int y, const Fighter& f, bool right) {
    DrawBox(x, y, x + 180, y + 12, GetColor(15, 20, 35), TRUE);
    int w = 180 * f.spGauge / 100;
    unsigned int col = (f.spGauge >= 100) ? ((g_frameCount / 4) % 2 ? YELLOW : ORANGE) : CYAN;
    if (right) DrawBox(x + 180 - w, y, x + 180, y + 12, col, TRUE);
    else DrawBox(x, y, x + w, y + 12, col, TRUE);
    DrawLine(x + 90, y, x + 90, y + 12, GRAY);
    DrawBox(x, y, x + 180, y + 12, WHITE, FALSE);
}

static void DrawHud(HudMode hud) {
    bool isNet = (hud == HUD_NET);
    bool practice = (hud == HUD_TRAINING || hud == HUD_TUTORIAL);
    int stage = ClampI(g_slots[g_currentSlot].stageProgress, 0, LAST_STAGE);

    DrawHpBar(30, 280, 22, g_me, false);
    DrawHpBar(360, 610, 22, g_other, true);

    if (isNet) DrawFormatString(32, 4, WHITE, "YOU: %s", CHARA_PROFILES[g_me.charaIndex].charaId);
    else if (practice) DrawFormatString(32, 4, WHITE, "%s: %s", (hud == HUD_TRAINING) ? "TRAINING" : "TUTORIAL", CHARA_PROFILES[g_me.charaIndex].charaId);
    else DrawFormatString(32, 4, WHITE, "DATA %d: %s", g_currentSlot + 1, CHARA_PROFILES[g_me.charaIndex].charaId);
    if (isNet) DrawFormatString(420, 4, WHITE, "2P: %s", CHARA_PROFILES[g_other.charaIndex].charaId);
    else if (practice) DrawFormatString(420, 4, WHITE, "DUMMY: %s", CHARA_PROFILES[g_other.charaIndex].charaId);
    else DrawFormatString(360, 4, WHITE, "%s", CPU_ROSTER[stage].name);

    // タイマー
    DrawBox(292, 16, 348, 50, GetColor(20, 20, 30), TRUE);
    bool infiniteTime = practice || (isNet && g_netRules.timeLimit == 0);
    DrawBox(292, 16, 348, 50, (!infiniteTime && g_roundTime <= 10) ? RED : YELLOW, FALSE);
    if (infiniteTime) DrawString(312, 24, "∞", YELLOW);
    else DrawFormatString(304, 24, (g_roundTime <= 10) ? RED : YELLOW, "%02d", g_roundTime);

    if (hud == HUD_STORY) {
        const SaveSlot& s = g_slots[g_currentSlot];
        DrawFormatString(30, 48, GRAY, "%d周目 %s", GetCurrentLap(), DIFFICULTY_NAMES[s.difficulty]);
        if (g_cpuTilt > 0) {
            char bars[16] = "";
            for (int i = 0; i < g_cpuTilt && i < 6; i++) strcat_s(bars, "■");
            DrawFormatString(420, 48, RED, "COMイライラ %s", bars);
        }
    }
    if (isNet) {
        int myIdx = g_is_host ? 0 : 1;
        for (int r = 0; r < g_netRules.roundsToWin; r++) {
            DrawCircle(270 - r * 16, 56, 6, (r < g_netRounds[myIdx]) ? YELLOW : GetColor(60, 60, 60), TRUE);
            DrawCircle(370 + r * 16, 56, 6, (r < g_netRounds[1 - myIdx]) ? YELLOW : GetColor(60, 60, 60), TRUE);
        }
    }

    // SPゲージ（下）
    DrawSpBar(30, 414, g_me, false);
    DrawSpBar(430, 414, g_other, true);
    if (g_me.spGauge >= 100) DrawString(215, 412, "SP MAX! [I]", YELLOW);
    else if (g_me.spGauge >= 50 && g_me.burstCooldown == 0) DrawString(215, 412, "BURST OK", CYAN);

    // コンボ数（大きく）とカウンター表示
    const Fighter* fs[2] = { &g_me, &g_other };
    for (int i = 0; i < 2; i++) {
        const Fighter& f = *fs[i];
        int x = (i == 0) ? 20 : 470;
        if (f.comboCount > 1 && f.comboDisplayTimer > 0) {
            char buf[32];
            sprintf_s(buf, "%d HITS", f.comboCount);
            DrawExtendString(x + 1, 81, 2.0, 2.0, buf, BLACK);
            DrawExtendString(x, 80, 2.0, 2.0, buf, (f.comboCount >= 8) ? ORANGE : YELLOW);
            if (g_other.wallBounceUsed && i == 0) DrawString(x, 116, "WALL COMBO!", ORANGE);
        }
        if (g_counterLabelTimer[i] > 0) {
            const char* txt = (g_counterLabelType[i] == 1) ? "COUNTER" : "PUNISH COUNTER";
            int w = GetDrawStringWidth(txt, (int)strlen(txt));
            int lx = (i == 0) ? 20 : 620 - w - 10;
            DrawBox(lx - 4, 136, lx + w + 6, 156, GetColor(120, 40, 10), TRUE);
            DrawBox(lx - 4, 136, lx + w + 6, 156, ORANGE, FALSE);
            DrawString(lx + 1, 138, txt, WHITE);
        }
    }

    if (g_parryNoticeTimer > 0) g_parryNoticeTimer--;

    // カットイン
    if (g_superCutinTimer > 0) {
        if (g_cutinIsCounter) {
            DrawBox(0, 185, 640, 275, GetColor(0, 15, 25), TRUE);
            DrawBox(0, 185, 640, 275, CYAN, FALSE);
            DrawBox(3, 188, 637, 272, WHITE, FALSE);
            DrawString(230, 200, "!!! C O U N T E R !!!", (rand() % 2 == 0) ? CYAN : WHITE);
            DrawString(150, 235, g_superCutinName, YELLOW);
        }
        else if (g_superIsUltimate) {
            DrawBox(0, 180, 640, 280, GetColor(20, 0, 0), TRUE);
            for (int b = 0; b < 6; b++) {
                int rcol = GetColor((rand() % 155) + 100, (rand() % 155) + 100, (rand() % 155) + 100);
                DrawBox(0 + b, 180 + b, 640 - b, 280 - b, rcol, FALSE);
            }
            DrawString(180, 195, "★★★ 確 変 突 入 !! 超 激 熱 ★★★", YELLOW);
            DrawString(130, 225, g_superCutinName, (rand() % 2 == 0 ? WHITE : YELLOW));
        }
        else {
            DrawBox(0, 195, 640, 265, GetColor(0, 0, 0), TRUE);
            DrawBox(0, 195, 640, 265, RED, FALSE);
            DrawString(200, 205, "=== SUPER COMBO ===", YELLOW);
            int w = GetDrawStringWidth(g_superCutinName, (int)strlen(g_superCutinName));
            DrawString(320 - w / 2, 232, g_superCutinName, WHITE);
        }
    }

    if (!practice) {
        if (g_readyFightTimer < 50) {
            DrawBox(220, 195, 420, 245, GetColor(0, 0, 0), TRUE);
            DrawBox(220, 195, 420, 245, YELLOW, FALSE);
            if (isNet) DrawFormatString(278, 212, YELLOW, "ROUND %d", g_netRoundNo);
            else DrawString(285, 212, "READY ?", YELLOW);
        }
        else if (g_readyFightTimer < 80) {
            DrawBox(200, 190, 440, 250, GetColor(0, 0, 0), TRUE);
            DrawBox(200, 190, 440, 250, RED, FALSE);
            DrawString(280, 210, "F I G H T !!", RED);
        }
    }

    if (hud == HUD_TRAINING && g_showInput) {
        DrawInputHistory(436);
    }
    else {
        DrawString(16, 438, "[A/D]移動 [W]跳 [S]しゃがみ [SPACE]ガード/短押しパリィ [SPACE+A/D]回避", WHITE);
        DrawString(16, 458, "[J]弱 [K]強 [L]必殺(前/下で変化) [左Shift]投げ [I]超必殺 [ESC]ポーズ [TAB]技表", WHITE);
    }

    if (g_flashTimer > 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, g_flashTimer * 8);
        DrawBox(0, 0, 640, 480, WHITE, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

// トレーニングの情報パネル
static void DrawTrainingOverlay() {
    // 情報はキャラにかぶらないよう画面の上側にまとめる
    DrawString(30, 50, "F1判定 F2フレーム F3入力 F4SP F5/F7位置 F6課題", GRAY);
    DrawFormatString(440, 50, YELLOW, "ダミー:[%d]%s", (int)g_dummyMode + 1, DUMMY_MODE_NAMES[g_dummyMode]);
    if (g_infiniteSp) DrawString(215, 396, "SP無限", CYAN);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 200);
    DrawBox(6, 68, 316, 140, GetColor(0, 0, 0), TRUE);
    DrawBox(324, 68, 634, 140, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(6, 68, 316, 140, GRAY, FALSE);
    DrawBox(324, 68, 634, 140, GRAY, FALSE);

    if (g_showFrames) {
        const MoveInfo& m = g_moveInfo[0];
        if (m.valid) {
            DrawFormatString(12, 71, WHITE, "%s（%s）", g_moves[m.chara][m.slot].name, SLOT_INPUT[m.slot]);
            DrawFormatString(12, 88, CYAN, "発生%dF 持続%dF 硬直%dF 全体%dF", m.startup, m.active, m.recovery, m.total);
        }
        else {
            DrawString(12, 71, "技を出すとフレームを表示", GRAY);
        }
        if (g_adv.valid) {
            const char* who = g_adv.byDummy ? "ダミー" : "自分";
            if (g_adv.launch && !g_adv.guarded) DrawFormatString(12, 105, GREEN, "%s: 打ち上げ・ダウン（追撃可）", who);
            else DrawFormatString(12, 105, g_adv.adv >= 0 ? GREEN : RED, "%s %s時: %+dF", who, g_adv.guarded ? "ガード" : "ヒット", g_adv.adv);
            if (g_adv.punishable) {
                DrawString(12, 122, g_adv.byDummy ? "確定反撃チャンス！" : "確定反撃あり！（相手の最速技が入る）",
                    (g_frameCount / 8) % 2 ? YELLOW : RED);
            }
        }
    }
    else {
        DrawString(12, 71, "フレーム表示 OFF（F2）", GRAY);
    }

    if (g_comboHits > 0) DrawFormatString(330, 71, YELLOW, "コンボ: %dHit %dダメージ", g_comboHits, g_comboDmg);
    else DrawFormatString(330, 71, WHITE, "前回: %dHit %dダメージ", g_lastComboHits, g_lastComboDmg);
    DrawFormatString(330, 88, ORANGE, "最大: %dHit / %dダメージ", g_maxComboHits, g_maxComboDmg);
    const ComboChallenge& ch = CHALLENGES[g_me.charaIndex][g_challengeIdx];
    DrawFormatString(330, 105, g_challengeDone[g_challengeIdx] ? GREEN : CYAN, "課題%d/3%s %s",
        g_challengeIdx + 1, g_challengeDone[g_challengeIdx] ? "[済]" : "", ch.desc);
    if (ch.hint[0]) DrawFormatString(330, 122, GRAY, "%s", ch.hint);
}

static const char* MoveTraitText(const MoveDef& m) {
    static char buf[64];
    buf[0] = '\0';
    if (m.flags & MF_LOW) strcat_s(buf, "[下段]");
    if (m.flags & MF_OVERHEAD) strcat_s(buf, "[中段]");
    if (m.flags & MF_LAUNCH) strcat_s(buf, "[打ち上げ]");
    if (m.flags & MF_KNOCKDOWN) strcat_s(buf, "[ダウン]");
    if (m.flags & MF_UNBLOCK) strcat_s(buf, "[ガード不能]");
    if (m.flags & MF_INVUL) strcat_s(buf, "[出始め無敵]");
    if (m.flags & MF_ARMOR) strcat_s(buf, "[アーマー]");
    if (m.whiff > 0) strcat_s(buf, "[空振り大スキ]");
    if (m.behavior == BH_SHOT || m.behavior == BH_WATERMELON || m.behavior == BH_SHOCKWAVE ||
        m.behavior == BH_TRAP_THROW || m.behavior == BH_8WAY) strcat_s(buf, "[飛び道具]");
    return buf;
}

// コマンドリスト（TAB）
static void DrawCommandList(bool training) {
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 230);
    DrawBox(16, 24, 624, 456, GetColor(5, 8, 18), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(16, 24, 624, 456, YELLOW, FALSE);

    int pageCount = training ? 3 : 2;
    int page = ClampI(g_cmdListPage, 0, pageCount - 1);
    const char* pageName[3] = { "技表・コンボ", "共通システム", "トレーニング操作" };
    DrawFormatString(28, 30, YELLOW, "コマンドリスト [%s] %d/%d", pageName[page], page + 1, pageCount);
    DrawString(330, 30, "A/D:キャラ W/S:ページ TAB:閉じる", GRAY);

    int y = 54;
    if (page == 0) {
        int c = g_cmdListChara;
        const CharacterProfile& p = CHARA_PROFILES[c];
        if (g_faceGraph[c] != -1) DrawExtendGraph(540, 50, 612, 122, g_faceGraph[c], TRUE);
        DrawFormatString(28, y, WHITE, "%s", p.charaId);
        DrawFormatString(220, y, (p.tier == 0) ? GREEN : ((p.tier == 1) ? YELLOW : RED), "%s", TIER_NAMES[p.tier]);
        y += 20;
        DrawFormatString(28, y, GRAY, "%s", p.archetype);
        y += 24;
        for (int s = 0; s < MS_COUNT; s++) {
            const MoveDef& m = g_moves[c][s];
            if (!m.valid) continue;
            DrawFormatString(36, y, CYAN, "%s", SLOT_INPUT[s]);
            DrawFormatString(112, y, WHITE, "%s", m.name);
            DrawFormatString(340, y, ORANGE, "%s", MoveTraitText(m));
            y += 18;
        }
        DrawString(36, y, "左Shift", CYAN);
        DrawString(112, y, "投げ（つかんで後ろを押すと裏投げ）", WHITE);
        y += 22;
        for (int i = 0; i < 2; i++) {
            if (CMD_TRAITS[c][i][0]) { DrawString(28, y, CMD_TRAITS[c][i], YELLOW); y += 18; }
        }
        y += 4;
        DrawString(28, y, "▼ おすすめコンボ（トレーニングの課題）", ORANGE);
        y += 20;
        for (int i = 0; i < 3 && y < 440; i++) {
            DrawFormatString(36, y, CYAN, "%d. %s", i + 1, CHALLENGES[c][i].desc);
            y += 18;
        }
    }
    else {
        const char** lines = (page == 1) ? CMD_COMMON : CMD_TRAINING;
        for (int i = 0; lines[i]; i++) {
            DrawString(36, y, lines[i], WHITE);
            y += 26;
        }
    }
}

static void HandleCommandListKeys(int defaultChara, bool training, bool allowCharaSwitch) {
    if (KeyHit(KEY_INPUT_TAB)) {
        g_cmdListOpen = !g_cmdListOpen;
        if (g_cmdListOpen) {
            g_cmdListChara = defaultChara;
            g_cmdListPage = 0;
        }
        return;
    }
    if (!g_cmdListOpen) return;
    int pageCount = training ? 3 : 2;
    if (KeyHit(KEY_INPUT_W) || KeyHit(KEY_INPUT_UP)) g_cmdListPage = (g_cmdListPage + pageCount - 1) % pageCount;
    if (KeyHit(KEY_INPUT_S) || KeyHit(KEY_INPUT_DOWN)) g_cmdListPage = (g_cmdListPage + 1) % pageCount;
    if (allowCharaSwitch) {
        if (KeyHit(KEY_INPUT_A) || KeyHit(KEY_INPUT_LEFT)) g_cmdListChara = (g_cmdListChara + CHARA_COUNT - 1) % CHARA_COUNT;
        if (KeyHit(KEY_INPUT_D) || KeyHit(KEY_INPUT_RIGHT)) g_cmdListChara = (g_cmdListChara + 1) % CHARA_COUNT;
    }
}

// ----------------------------------------------------
// ポーズメニュー（ESC）
// ----------------------------------------------------
enum PauseResult { PAUSE_NONE, PAUSE_RESUME, PAUSE_OPTION1, PAUSE_OPTION2 };

// 戻り値：選ばれた項目（0=続ける 1=項目1 2=項目2）。何も選ばれていなければ -1
static int HandlePauseMenu(const char* opt1, const char* opt2) {
    if (!g_paused) {
        if (KeyHit(KEY_INPUT_ESCAPE)) {
            g_paused = true;
            g_pauseCursor = 0;
            g_cmdListOpen = false;
        }
        return -1;
    }
    int count = opt2 ? 3 : 2;
    if (KeyHit(KEY_INPUT_ESCAPE)) { g_paused = false; return 0; }
    if (KeyHit(KEY_INPUT_W) || KeyHit(KEY_INPUT_UP)) g_pauseCursor = (g_pauseCursor + count - 1) % count;
    if (KeyHit(KEY_INPUT_S) || KeyHit(KEY_INPUT_DOWN)) g_pauseCursor = (g_pauseCursor + 1) % count;
    if (KeyHit(KEY_INPUT_RETURN)) {
        g_paused = false;
        s_keyCooldown = 15;
        return g_pauseCursor;
    }
    return -1;
}

static void DrawPauseMenu(const char* opt1, const char* opt2) {
    if (!g_paused) return;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
    DrawBox(0, 0, 640, 480, BLACK, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(170, 150, 470, 330, GetColor(15, 18, 30), TRUE);
    DrawBox(170, 150, 470, 330, YELLOW, FALSE);
    DrawExtendString(250, 162, 2.0, 2.0, "PAUSE", YELLOW);
    const char* items[3] = { "続ける", opt1, opt2 };
    int count = opt2 ? 3 : 2;
    for (int i = 0; i < count; i++) {
        bool sel = (g_pauseCursor == i);
        DrawFormatString(200, 220 + i * 30, sel ? YELLOW : WHITE, "%s%s", sel ? "> " : "  ", items[i]);
    }
    DrawString(188, 306, "W/S:選択 Enter:決定 ESC:戻る", GRAY);
}

static void DrawStatsBlock(int x, int y, int idx, int chara, unsigned int col) {
    const MatchStats& s = g_stats[idx];
    DrawFormatString(x, y, col, "使用技 %d回（最多: %s）", s.moves, MostUsedMove(idx, chara));
    DrawFormatString(x, y + 20, col, "ヒット %d / 被弾 %d / 与ダメージ %d", s.hits, s.taken, s.damage);
    DrawFormatString(x, y + 40, col, "ガード %d / パリィ %d / 最大コンボ %d", s.guards, s.parries, s.maxCombo);
}

// ----------------------------------------------------
// キャラ選択のカード
// ----------------------------------------------------
static void DrawPadlock(int x, int y, int size) {
    DrawCircle(x, y - size / 2, size / 2, YELLOW, FALSE, 3);
    DrawBox(x - size * 3 / 4, y - size / 3, x + size * 3 / 4, y + size * 2 / 3, YELLOW, TRUE);
    DrawCircle(x, y + 2, size / 6 + 1, BLACK, TRUE);
}

static void DrawFace(int c, int x1, int y1, int x2, int y2, bool silhouette) {
    if (g_faceGraph[c] == -1) {
        DrawCircle((x1 + x2) / 2, (y1 + y2) / 2, (x2 - x1) / 3, silhouette ? GetColor(40, 40, 50) : CHARA_PROFILES[c].baseColor, TRUE);
        return;
    }
    int size = (x2 - x1 < y2 - y1) ? (x2 - x1) : (y2 - y1);
    int cx = (x1 + x2) / 2, cy = (y1 + y2) / 2;
    if (silhouette) SetDrawBright(25, 25, 35);
    DrawExtendGraph(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2, g_faceGraph[c], TRUE);
    SetDrawBright(255, 255, 255);
}

static void DrawTierBadge(int tier, int x, int y) {
    unsigned int col = (tier == 0) ? GREEN : ((tier == 1) ? YELLOW : RED);
    const char* t = TIER_NAMES[tier];
    int w = GetDrawStringWidth(t, (int)strlen(t));
    DrawBox(x, y, x + w + 8, y + 18, GetColor(20, 20, 20), TRUE);
    DrawBox(x, y, x + w + 8, y + 18, col, FALSE);
    DrawString(x + 4, y + 1, t, col);
}

static void DrawPokeCard(int charaIdx, float offsetX, float scale, bool isSelected, int rank, bool noCon, bool locked, bool isNew) {
    int centerX = 320 + (int)offsetX;
    int centerY = 210;
    int cardW = (int)(180.0f * scale);
    int cardH = (int)(240.0f * scale);
    int x1 = centerX - cardW / 2, y1 = centerY - cardH / 2;
    int x2 = centerX + cardW / 2, y2 = centerY + cardH / 2;
    int illX1 = x1 + (int)(10.0f * scale), illY1 = y1 + (int)(10.0f * scale);
    int illX2 = x2 - (int)(10.0f * scale), illY2 = y1 + (int)(140.0f * scale);

    if (locked) {
        DrawBox(x1, y1, x2, y2, GetColor(10, 10, 16), TRUE);
        DrawBox(x1, y1, x2, y2, isSelected ? YELLOW : GetColor(80, 70, 40), FALSE);
        DrawBox(illX1, illY1, illX2, illY2, GetColor(20, 20, 28), TRUE);
        DrawFace(charaIdx, illX1, illY1, illX2, illY2, true);   // シルエットでチラ見せ
        DrawPadlock((illX1 + illX2) / 2, (illY1 + illY2) / 2, (int)(26.0f * scale));
        if (scale > 0.85f) {
            DrawString(x1 + 8, y1 + (int)(150.0f * scale), "？？？？？", GRAY);
            DrawString(x1 + 8, y1 + (int)(172.0f * scale), "LOCKED", RED);
            DrawFormatString(x1 + 8, y1 + (int)(194.0f * scale), YELLOW, "%d周目で解放", CHARA_PROFILES[charaIdx].unlockLap);
        }
        return;
    }

    DrawBox(x1, y1, x2, y2, GetColor(18, 22, 36), TRUE);
    bool isCleared = rank > 0;
    unsigned int borderCol = isCleared ? YELLOW : (isSelected ? WHITE : GetColor(70, 75, 95));
    if (rank >= 2) borderCol = GetColor(128 + (g_frameCount * 5) % 128, 200, 255 - (g_frameCount * 3) % 128);
    DrawBox(x1, y1, x2, y2, borderCol, FALSE);
    if (isSelected || isCleared) DrawBox(x1 - 2, y1 - 2, x2 + 2, y2 + 2, borderCol, FALSE);

    DrawBox(illX1, illY1, illX2, illY2, GetColor(28, 34, 52), TRUE);
    DrawFace(charaIdx, illX1, illY1, illX2, illY2, false);

    if (rank >= 2) {
        DrawBox(x1 + 6, y1 + 6, x1 + 98, y1 + 24, GetColor(60, 20, 60), TRUE);
        DrawString(x1 + 10, y1 + 8, "★★LEGEND", borderCol);
    }
    else if (rank == 1) {
        DrawBox(x1 + 6, y1 + 6, x1 + 75, y1 + 24, GetColor(50, 40, 10), TRUE);
        DrawString(x1 + 10, y1 + 8, "[★CLEAR]", YELLOW);
    }
    if (noCon) DrawString(x2 - 40, y1 + 8, "ﾉｰｺﾝ", GREEN);
    if (isNew && (g_frameCount / 10) % 2 == 0) DrawString(x2 - 40, y1 + 26, "NEW!", RED);

    if (scale > 0.85f) {
        DrawFormatString(x1 + 8, y1 + (int)(146.0f * scale), WHITE, "%s", CHARA_PROFILES[charaIdx].charaId);
        DrawTierBadge(CHARA_PROFILES[charaIdx].tier, x1 + 6, y1 + (int)(168.0f * scale));
        DrawFormatString(x1 + 8, y1 + (int)(192.0f * scale), YELLOW, "HP %d", PlayableMaxHp(charaIdx));
        if (isCleared) DrawFormatString(x1 + 8, y1 + (int)(214.0f * scale), ORANGE, "『%s』", CHARA_TITLES[charaIdx][rank - 1]);
    }
}

// ----------------------------------------------------
// 戦闘シーンの描画（カメラ付き）
// ----------------------------------------------------
static void DrawBattleScene(int stageIdx, HudMode hud, bool showHitbox) {
    BeginWorldDraw();
    DrawStageBackground(stageIdx);
    DrawGimmick(stageIdx);
    DrawFighterSprite(g_other, false);
    DrawFighterSprite(g_me, true);
    DrawWorldEffects();
    if (showHitbox) DrawHitboxes();
    EndWorldDraw();

    DrawStageOverlay(stageIdx);
    DrawFighterOverlay(g_other, false);
    DrawFighterOverlay(g_me, true);
    DrawPopups();
    DrawHud(hud);
}
