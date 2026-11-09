#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")
#include "DxLib.h"

// 1つの翻訳単位としてまとめて読み込む（プロジェクト設定は main.cpp のまま）
#include "game_defs.h"
#include "game_data.h"
#include "game_combat.h"
#include "game_control.h"
#include "game_draw_world.h"
#include "game_draw_ui.h"
#include "game_modes.h"
#include "game_net.h"

static void GoTitle() {
    g_mode = MODE_TITLE;
    g_paused = false;
    g_cmdListOpen = false;
    s_keyCooldown = 15;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    ChangeWindowMode(TRUE);
    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;

    srand((unsigned int)time(NULL));
    InitMasterData();
    LoadAllSlots();
    LoadTutorialProgress();
    LoadCharaGraphs();
    g_worldScreen = MakeScreen(640, 480, FALSE);
    memset(g_keyNow, 0, sizeof(g_keyNow));
    memset(g_keyPrev, 0, sizeof(g_keyPrev));

    // 通信対戦で選べるキャラ（最初から使えるキャラのみ）
    int netCharaList[CHARA_COUNT];
    int netCharaCount = 0;
    for (int i = 0; i < ROSTER_COUNT; i++) {
        if (CHARA_PROFILES[g_rosterOrder[i]].unlockLap <= 1) netCharaList[netCharaCount++] = g_rosterOrder[i];
    }
    int trainCharaList[CHARA_COUNT];
    int trainCharaCount = 0;
    int dummyCharaList[CHARA_COUNT];
    int dummyCharaCount = 0;

    while (ProcessMessage() == 0) {
        SetDrawScreen(DX_SCREEN_BACK);
        ClearDrawScreen();
        g_frameCount++;
        memcpy(g_keyPrev, g_keyNow, sizeof(g_keyNow));
        GetHitKeyStateAll(g_keyNow);
        if (s_keyCooldown > 0) s_keyCooldown--;

        int curStage = ClampI(g_slots[g_currentSlot].stageProgress, 0, LAST_STAGE);

        switch (g_mode) {
        case MODE_TITLE: {
            static const char* items[5] = {
                "TUTORIAL (チュートリアル)", "MAIN STORY (CPU勝ち抜き)", "TRAINING (トレーニング)",
                "LAN VERSUS (ローカル通信対戦)", "EXIT"
            };
            bool tutDone = TutorialDone();
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(140, 60, "=== STREET DUEL: ARCADE CLASH ===", YELLOW);
            DrawString(190, 96, "世界格闘チャンピオンシップ", WHITE);
            if (!tutDone) {
                DrawBox(60, 130, 580, 166, GetColor(40, 30, 0), TRUE);
                DrawBox(60, 130, 580, 166, YELLOW, FALSE);
                DrawString(76, 140, "はじめての方へ：まずはチュートリアルで基本操作を覚えよう！", YELLOW);
            }
            for (int i = 0; i < 5; i++) {
                bool sel = (g_titleMenu == i);
                bool locked = !tutDone && i >= 1 && i <= 3;
                int y = 190 + i * 34;
                unsigned int col = locked ? GetColor(90, 90, 100) : (sel ? YELLOW : WHITE);
                DrawFormatString(200, y, col, "%s%s", sel ? "> " : "  ", items[i]);
                if (locked) DrawString(480, y, "[LOCK]", GetColor(140, 110, 40));
                if (i == 0) {
                    if (tutDone) DrawString(480, y, "[済]", GREEN);
                    else {
                        if ((g_frameCount / 15) % 2) DrawString(84, y, "まずはここ→", ORANGE);
                        if (g_tutorialChapters > 0) DrawFormatString(480, y, CYAN, "第%d章から", g_tutorialChapters + 1);
                    }
                }
            }
            if (g_titleMenu == 0) DrawString(110, 370, "操作を3章に分けて少しずつ学べます。クリアで全モード解放", GRAY);
            else if (!tutDone && g_titleMenu <= 3) DrawString(150, 370, "チュートリアルをクリアすると遊べます", GRAY);
            else if (g_titleMenu == 2) DrawString(150, 370, "フレーム表示・当たり判定・コンボ課題で上達しよう", GRAY);
            DrawString(180, 420, "[W/S]: 選択   [ENTER]: 決定", GRAY);

            if (g_titleLockedMsgTimer > 0) {
                g_titleLockedMsgTimer--;
                DrawBox(110, 280, 530, 324, BLACK, TRUE);
                DrawBox(110, 280, 530, 324, RED, FALSE);
                DrawString(130, 294, "まずはチュートリアルをクリアしよう！（一番上）", RED);
            }

            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP)) { g_titleMenu = (g_titleMenu + 4) % 5; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) { g_titleMenu = (g_titleMenu + 1) % 5; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 15;
                    if (!tutDone && g_titleMenu >= 1 && g_titleMenu <= 3) {
                        g_titleLockedMsgTimer = 90;
                    }
                    else if (g_titleMenu == 0) {
                        SetupTutorial(tutDone ? 0 : g_tutorialChapters);
                        g_mode = MODE_TUTORIAL;
                    }
                    else if (g_titleMenu == 1) {
                        g_is_net_battle = false;
                        g_mode = MODE_SLOT_SELECT;
                    }
                    else if (g_titleMenu == 2) {
                        g_is_net_battle = false;
                        trainCharaCount = 0;
                        for (int i = 0; i < ROSTER_COUNT; i++) {
                            if (IsCharaUnlockedAnySlot(g_rosterOrder[i])) trainCharaList[trainCharaCount++] = g_rosterOrder[i];
                        }
                        dummyCharaCount = 0;
                        for (int i = 0; i < ROSTER_COUNT; i++) dummyCharaList[dummyCharaCount++] = g_rosterOrder[i];
                        dummyCharaList[dummyCharaCount++] = 5;
                        g_trainRow = 0;
                        g_cmdListOpen = false;
                        g_mode = MODE_TRAINING_SELECT;
                    }
                    else if (g_titleMenu == 3) {
                        g_is_net_battle = true;
                        g_mode = MODE_NET_MENU;
                    }
                    else {
                        goto EXIT_GAME;
                    }
                }
            }
            break;
        }

        case MODE_SLOT_SELECT: {
            DrawBox(0, 0, 640, 480, GetColor(15, 18, 28), TRUE);
            DrawString(220, 25, "DATA SLOT SELECT", YELLOW);
            DrawString(150, 50, "プレイするデータスロットを選択してください", GRAY);
            for (int i = 0; i < 3; i++) {
                int sy = 80 + i * 110;
                DrawBox(50, sy, 590, sy + 95, GetColor(25, 30, 45), TRUE);
                DrawBox(50, sy, 590, sy + 95, (i == g_slotCursor) ? YELLOW : GRAY, FALSE);
                const SaveSlot& s = g_slots[i];
                if (s.inUse) {
                    if (s.needCharaSelect) DrawFormatString(70, sy + 12, YELLOW, "[ DATA %d ] - %d周目  キャラ選択から開始", i + 1, s.loopCount + 1);
                    else DrawFormatString(70, sy + 12, YELLOW, "[ DATA %d ] - %d周目  %s", i + 1, s.loopCount + 1, STAGES[ClampI(s.stageProgress, 0, LAST_STAGE)].name);
                    DrawFormatString(70, sy + 38, CYAN, "ファイター: %s  称号: %d個  難易度: %s",
                        CHARA_PROFILES[s.chosenChara].charaId, CountTitles(s), DIFFICULTY_NAMES[s.difficulty]);
                    float winRate = (s.totalFights > 0) ? (float)s.wins * 100.0f / (float)s.totalFights : 0.0f;
                    DrawFormatString(70, sy + 64, WHITE, "戦績: %d戦 %d勝 %d敗 %d分  (勝率 %.1f%%)", s.totalFights, s.wins, s.losses, s.draws, winRate);
                }
                else {
                    DrawFormatString(70, sy + 25, GRAY, "[ DATA %d ] - NEW GAME (データなし)", i + 1);
                    DrawString(70, sy + 55, "選択すると新規キャラクター選択からスタートします", GRAY);
                }
            }
            DrawString(70, 425, "[W/S]: データ選択   [ENTER]: 決定/再開   [DEL]: データ消去", WHITE);
            DrawString(70, 450, "[ESC]: タイトル画面へ戻る", GRAY);
            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP)) { g_slotCursor = (g_slotCursor + 2) % 3; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) { g_slotCursor = (g_slotCursor + 1) % 3; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_DELETE)) { DeleteSlot(g_slotCursor); s_keyCooldown = 15; }
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    g_currentSlot = g_slotCursor;
                    s_keyCooldown = 15;
                    if (g_slots[g_currentSlot].inUse && !g_slots[g_currentSlot].needCharaSelect) {
                        g_mode = MODE_VS_SCREEN;
                        g_phaseTimer = 0;
                    }
                    else EnterCharaSelect(false);
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) GoTitle();
            }
            break;
        }

        case MODE_CHARA_SELECT: {
            const SaveSlot& slot = g_slots[g_currentSlot];
            int lap = GetCurrentLap();
            int cur = g_rosterOrder[g_charaCursor];
            bool curLocked = !IsCharaUnlocked(cur);
            HandleCommandListKeys(cur, false, false);

            DrawBox(0, 0, 640, 480, GetColor(8, 10, 18), TRUE);
            DrawFormatString(210, 20, YELLOW, "[ DATA %d ] SELECT CARD", g_currentSlot + 1);
            DrawFormatString(470, 20, (lap > 1) ? ORANGE : GRAY, "%d周目", lap);
            DrawString(180, 45, "コレクションからファイターを選択", GRAY);

            float targetScroll = (float)g_charaCursor * 210.0f;
            g_cardScrollX += (targetScroll - g_cardScrollX) * 0.18f;
            for (int i = 0; i < ROSTER_COUNT; i++) {
                int c = g_rosterOrder[i];
                float dist = ((float)i * 210.0f) - g_cardScrollX;
                if (fabsf(dist) > 700.0f) continue;
                float scale = 1.0f - (fabsf(dist) / 550.0f);
                if (scale < 0.65f) scale = 0.65f;
                bool locked = !IsCharaUnlocked(c);
                bool isNew = !locked && CHARA_PROFILES[c].unlockLap == lap && lap > 1;
                DrawPokeCard(c, dist, scale, (i == g_charaCursor && fabsf(dist) < 25.0f), slot.clearRank[c], slot.noContinueClear[c], locked, isNew);
            }

            DrawBox(30, 340, 610, 440, GetColor(20, 24, 38), TRUE);
            DrawBox(30, 340, 610, 440, YELLOW, FALSE);
            DrawFormatString(45, 348, YELLOW, "%s", CHARA_PROFILES[cur].archetype);
            DrawTierBadge(CHARA_PROFILES[cur].tier, 460, 346);
            if (curLocked) {
                DrawString(45, 370, "NAME    : ？？？？？", GRAY);
                DrawFormatString(45, 392, RED, "LOCKED  : %d周目から使用可能（クリアして周回すると解放）", CHARA_PROFILES[cur].unlockLap);
                DrawString(45, 414, "HINT    : 操作は難しいが、使いこなせば最強クラス…？", GRAY);
            }
            else {
                DrawFormatString(45, 370, WHITE, "NAME    : %s", CHARA_PROFILES[cur].charaId);
                DrawFormatString(45, 392, CYAN, "SKILL   : %s", CHARA_PROFILES[cur].specialDesc);
                if (slot.clearRank[cur] > 0) {
                    DrawFormatString(45, 414, ORANGE, "称号    : 『%s』 クリア%d回%s", CHARA_TITLES[cur][slot.clearRank[cur] - 1],
                        slot.clearCount[cur], slot.noContinueClear[cur] ? " / ノーコン達成" : "");
                }
                else DrawString(45, 414, "称号    : まだなし（クリアで獲得）", GRAY);
            }
            DrawString(70, 455, "[A/D]: カードをめくる  [ENTER]: 決定  [TAB]: 技表  [ESC]: 戻る", WHITE);

            if (g_unlockBannerTimer > 0) {
                g_unlockBannerTimer--;
                DrawBox(60, 64, 580, 92, GetColor(40, 0, 0), TRUE);
                DrawBox(60, 64, 580, 92, (g_frameCount / 6) % 2 ? YELLOW : RED, FALSE);
                DrawFormatString(80, 70, YELLOW, "NEW CHALLENGER!!  『%s』が参戦可能になった！", CHARA_PROFILES[g_unlockBannerChara].charaId);
            }
            if (g_lockedMsgTimer > 0) {
                g_lockedMsgTimer--;
                DrawBox(150, 190, 490, 230, BLACK, TRUE);
                DrawBox(150, 190, 490, 230, RED, FALSE);
                DrawFormatString(170, 202, RED, "まだ使えない！ %d周目で解放されます", CHARA_PROFILES[cur].unlockLap);
            }
            if (g_cmdListOpen) {
                if (curLocked) { g_cmdListOpen = false; g_lockedMsgTimer = 70; }
                else DrawCommandList(false);
                break;
            }

            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_A) || CheckHitKey(KEY_INPUT_LEFT)) { if (g_charaCursor > 0) g_charaCursor--; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_D) || CheckHitKey(KEY_INPUT_RIGHT)) { if (g_charaCursor < ROSTER_COUNT - 1) g_charaCursor++; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 20;
                    if (curLocked) g_lockedMsgTimer = 70;
                    else {
                        SaveSlot& s = g_slots[g_currentSlot];
                        s.inUse = true;
                        s.chosenChara = cur;
                        s.stageProgress = 0;
                        s.totalFights = s.wins = s.losses = s.draws = 0;
                        s.hasContinued = false;
                        s.hadTimeOver = false;
                        s.needCharaSelect = false;
                        SaveCurrentSlot();
                        g_mode = MODE_STORY_INTRO;
                        g_phaseTimer = 0;
                    }
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) { g_mode = MODE_SLOT_SELECT; s_keyCooldown = 15; }
            }
            break;
        }

        case MODE_STORY_INTRO: {
            g_phaseTimer++;
            SaveSlot& s = g_slots[g_currentSlot];
            int c = s.chosenChara;
            DrawBox(0, 0, 640, 480, GetColor(10, 8, 20), TRUE);
            DrawString(260, 30, "- STORY -", YELLOW);
            DrawBox(40, 60, 160, 180, GetColor(28, 34, 52), TRUE);
            DrawFace(c, 44, 64, 156, 176, false);
            DrawFormatString(180, 80, WHITE, "%s", CHARA_PROFILES[c].charaId);
            DrawFormatString(180, 104, ORANGE, "%s", CHARA_PROFILES[c].archetype);
            DrawTierBadge(CHARA_PROFILES[c].tier, 180, 130);
            DrawBox(40, 196, 600, 310, GetColor(20, 20, 35), TRUE);
            DrawBox(40, 196, 600, 310, GRAY, FALSE);
            for (int i = 0; i < 3; i++) {
                if (g_phaseTimer > 20 + i * 40) DrawString(60, 212 + i * 32, STORY_PROLOGUE[c][i], WHITE);
            }
            // 難易度の選択
            DrawString(60, 330, "難易度を選んでください（A/D）", YELLOW);
            static const char* diffDesc[3] = {
                "敵の攻撃力・体力が低め。コンボやハメは使ってこない",
                "標準。1周目の敵はコンボを使わない。周回で強くなる",
                "最初から敵がコンボ・投げ抜け・バーストを使う"
            };
            for (int i = 0; i < 3; i++) {
                bool sel = (s.difficulty == i);
                DrawBox(60 + i * 180, 354, 220 + i * 180, 384, sel ? GetColor(60, 50, 10) : GetColor(25, 25, 35), TRUE);
                DrawBox(60 + i * 180, 354, 220 + i * 180, 384, sel ? YELLOW : GRAY, FALSE);
                DrawString(100 + i * 180, 361, DIFFICULTY_NAMES[i], sel ? YELLOW : WHITE);
            }
            DrawString(60, 394, diffDesc[s.difficulty], CYAN);
            if (GetCurrentLap() > 1) DrawFormatString(60, 414, ORANGE, "%d周目：周回ボーナスで敵がさらに手強くなっている…！", GetCurrentLap());
            DrawString(220, 446, "[Enter]: 戦いへ向かう", (g_frameCount / 20) % 2 ? WHITE : GRAY);

            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_A) || CheckHitKey(KEY_INPUT_LEFT)) { s.difficulty = ClampI(s.difficulty - 1, 0, 2); s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_D) || CheckHitKey(KEY_INPUT_RIGHT)) { s.difficulty = ClampI(s.difficulty + 1, 0, 2); s_keyCooldown = 12; }
                if (g_phaseTimer > 20 && CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 20;
                    SaveCurrentSlot();
                    g_mode = MODE_VS_SCREEN;
                    g_phaseTimer = 0;
                }
            }
            break;
        }

        case MODE_VS_SCREEN: {
            g_phaseTimer++;
            const SaveSlot& s = g_slots[g_currentSlot];
            int enemy = CPU_ROSTER[curStage].charaIndex;
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(240, 40, "BATTLE MATCH-UP", YELLOW);
            DrawFormatString(160, 70, CYAN, "%s", STAGES[curStage].name);
            DrawFormatString(200, 96, GRAY, "%d周目 / 難易度：%s", GetCurrentLap(), DIFFICULTY_NAMES[s.difficulty]);

            DrawBox(40, 130, 280, 370, GetColor(25, 30, 50), TRUE);
            DrawBox(40, 130, 280, 370, CYAN, FALSE);
            DrawFace(s.chosenChara, 90, 140, 230, 280, false);
            DrawFormatString(60, 290, WHITE, "DATA %d", g_currentSlot + 1);
            DrawFormatString(60, 314, YELLOW, "%s", CHARA_PROFILES[s.chosenChara].charaId);
            if (s.clearRank[s.chosenChara] > 0) DrawFormatString(60, 338, ORANGE, "『%s』", CHARA_TITLES[s.chosenChara][s.clearRank[s.chosenChara] - 1]);

            DrawExtendString(298, 230, 2.0, 2.0, "VS", RED);

            DrawBox(360, 130, 600, 370, GetColor(50, 25, 30), TRUE);
            DrawBox(360, 130, 600, 370, RED, FALSE);
            DrawFace(enemy, 410, 140, 550, 280, false);
            DrawFormatString(375, 290, WHITE, "%s", CPU_ROSTER[curStage].name);
            DrawFormatString(375, 314, YELLOW, "%s", CPU_ROSTER[curStage].alias);
            if (curStage == 1) DrawString(130, 390, "注意: 床に電流が流れる。予告の点滅を見て回避しよう", CYAN);

            if (g_phaseTimer > 120 || (CheckHitKey(KEY_INPUT_RETURN) && s_keyCooldown == 0 && g_phaseTimer > 20)) {
                SetupBattleRound();
                g_mode = MODE_BATTLE_PLAY;
            }
            break;
        }

        case MODE_BATTLE_PLAY: {
            int sel = HandlePauseMenu("キャラクター選択に戻る", "タイトルに戻る");
            if (sel == 1) { EnterCharaSelect(false); break; }
            if (sel == 2) { GoTitle(); break; }
            if (!g_paused) HandleCommandListKeys(g_me.charaIndex, false, true);

            if (!g_paused && !g_cmdListOpen) {
                if (g_readyFightTimer < 80) {
                    g_readyFightTimer++;
                    if (g_readyFightTimer == 50) TriggerScreenShake(8);
                }
                else if (++g_battleTimerFrames >= 60) {
                    g_battleTimerFrames = 0;
                    if (g_roundTime > 0) g_roundTime--;
                }
                UpdateControlledFighter(g_me, g_other, g_ctrl[0], ReadLocalBits());
                UpdateCpu();
                ResolvePushboxes();
                UpdateTraps();
                UpdateStageGimmick(curStage);
                UpdateShots();
                UpdateEffects();
                UpdateCamera(false);
            }

            DrawBattleScene(curStage, HUD_STORY, false);
            if (g_cmdListOpen) { DrawCommandList(false); break; }
            DrawPauseMenu("キャラクター選択に戻る", "タイトルに戻る");
            if (g_paused) break;

            if (g_me.hp <= 0 || g_other.hp <= 0) {
                g_roundWon = (g_me.hp > 0);
                g_koWinner = g_roundWon ? 1 : 2;
                g_phaseTimer = 0;
                TriggerScreenShake(g_isKokusenKoFinish ? 25 : 16);
                g_slots[g_currentSlot].totalFights++;
                if (g_roundWon) g_slots[g_currentSlot].wins++;
                else g_slots[g_currentSlot].losses++;
                SaveCurrentSlot();
                g_mode = MODE_KO_DISPLAY;
            }
            else if (g_roundTime <= 0) {
                SaveSlot& s = g_slots[g_currentSlot];
                s.hadTimeOver = true;
                float myR = (float)g_me.hp / (float)g_me.maxHp;
                float othR = (float)g_other.hp / (float)g_other.maxHp;
                s.totalFights++;
                if (fabsf(myR - othR) < 0.001f) { g_isDraw = true; g_roundWon = false; s.draws++; }
                else if (myR > othR) { g_isDraw = false; g_roundWon = true; s.wins++; }
                else { g_isDraw = false; g_roundWon = false; s.losses++; }
                g_koWinner = g_isDraw ? 0 : (g_roundWon ? 1 : 2);
                SaveCurrentSlot();
                g_phaseTimer = 0;
                TriggerScreenShake(12);
                g_mode = MODE_TIME_OVER;
            }
            break;
        }

        case MODE_TIME_OVER:
        case MODE_KO_DISPLAY: {
            g_phaseTimer++;
            // 勝負がついた後も少しだけ物理を動かして、倒れる・勝利ポーズを見せる
            UpdateFighterCommon(g_me, g_other, false);
            UpdateFighterCommon(g_other, g_me, false);
            UpdateEffects();
            UpdateCamera(false);
            DrawBattleScene(curStage, HUD_STORY, false);
            bool ko = (g_mode == MODE_KO_DISPLAY);
            if (ko && g_isKokusenKoFinish) {
                for (int b = 0; b < 8; b++) {
                    int rcol = GetColor((rand() % 155) + 100, (rand() % 155) + 100, (rand() % 155) + 100);
                    DrawBox(100 + b, 140 + b, 540 - b, 300 - b, rcol, FALSE);
                }
                DrawBox(108, 148, 532, 292, GetColor(0, 0, 0), TRUE);
                DrawString(160, 170, "★☆★ 大 激 闘 完 全 制 覇 ★☆★", (rand() % 2 == 0 ? YELLOW : ORANGE));
                DrawString(170, 210, "★ 極 限 神 撃  K . O . ★", (rand() % 2 == 0 ? WHITE : YELLOW));
                DrawString(200, 255, "【超 絶 大 当 り 勝 利 !!】", RED);
            }
            else {
                DrawBox(180, 180, 460, 260, GetColor(0, 0, 0), TRUE);
                DrawBox(180, 180, 460, 260, g_roundWon ? YELLOW : RED, FALSE);
                if (ko) DrawExtendString(270, 200, 2.0, 2.0, "K. O.", YELLOW);
                else DrawString(250, 210, g_isDraw ? "DRAW  GAME" : "TIME  OVER !!", g_isDraw ? GRAY : (g_roundWon ? YELLOW : RED));
            }
            if (g_phaseTimer > ((ko && g_isKokusenKoFinish) ? 160 : 120)) {
                g_phaseTimer = 0;
                g_koWinner = 0;
                if (g_roundWon) {
                    g_slots[g_currentSlot].stageProgress++;
                    SaveCurrentSlot();
                    g_mode = MODE_WIN_QUOTE;
                }
                else g_mode = MODE_GAME_OVER;
            }
            break;
        }

        case MODE_WIN_QUOTE: {
            g_phaseTimer++;
            SaveSlot& s = g_slots[g_currentSlot];
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            int prevStage = ClampI(s.stageProgress - 1, 0, LAST_STAGE);
            bool secretNext = (s.stageProgress == LAST_STAGE && !s.hasContinued && !s.hadTimeOver);
            DrawString(220, 50, "=== VICTORY! ===", YELLOW);
            if (g_poseGraph[s.chosenChara][PO_WIN] != -1) DrawExtendGraph(470, 30, 470 + 144, 30 + 120, g_poseGraph[s.chosenChara][PO_WIN], TRUE);
            DrawFormatString(60, 100, WHITE, "敗者 %s:", CPU_ROSTER[prevStage].name);
            DrawFormatString(60, 122, WHITE, "「%s」", CPU_ROSTER[prevStage].winQuote);
            float winRate = (s.totalFights > 0) ? (float)s.wins * 100.0f / (float)s.totalFights : 0.0f;
            DrawFormatString(60, 156, CYAN, "通算戦績: %d戦 %d勝 %d敗 %d分 (勝率: %.1f%%)", s.totalFights, s.wins, s.losses, s.draws, winRate);
            DrawBox(50, 185, 590, 265, GetColor(20, 24, 38), TRUE);
            DrawBox(50, 185, 590, 265, GRAY, FALSE);
            DrawString(60, 190, "この試合の記録", ORANGE);
            DrawStatsBlock(60, 210, 0, g_me.charaIndex, WHITE);
            if (secretNext) DrawString(60, 290, "……ただならぬ気配！ 何者かが乱入してくる！", RED);
            else if (s.stageProgress >= LAST_STAGE) DrawString(60, 290, "全ての強敵を打ち倒した！", GREEN);
            else DrawString(60, 290, "次の戦場へ向かう……", GREEN);
            DrawString(180, 380, "[Enter]: 次へ進む", WHITE);

            if (CheckHitKey(KEY_INPUT_RETURN) && g_phaseTimer > 40 && s_keyCooldown == 0) {
                s_keyCooldown = 20;
                g_phaseTimer = 0;
                if (s.stageProgress == LAST_STAGE) {
                    if (secretNext) g_mode = MODE_VS_SCREEN;
                    else { OnRunCleared(false); g_mode = MODE_ENDING; }
                }
                else if (s.stageProgress > LAST_STAGE) { OnRunCleared(true); g_mode = MODE_ENDING; }
                else g_mode = MODE_VS_SCREEN;
            }
            break;
        }

        case MODE_GAME_OVER: {
            DrawBox(0, 0, 640, 480, GetColor(10, 0, 0), TRUE);
            DrawString(240, 90, "=== DEFEAT... ===", RED);
            DrawString(220, 130, "CONTINUE CHALLENGE?", WHITE);
            DrawBox(50, 170, 590, 250, GetColor(25, 10, 10), TRUE);
            DrawBox(50, 170, 590, 250, GRAY, FALSE);
            DrawString(60, 175, "この試合の記録", ORANGE);
            DrawStatsBlock(60, 195, 0, g_me.charaIndex, WHITE);
            DrawString(200, 290, "[Enter]: このステージから再挑戦", GREEN);
            DrawString(200, 320, "[Esc]: スロット選択画面へ戻る", GRAY);
            DrawString(90, 370, "ヒント: 難しいと感じたら、新しく始めるときに「かんたん」を選べます", GRAY);
            DrawString(90, 392, "        TRAINING ではコンボやフレームを練習できます", GRAY);
            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 20;
                    g_slots[g_currentSlot].hasContinued = true;
                    SaveCurrentSlot();
                    g_mode = MODE_VS_SCREEN;
                    g_phaseTimer = 0;
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) { s_keyCooldown = 20; g_mode = MODE_SLOT_SELECT; }
            }
            break;
        }

        case MODE_ENDING: {
            g_phaseTimer++;
            const SaveSlot& s = g_slots[g_currentSlot];
            int c = s.chosenChara;
            DrawBox(0, 0, 640, 480, GetColor(20, 25, 40), TRUE);
            if (g_endingSecret) {
                DrawExtendString(236, 20, 3.0, 3.0, "CLEAR!!", ((g_phaseTimer / 6) % 2) ? YELLOW : ORANGE);
                DrawString(150, 76, "【伝説達成】裏ボス・真 天輪太子を撃破した！", RED);
            }
            else {
                DrawExtendString(184, 24, 2.0, 2.0, "FINAL STAGE CLEAR", YELLOW);
                DrawString(90, 76, "※ノーコンティニュー＆タイムオーバー無しで… 何かが現れる？", GRAY);
            }
            DrawBox(40, 100, 600, 220, GetColor(10, 12, 24), TRUE);
            DrawBox(40, 100, 600, 220, GRAY, FALSE);
            DrawFormatString(52, 104, ORANGE, "- %s ENDING -", CHARA_PROFILES[c].charaId);
            for (int i = 0; i < 3; i++) {
                if (g_phaseTimer > 30 + i * 50) DrawString(52, 128 + i * 22, STORY_ENDING[c][i], WHITE);
            }
            if (g_endingSecret && g_phaseTimer > 180) DrawString(52, 196, STORY_ENDING_SECRET[c], YELLOW);
            DrawFormatString(60, 232, YELLOW, "称号『%s』を獲得！", CHARA_TITLES[c][g_endingRank - 1]);
            float winRate = (s.totalFights > 0) ? (float)s.wins * 100.0f / (float)s.totalFights : 0.0f;
            DrawFormatString(60, 256, CYAN, "最終戦績: %d戦 %d勝 %d敗 %d分 (勝率: %.1f%%)", s.totalFights, s.wins, s.losses, s.draws, winRate);
            DrawFormatString(60, 280, ORANGE, "次は %d周目：周回ボーナスで敵がさらに強くなります", GetCurrentLap());
            if (g_endingUnlockChara >= 0 && (g_phaseTimer / 10) % 3 != 0) {
                DrawFormatString(60, 312, CYAN, "NEW CHALLENGER!! 次の周回から『%s』が使用可能に！", CHARA_PROFILES[g_endingUnlockChara].charaId);
            }
            DrawString(170, 380, "[Enter]: キャラクター選択へ (周回プレイ)", WHITE);
            DrawString(170, 405, "[Esc]: タイトルへ戻る", GRAY);
            if (s_keyCooldown == 0 && g_phaseTimer > 60) {
                if (CheckHitKey(KEY_INPUT_RETURN)) { s_keyCooldown = 20; EnterCharaSelect(true); }
                else if (CheckHitKey(KEY_INPUT_ESCAPE)) GoTitle();
            }
            break;
        }

        case MODE_TUTORIAL: {
            int sel = (g_tut.phase == TP_PLAY || g_tut.phase == TP_INTRO) ? HandlePauseMenu("タイトルに戻る（章ごとに保存）", NULL) : -1;
            if (sel == 1) { GoTitle(); break; }
            if (!g_paused && g_tut.phase == TP_PLAY) HandleCommandListKeys(g_me.charaIndex, false, true);
            if (!g_paused && !g_cmdListOpen) UpdateTutorialFrame();

            DrawBattleScene(0, HUD_TUTORIAL, false);
            DrawTutorialOverlay();
            if (g_cmdListOpen) { DrawCommandList(false); break; }
            DrawPauseMenu("タイトルに戻る（章ごとに保存）", NULL);
            if (g_paused) break;

            if (g_tut.phase == TP_CHAPTER_CLEAR && g_tut.timer > 30) {
                if (KeyHit(KEY_INPUT_RETURN)) SetupTutorial(g_tut.chapter + 1);
                else if (KeyHit(KEY_INPUT_ESCAPE)) GoTitle();
            }
            else if (g_tut.phase == TP_ALL_CLEAR && g_tut.timer > 30) {
                if (KeyHit(KEY_INPUT_RETURN) || KeyHit(KEY_INPUT_ESCAPE)) { GoTitle(); g_titleMenu = 1; }
            }
            break;
        }

        case MODE_TRAINING_SELECT: {
            DrawBox(0, 0, 640, 480, GetColor(8, 14, 20), TRUE);
            DrawString(230, 24, "=== TRAINING MODE ===", YELLOW);
            DrawString(130, 48, "反撃してこないサンドバッグ相手に自由に練習できます", GRAY);
            int myPos = 0, dmPos = 0;
            for (int i = 0; i < trainCharaCount; i++) if (trainCharaList[i] == g_trainMyChara) myPos = i;
            for (int i = 0; i < dummyCharaCount; i++) if (dummyCharaList[i] == g_trainDummyChara) dmPos = i;
            g_trainMyChara = trainCharaList[myPos];
            g_trainDummyChara = dummyCharaList[dmPos];

            DrawBox(80, 76, 180, 176, GetColor(28, 34, 52), TRUE);
            DrawFace(g_trainMyChara, 82, 78, 178, 174, false);
            DrawBox(460, 76, 560, 176, GetColor(52, 28, 34), TRUE);
            DrawFace(g_trainDummyChara, 462, 78, 558, 174, false);
            DrawTierBadge(CHARA_PROFILES[g_trainMyChara].tier, 190, 150);

            const char* rowName[4] = { "自分のキャラ", "ダミーのキャラ", "ステージ", "トレーニング開始" };
            for (int r = 0; r < 4; r++) {
                int y = 194 + r * 50;
                bool sel = (g_trainRow == r);
                DrawBox(80, y, 560, y + 40, sel ? GetColor(40, 40, 20) : GetColor(20, 24, 34), TRUE);
                DrawBox(80, y, 560, y + 40, sel ? YELLOW : GRAY, FALSE);
                DrawString(96, y + 12, rowName[r], sel ? YELLOW : WHITE);
                if (r == 0) DrawFormatString(260, y + 12, WHITE, "< %s >", CHARA_PROFILES[g_trainMyChara].charaId);
                if (r == 1) DrawFormatString(260, y + 12, WHITE, "< %s >", CHARA_PROFILES[g_trainDummyChara].charaId);
                if (r == 2) DrawFormatString(260, y + 12, WHITE, "< %s >", STAGES[g_trainStage].name);
            }
            DrawString(70, 400, "※未解放キャラは、どれかのセーブデータで解放すると使えます", GRAY);
            DrawString(60, 430, "[W/S]: 項目  [A/D]: 変更  [ENTER]: 開始  [TAB]: 技表  [ESC]: 戻る", WHITE);

            HandleCommandListKeys(g_trainMyChara, true, true);
            if (g_cmdListOpen) { DrawCommandList(true); break; }
            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP)) { g_trainRow = (g_trainRow + 3) % 4; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) { g_trainRow = (g_trainRow + 1) % 4; s_keyCooldown = 12; }
                int delta = 0;
                if (CheckHitKey(KEY_INPUT_A) || CheckHitKey(KEY_INPUT_LEFT)) delta = -1;
                if (CheckHitKey(KEY_INPUT_D) || CheckHitKey(KEY_INPUT_RIGHT)) delta = 1;
                if (delta != 0) {
                    s_keyCooldown = 12;
                    if (g_trainRow == 0) g_trainMyChara = trainCharaList[(myPos + delta + trainCharaCount) % trainCharaCount];
                    if (g_trainRow == 1) g_trainDummyChara = dummyCharaList[(dmPos + delta + dummyCharaCount) % dummyCharaCount];
                    if (g_trainRow == 2) g_trainStage = (g_trainStage + delta + 6) % 6;
                }
                if (CheckHitKey(KEY_INPUT_RETURN)) { s_keyCooldown = 15; SetupTraining(); g_mode = MODE_TRAINING; }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) GoTitle();
            }
            break;
        }

        case MODE_TRAINING: {
            int sel = HandlePauseMenu("キャラ・ステージ選択に戻る", "タイトルに戻る");
            if (sel == 1) { g_mode = MODE_TRAINING_SELECT; s_keyCooldown = 15; break; }
            if (sel == 2) { GoTitle(); break; }
            if (!g_paused) HandleCommandListKeys(g_me.charaIndex, true, true);
            if (!g_paused && !g_cmdListOpen) {
                HandleTrainingKeys();
                UpdateTrainingFrame();
            }
            DrawBattleScene(g_currentStageIdx, HUD_TRAINING, g_showHitbox);
            DrawTrainingOverlay();
            if (g_cmdListOpen) DrawCommandList(true);
            DrawPauseMenu("キャラ・ステージ選択に戻る", "タイトルに戻る");
            break;
        }

        case MODE_NET_MENU: {
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(200, 80, "=== LAN VERSUS MENU ===", YELLOW);
            DrawString(210, 180, (g_netMenu == 0) ? "> HOST 作成 (部屋を開く)" : "  HOST 作成 (部屋を開く)", (g_netMenu == 0) ? YELLOW : WHITE);
            DrawString(210, 220, (g_netMenu == 1) ? "> CLIENT 参加 (IP接続)" : "  CLIENT 参加 (IP接続)", (g_netMenu == 1) ? YELLOW : WHITE);
            DrawString(120, 300, "ホストが当たり判定・HP・乱数をすべて計算するため、", GRAY);
            DrawString(120, 320, "遅延があっても両者の画面の結果は食い違いません", GRAY);
            DrawString(190, 380, "[W/S]: 選択   [ENTER]: 決定   [ESC]: 戻る", GRAY);
            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP)) { g_netMenu = 0; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) { g_netMenu = 1; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 15;
                    if (g_netMenu == 0) StartHost();
                    else g_mode = MODE_CLIENT_INPUT;
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) { g_is_net_battle = false; GoTitle(); }
            }
            break;
        }

        case MODE_HOST_WAIT: {
            UpdateHostWait();
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(50, 50, "[ 対戦者待機中 ]", GREEN);
            DrawFormatString(70, 100, WHITE, "部屋番号: %04d", g_net_room_code);
            DrawString(70, 130, "相手PCからの接続を待機しています...", GRAY);
            DrawString(70, 380, "[Esc]: メニューへ戻る", GRAY);
            if (CheckHitKey(KEY_INPUT_ESCAPE)) { CloseSocket(g_listen); g_mode = MODE_NET_MENU; s_keyCooldown = 15; }
            break;
        }

        case MODE_CLIENT_INPUT: {
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(50, 50, "[ 部屋へ参加 ]", YELLOW);
            DrawString(70, 90, "接続先ホストの IPv4 アドレスを入力してください:", WHITE);
            DrawBox(70, 150, 460, 190, GetColor(25, 30, 45), TRUE);
            DrawBox(70, 150, 460, 190, WHITE, FALSE);
            HandleTextInput(g_client_ip_input, 32, true);
            DrawFormatString(85, 162, WHITE, "> %s_", g_client_ip_input);
            DrawString(70, 230, "[Enter] 接続開始", GREEN);
            DrawString(70, 260, "[Esc] メニューに戻る", GRAY);
            if (CheckHitKey(KEY_INPUT_RETURN)) { StartClient(g_client_ip_input); s_keyCooldown = 15; }
            if (CheckHitKey(KEY_INPUT_ESCAPE)) { g_mode = MODE_NET_MENU; s_keyCooldown = 15; }
            break;
        }

        case MODE_NET_STAGE_SELECT: {
            ReceivePackets();
            if (g_mode != MODE_NET_STAGE_SELECT) break;
            static const int timeOptions[3] = { 60, 99, 0 };
            DrawBox(0, 0, 640, 480, GetColor(10, 8, 20), TRUE);
            DrawString(220, 20, "STAGE & RULE SELECT", PURPLE);
            DrawBox(40, 50, 600, 190, STAGES[g_netRules.stage].skyColor, TRUE);
            DrawBox(40, 50, 600, 190, WHITE, FALSE);
            DrawFormatString(60, 80, YELLOW, "%s", STAGES[g_netRules.stage].name);
            const char* rowName[4] = { "ステージ", "勝利条件", "制限時間", "ギミック・アイテム" };
            char rowVal[4][48];
            sprintf_s(rowVal[0], "STAGE %d", g_netRules.stage + 1);
            sprintf_s(rowVal[1], "%d本先取", g_netRules.roundsToWin);
            if (g_netRules.timeLimit == 0) sprintf_s(rowVal[2], "無制限");
            else sprintf_s(rowVal[2], "%d秒", g_netRules.timeLimit);
            sprintf_s(rowVal[3], "%s", g_netRules.gimmickOn ? "あり" : "なし");
            for (int r = 0; r < 4; r++) {
                int y = 210 + r * 40;
                bool sel = g_is_host && (g_netRuleRow == r);
                DrawBox(80, y, 560, y + 32, sel ? GetColor(40, 40, 20) : GetColor(20, 24, 34), TRUE);
                DrawBox(80, y, 560, y + 32, sel ? YELLOW : GRAY, FALSE);
                DrawString(96, y + 8, rowName[r], sel ? YELLOW : WHITE);
                DrawFormatString(300, y + 8, WHITE, "< %s >", rowVal[r]);
            }
            if (g_is_host) {
                DrawString(120, 400, "W/S: 項目  A/D: 変更  Enter: 決定してキャラ選択へ", WHITE);
                if (s_keyCooldown == 0) {
                    if (CheckHitKey(KEY_INPUT_W)) { g_netRuleRow = (g_netRuleRow + 3) % 4; s_keyCooldown = 12; }
                    if (CheckHitKey(KEY_INPUT_S)) { g_netRuleRow = (g_netRuleRow + 1) % 4; s_keyCooldown = 12; }
                    int delta = CheckHitKey(KEY_INPUT_A) ? -1 : (CheckHitKey(KEY_INPUT_D) ? 1 : 0);
                    if (delta != 0) {
                        s_keyCooldown = 12;
                        if (g_netRuleRow == 0) g_netRules.stage = (unsigned char)((g_netRules.stage + delta + 5) % 5);
                        if (g_netRuleRow == 1) g_netRules.roundsToWin = (unsigned char)((g_netRules.roundsToWin - 1 + delta + 3) % 3 + 1);
                        if (g_netRuleRow == 2) {
                            int idx = 0;
                            for (int i = 0; i < 3; i++) if (timeOptions[i] == g_netRules.timeLimit) idx = i;
                            g_netRules.timeLimit = (unsigned char)timeOptions[(idx + delta + 3) % 3];
                        }
                        if (g_netRuleRow == 3) g_netRules.gimmickOn = g_netRules.gimmickOn ? 0 : 1;
                    }
                    if (CheckHitKey(KEY_INPUT_RETURN)) {
                        g_myNetReady = false;
                        g_enemyNetReady = false;
                        SendPacket(PKT_SETUP, &g_netRules, sizeof(g_netRules));
                        g_mode = MODE_NET_CHARA_SELECT;
                        s_keyCooldown = 15;
                    }
                }
            }
            else DrawString(180, 400, "ホストがステージとルールを選んでいます...", YELLOW);
            if (CheckHitKey(KEY_INPUT_ESCAPE)) { CloseSocket(g_peer); g_is_net_battle = false; GoTitle(); }
            break;
        }

        case MODE_NET_CHARA_SELECT: {
            ReceivePackets();
            if (g_mode != MODE_NET_CHARA_SELECT) break;
            DrawBox(0, 0, 640, 480, GetColor(14, 16, 26), TRUE);
            DrawString(225, 18, "CHARACTER SELECT", YELLOW);
            g_netCharaCursor = ClampI(g_netCharaCursor, 0, netCharaCount - 1);
            if (!g_myNetReady) g_myNetChara = netCharaList[g_netCharaCursor];

            DrawBox(30, 45, 290, 275, GetColor(25, 30, 45), TRUE);
            DrawBox(30, 45, 290, 275, g_myNetReady ? GREEN : RED, FALSE);
            DrawString(45, 55, "1P: YOU", g_myNetReady ? GREEN : RED);
            DrawFace(g_myNetChara, 100, 80, 220, 200, false);
            DrawFormatString(45, 214, WHITE, "%s", CHARA_PROFILES[g_myNetChara].charaId);
            DrawTierBadge(CHARA_PROFILES[g_myNetChara].tier, 45, 240);

            DrawBox(350, 45, 610, 275, GetColor(35, 20, 25), TRUE);
            DrawBox(350, 45, 610, 275, g_enemyNetReady ? GREEN : BLUE, FALSE);
            DrawString(365, 55, "2P: REMOTE", g_enemyNetReady ? GREEN : BLUE);
            if (g_enemyNetReady) {
                DrawFace(g_enemyNetChara, 420, 80, 540, 200, false);
                DrawFormatString(365, 214, WHITE, "%s", CHARA_PROFILES[g_enemyNetChara].charaId);
            }
            else DrawString(365, 214, "選択中...", GRAY);

            for (int i = 0; i < netCharaCount; i++) {
                int cx = 22 + i * 100, cy = 290;
                int c = netCharaList[i];
                DrawBox(cx, cy, cx + 94, cy + 100, GetColor(20, 25, 35), TRUE);
                DrawBox(cx, cy, cx + 94, cy + 100, (i == g_netCharaCursor) ? YELLOW : GRAY, FALSE);
                DrawFace(c, cx + 15, cy + 6, cx + 79, cy + 70, false);
                DrawFormatString(cx + 4, cy + 76, WHITE, "%s", CHARA_PROFILES[c].charaId);
            }

            HandleCommandListKeys(g_myNetChara, false, true);
            if (g_cmdListOpen) DrawCommandList(false);
            else if (!g_myNetReady) {
                DrawString(120, 420, "A / D: キャラ選択   Enter: 決定   TAB: 技表", WHITE);
                if (s_keyCooldown == 0) {
                    if (CheckHitKey(KEY_INPUT_A)) { g_netCharaCursor = (g_netCharaCursor + netCharaCount - 1) % netCharaCount; s_keyCooldown = 12; }
                    if (CheckHitKey(KEY_INPUT_D)) { g_netCharaCursor = (g_netCharaCursor + 1) % netCharaCount; s_keyCooldown = 12; }
                    if (CheckHitKey(KEY_INPUT_RETURN)) {
                        g_myNetChara = netCharaList[g_netCharaCursor];
                        g_myNetReady = true;
                        NetCharaMsg msg = { (unsigned char)g_myNetChara };
                        SendPacket(PKT_CHARA, &msg, sizeof(msg));
                        s_keyCooldown = 15;
                    }
                }
            }
            else DrawString(200, 420, "相手の決定を待機中...", YELLOW);

            if (g_myNetReady && g_enemyNetReady) {
                g_cmdListOpen = false;
                SetupNetMatch();
                g_mode = MODE_NET_BATTLE_PLAY;
            }
            break;
        }

        case MODE_NET_BATTLE_PLAY: {
            // 通信中は時間を止められないので、ポーズは「表示だけ」
            int sel = HandlePauseMenu("切断してタイトルへ", NULL);
            if (sel == 1) { CloseSocket(g_peer); g_is_net_battle = false; GoTitle(); break; }
            if (!g_paused) HandleCommandListKeys(g_me.charaIndex, false, false);
            if (g_is_host) {
                ReceivePackets();
                if (g_mode != MODE_NET_BATTLE_PLAY) break;
                NetHostUpdate();
            }
            else NetClientUpdate();
            FlushSend();
            if (g_mode == MODE_NET_ERROR) break;
            UpdateCamera(false);

            DrawBattleScene(g_currentStageIdx, HUD_NET, false);
            DrawNetPhaseBanner();
            if (!g_is_host && !g_netGotSnapshot) {
                DrawBox(200, 300, 440, 330, BLACK, TRUE);
                DrawString(230, 307, "ホストと同期中...", YELLOW);
            }
            if (g_cmdListOpen) DrawCommandList(false);
            DrawPauseMenu("切断してタイトルへ", NULL);
            break;
        }

        case MODE_NET_RESULT: {
            DrawBox(0, 0, 640, 480, GetColor(10, 10, 15), TRUE);
            if (g_netWinner == 3) DrawString(250, 40, "=== DRAW GAME ===", GRAY);
            else if (g_roundWon) DrawString(240, 40, "=== YOU WIN! ===", YELLOW);
            else DrawString(240, 40, "=== YOU LOSE... ===", RED);
            int myIdx = g_is_host ? 0 : 1;
            DrawFormatString(250, 70, WHITE, "ラウンド  %d - %d", g_netRounds[myIdx], g_netRounds[1 - myIdx]);
            DrawBox(30, 100, 610, 190, GetColor(20, 24, 38), TRUE);
            DrawBox(30, 100, 610, 190, CYAN, FALSE);
            DrawFormatString(40, 106, CYAN, "YOU: %s", CHARA_PROFILES[g_me.charaIndex].charaId);
            DrawStatsBlock(40, 126, 0, g_me.charaIndex, WHITE);
            DrawBox(30, 200, 610, 290, GetColor(38, 20, 24), TRUE);
            DrawBox(30, 200, 610, 290, RED, FALSE);
            DrawFormatString(40, 206, RED, "REMOTE: %s", CHARA_PROFILES[g_other.charaIndex].charaId);
            DrawStatsBlock(40, 226, 1, g_other.charaIndex, WHITE);
            DrawString(190, 330, "[Enter] 再戦 (ステージ選択へ)", GREEN);
            DrawString(190, 360, "[ESC] メニューへ戻る", GRAY);
            if (s_keyCooldown == 0 && CheckHitKey(KEY_INPUT_RETURN)) {
                g_myNetReady = false;
                g_enemyNetReady = false;
                g_netRuleRow = 0;
                g_mode = MODE_NET_STAGE_SELECT;
                s_keyCooldown = 15;
            }
            if (s_keyCooldown == 0 && CheckHitKey(KEY_INPUT_ESCAPE)) { CloseSocket(g_peer); g_is_net_battle = false; GoTitle(); }
            break;
        }

        case MODE_NET_ERROR: {
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(100, 180, "[通信エラー / 切断]", RED);
            DrawString(100, 220, g_net_status, WHITE);
            DrawString(100, 280, "[ESC] タイトルへ戻る", GRAY);
            if (CheckHitKey(KEY_INPUT_ESCAPE)) { g_is_net_battle = false; GoTitle(); }
            break;
        }
        }

        ScreenFlip();
    }

EXIT_GAME:
    CloseSocket(g_peer);
    CloseSocket(g_listen);
    WSACleanup();
    DeleteCharaGraphs();
    if (g_worldScreen != -1) DeleteGraph(g_worldScreen);
    DxLib_End();
    return 0;
}
