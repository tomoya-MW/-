// ====================================================
// ストーリー開始・周回・トレーニング・チュートリアル
// ====================================================
#pragma once

// ストーリーの1戦を準備する
static void SetupBattleRound() {
    int curStage = ClampI(g_slots[g_currentSlot].stageProgress, 0, LAST_STAGE);
    g_currentStageIdx = curStage;
    g_gimmickEnabled = true;
    InitStageGimmick(curStage);
    ClearBattleObjects();

    int myChara = ClampI(g_slots[g_currentSlot].chosenChara, 0, CHARA_COUNT - 1);
    int enemyChara = CPU_ROSTER[curStage].charaIndex;
    ResetFighter(g_me, 200.0f, 1, myChara, PlayableMaxHp(myChara));
    int bonus = curStage * 60 + GetHardLevel() * 100 - ((g_slots[g_currentSlot].difficulty == 0) ? 150 : 0);
    ResetFighter(g_other, 440.0f, -1, enemyChara, CHARA_PROFILES[enemyChara].maxHp + bonus);

    ResetBattleGlobals();
    ResetMatchStats();
    UpdateCamera(true);
}

// ラスボス（または裏ボス）撃破：称号を記録し、次の周回へ
static void OnRunCleared(bool beatSecretBoss) {
    SaveSlot& s = g_slots[g_currentSlot];
    int c = s.chosenChara;
    int rank = beatSecretBoss ? 2 : 1;
    if (s.clearRank[c] < rank) s.clearRank[c] = (unsigned char)rank;
    s.clearCount[c]++;
    if (!s.hasContinued) s.noContinueClear[c] = true;

    s.loopCount++;
    s.stageProgress = 0;
    s.needCharaSelect = true;
    SaveCurrentSlot();

    g_endingSecret = beatSecretBoss;
    g_endingRank = rank;
    g_endingUnlockChara = -1;
    int nextLap = s.loopCount + 1;
    for (int i = 0; i < ROSTER_COUNT; i++) {
        int rc = g_rosterOrder[i];
        if (CHARA_PROFILES[rc].unlockLap == nextLap) { g_endingUnlockChara = rc; break; }
    }
}

static void EnterCharaSelect(bool showUnlockBanner) {
    int chosen = g_slots[g_currentSlot].chosenChara;
    g_charaCursor = IsCharaUnlocked(chosen) ? RosterPosOf(chosen) : 0;
    g_cardScrollX = (float)g_charaCursor * 210.0f;
    g_unlockBannerChara = showUnlockBanner ? g_endingUnlockChara : -1;
    g_unlockBannerTimer = (g_unlockBannerChara >= 0) ? 300 : 0;
    g_lockedMsgTimer = 0;
    g_cmdListOpen = false;
    g_paused = false;
    g_mode = MODE_CHARA_SELECT;
}

// ----------------------------------------------------
// トレーニング
// ----------------------------------------------------
static void ResetTrainingPositions(bool corner) {
    float myX = corner ? 500.0f : 220.0f;
    float dmX = corner ? 590.0f : 420.0f;
    int myChara = g_me.charaIndex, dmChara = g_other.charaIndex;
    int sp = g_me.spGauge;
    ResetFighter(g_me, myX, 1, myChara, PlayableMaxHp(myChara));
    ResetFighter(g_other, dmX, -1, dmChara, CHARA_PROFILES[dmChara].maxHp);
    g_me.spGauge = sp;
    for (int i = 0; i < MAX_SHOTS; i++) g_shots[i].active = false;
    for (int i = 0; i < MAX_TRAPS; i++) g_traps[i].active = false;
    g_comboLogCount = 0;
    g_comboHits = 0;
    g_comboDmg = 0;
    g_challengeHitThisCombo = false;
    UpdateCamera(true);
}

static void SetupTraining() {
    g_is_net_battle = false;
    g_currentStageIdx = g_trainStage;
    g_gimmickEnabled = true;
    InitStageGimmick(g_trainStage);
    ClearBattleObjects();
    ResetFighter(g_me, 220.0f, 1, g_trainMyChara, PlayableMaxHp(g_trainMyChara));
    ResetFighter(g_other, 420.0f, -1, g_trainDummyChara, CHARA_PROFILES[g_trainDummyChara].maxHp);
    ResetBattleGlobals();
    ResetMatchStats();
    g_readyFightTimer = 80;
    memset(&g_moveInfo, 0, sizeof(g_moveInfo));
    memset(&g_adv, 0, sizeof(g_adv));
    g_challengeIdx = 0;
    for (int i = 0; i < 3; i++) g_challengeDone[i] = false;
    g_challengeHitThisCombo = false;
    g_comboLogCount = 0;
    g_comboHits = g_comboDmg = 0;
    g_lastComboHits = g_lastComboDmg = 0;
    g_maxComboHits = g_maxComboDmg = 0;
    g_inputHistCount = 0;
    g_dummyNeutralFrames = 0;
    g_meNeutralFrames = 0;
    UpdateCamera(true);
}

static void UpdateInputHistory(unsigned short bits) {
    if (g_inputHistCount > 0 && g_inputHist[0].bits == bits) {
        if (g_inputHist[0].frames < 999) g_inputHist[0].frames++;
        return;
    }
    int n = (g_inputHistCount < INPUT_HIST_MAX) ? g_inputHistCount : INPUT_HIST_MAX - 1;
    for (int i = n; i > 0; i--) g_inputHist[i] = g_inputHist[i - 1];
    g_inputHist[0].bits = bits;
    g_inputHist[0].frames = 1;
    if (g_inputHistCount < INPUT_HIST_MAX) g_inputHistCount++;
}

static void HandleTrainingKeys() {
    static const int numKeys[7] = { KEY_INPUT_1, KEY_INPUT_2, KEY_INPUT_3, KEY_INPUT_4, KEY_INPUT_5, KEY_INPUT_6, KEY_INPUT_7 };
    static const int padKeys[7] = { KEY_INPUT_NUMPAD1, KEY_INPUT_NUMPAD2, KEY_INPUT_NUMPAD3, KEY_INPUT_NUMPAD4, KEY_INPUT_NUMPAD5, KEY_INPUT_NUMPAD6, KEY_INPUT_NUMPAD7 };
    for (int i = 0; i < 7; i++) {
        if (KeyHit(numKeys[i]) || KeyHit(padKeys[i])) {
            g_dummyMode = (DummyMode)i;
            if (g_other.state == ACT_GUARD) { g_other.state = ACT_IDLE; g_other.stateTimer = 0; }
            g_other.crouching = false;
            SpawnPopup(g_other.x, g_other.y - 120, DUMMY_MODE_NAMES[i], YELLOW);
        }
    }
    if (KeyHit(KEY_INPUT_F1)) g_showHitbox = !g_showHitbox;
    if (KeyHit(KEY_INPUT_F2)) g_showFrames = !g_showFrames;
    if (KeyHit(KEY_INPUT_F3)) g_showInput = !g_showInput;
    if (KeyHit(KEY_INPUT_F4)) g_infiniteSp = !g_infiniteSp;
    if (KeyHit(KEY_INPUT_F5)) ResetTrainingPositions(false);
    if (KeyHit(KEY_INPUT_F7)) ResetTrainingPositions(true);
    if (KeyHit(KEY_INPUT_F6)) {
        g_challengeIdx = (g_challengeIdx + 1) % 3;
        g_challengeHitThisCombo = false;
        g_comboLogCount = 0;
    }
}

static void UpdateTrainingFrame() {
    unsigned short bits = ReadLocalBits();
    if (g_showInput) UpdateInputHistory(bits);

    UpdateControlledFighter(g_me, g_other, g_ctrl[0], bits);
    UpdateDummy();
    ResolvePushboxes();
    UpdateTraps();
    UpdateStageGimmick(g_currentStageIdx);
    UpdateShots();
    UpdateEffects();
    UpdateCamera(false);

    if (g_infiniteSp) g_me.spGauge = 100;

    // コンボの区切り：ダミーが自由に動ける状態に戻ったら確定
    bool dummyNeutral = !IsHitState(g_other.state) && g_other.state != ACT_SLIP && g_other.state != ACT_DOWN &&
        g_other.state != ACT_STUN && g_other.state != ACT_HELD && g_other.hitStop == 0;
    if (dummyNeutral) {
        if (g_comboHits > 0) {
            g_lastComboHits = g_comboHits;
            g_lastComboDmg = g_comboDmg;
            if (g_comboHits > g_maxComboHits) g_maxComboHits = g_comboHits;
            if (g_comboDmg > g_maxComboDmg) g_maxComboDmg = g_comboDmg;
        }
        g_comboHits = 0;
        g_comboDmg = 0;
        g_comboLogCount = 0;
        g_challengeHitThisCombo = false;
        g_dummyNeutralFrames++;
    }
    else {
        g_dummyNeutralFrames = 0;
    }
    if (g_dummyNeutralFrames > 45) g_other.hp = g_other.maxHp;
    if (g_other.hp <= 0) g_other.hp = 1;

    bool meNeutral = !IsHitState(g_me.state) && g_me.state != ACT_SLIP && g_me.state != ACT_STUN && g_me.state != ACT_DOWN;
    g_meNeutralFrames = meNeutral ? g_meNeutralFrames + 1 : 0;
    if (g_meNeutralFrames > 45) g_me.hp = g_me.maxHp;
    if (g_me.hp <= 0) g_me.hp = 1;
}

// ----------------------------------------------------
// チュートリアル（3章構成。クリアするまで他のモードは遊べない）
// ----------------------------------------------------
enum TutKind { TK_MOVE, TK_JUMP, TK_CROUCH, TK_HIT, TK_GUARD, TK_LOW_GUARD, TK_PARRY, TK_PARRY_HIT, TK_DASH, TK_AIR, TK_DODGE, TK_COMBO, TK_BURST };
enum TutDummy { TD_STOP, TD_GUARD, TD_ATTACK, TD_LOW_ATTACK, TD_COMBO };
enum TutPhase { TP_INTRO, TP_PLAY, TP_STEP_CLEAR, TP_CHAPTER_CLEAR, TP_ALL_CLEAR };
static const int TUT_HIT_ANY_J = -1;
static const int TUT_HIT_ANY_K = -2;
static const int TUT_HIT_ANY_L = -3;
static const int TUT_HIT_SUPER = -4;

struct TutStep {
    const char* title;
    const char* line1;
    const char* line2;
    int kind;
    int param;
    int target;
    int dummy;
    bool fullSp;
};

static const TutStep TUT_STEPS[] = {
    // 第1章：基本操作
    { "移動", "A / D で左右に歩いてみよう", "右へ少し、左へ少し歩けばOK", TK_MOVE, 0, 2, TD_STOP, false },
    { "ジャンプ", "W でジャンプ！", "2回ジャンプしてみよう", TK_JUMP, 0, 2, TD_STOP, false },
    { "しゃがみ", "S を押している間しゃがむ", "しゃがむと高い攻撃をかわせる。S を少し押し続けよう", TK_CROUCH, 0, 1, TD_STOP, false },
    { "弱攻撃 J", "J は出の速い弱攻撃。ダミーに3回当てよう", "S+J でしゃがみ弱（下段）、空中 J でジャンプ攻撃（中段）", TK_HIT, TUT_HIT_ANY_J, 3, TD_STOP, false },
    { "強攻撃 K", "K は威力とリーチのある強攻撃。2回当てよう", "S+K は足払い（下段・ダウンを奪える）", TK_HIT, TUT_HIT_ANY_K, 2, TD_STOP, false },
    { "必殺技 L", "L は必殺技。大乗殿は昇天脚（対空にも使える）", "前+L で気弾、S+L で鉄拳突き。どれかを2回当てよう", TK_HIT, TUT_HIT_ANY_L, 2, TD_STOP, false },
    { "ガード", "相手の攻撃は SPACE 長押しでガード", "ダミーの頭に「！」が出たら SPACE を押しっぱなし。3回ガード", TK_GUARD, 0, 3, TD_ATTACK, false },
    // 第2章：攻防の基本
    { "しゃがみガード", "下段攻撃は S+SPACE のしゃがみガードでしか防げない", "ダミーが足元を狙ってくる。S を押しながら SPACE で2回防ごう", TK_LOW_GUARD, 0, 2, TD_LOW_ATTACK, false },
    { "投げ", "ガードを固める相手には投げが有効", "密着して 左Shift。投げを1回決めよう（つかんだら後ろで裏投げ）", TK_HIT, TAG_THROW, 1, TD_GUARD, false },
    { "パリィ", "攻撃が当たる瞬間に SPACE を短く押すとパリィ", "「！」が消えた直後が狙い目。2回成功させよう", TK_PARRY, 0, 2, TD_ATTACK, false },
    { "パリィから反撃", "パリィ成功で相手はピヨり、次の一撃が1.5倍になる", "パリィ → すぐ近づいて J を当てよう", TK_PARRY_HIT, 0, 1, TD_ATTACK, false },
    // 第3章：応用
    { "ダッシュ", "同じ方向キーを素早く2回押すとダッシュ", "DD と AA をそれぞれ1回ずつ出そう", TK_DASH, 0, 2, TD_STOP, false },
    { "空中アクション", "ジャンプ中に W でもう一度ジャンプ（2段ジャンプ）", "ジャンプ中に S で急降下。両方やってみよう", TK_AIR, 0, 2, TD_STOP, false },
    { "回避", "ガード中（SPACE押しっぱなし）に A/D で無敵ロール", "ダミーの「！」を見てから回避！ 2回かわそう", TK_DODGE, 0, 2, TD_ATTACK, false },
    { "コンボ", "当たった瞬間に次のボタンを押すと技が繋がる", "J → J → K → L の順に押して4ヒットのコンボを決めよう", TK_COMBO, 0, 1, TD_STOP, false },
    { "バースト", "連続で殴られている最中に SPACE でコンボから抜け出せる", "SPを50使う（今回は満タン）。殴られたら SPACE！", TK_BURST, 0, 1, TD_COMBO, true },
    { "超必殺技", "SPが100溜まったら I キーで超必殺技", "近づいて超必殺技を当てよう（今回はSP満タン）", TK_HIT, TUT_HIT_SUPER, 1, TD_STOP, true },
};
static const int TUT_CHAPTER_COUNT = 3;
static const int TUT_CHAPTER_START[TUT_CHAPTER_COUNT + 1] = { 0, 7, 11, sizeof(TUT_STEPS) / sizeof(TUT_STEPS[0]) };
static const char* TUT_CHAPTER_NAMES[TUT_CHAPTER_COUNT] = { "基本操作", "攻防の基本", "応用テクニック" };
static const char* TUT_CHAPTER_DESC[TUT_CHAPTER_COUNT] = {
    "「歩く・跳ぶ・しゃがむ・攻撃・ガード」を覚えよう",
    "下段とガード、守りを崩す「投げ」、攻撃を弾く「パリィ」",
    "ダッシュ・回避・コンボ・バースト・超必殺技"
};

struct TutorialState {
    int chapter;
    int step;
    int phase;
    int timer;
    int progress;
    float movedR, movedL, lastX;
    bool flagA, flagB;
    int parryTimer;
    int crouchFrames;
    int attackDelay;
    int attackCooldown;
};
static TutorialState g_tut;
static int g_tutorialChapters = 0;
static int g_titleLockedMsgTimer = 0;

static bool TutorialDone() { return g_tutorialChapters >= TUT_CHAPTER_COUNT; }

static void LoadTutorialProgress() {
    FILE* fp = NULL;
    fopen_s(&fp, "tutorial.dat", "rb");
    if (!fp) return;
    int v = 0;
    if (fread(&v, sizeof(v), 1, fp) == 1) g_tutorialChapters = ClampI(v, 0, TUT_CHAPTER_COUNT);
    fclose(fp);
}

static void SaveTutorialProgress() {
    FILE* fp = NULL;
    fopen_s(&fp, "tutorial.dat", "wb");
    if (!fp) return;
    fwrite(&g_tutorialChapters, sizeof(g_tutorialChapters), 1, fp);
    fclose(fp);
}

static void StartTutorialStep(int step) {
    g_tut.step = step;
    g_tut.progress = 0;
    g_tut.movedR = g_tut.movedL = 0.0f;
    g_tut.flagA = g_tut.flagB = false;
    g_tut.parryTimer = 0;
    g_tut.crouchFrames = 0;
    g_tut.attackDelay = 0;
    g_tut.attackCooldown = 60;
    ResetFighter(g_me, 220.0f, 1, 0, PlayableMaxHp(0));
    ResetFighter(g_other, 420.0f, -1, 0, CHARA_PROFILES[0].maxHp);
    g_tut.lastX = g_me.x;
    for (int i = 0; i < MAX_SHOTS; i++) g_shots[i].active = false;
    ResetControl(g_ctrl[0]);
    g_readyFightTimer = 80;
    UpdateCamera(true);
}

static void SetupTutorial(int chapter) {
    g_is_net_battle = false;
    g_currentStageIdx = 0;
    g_gimmickEnabled = false;
    InitStageGimmick(0);
    ClearBattleObjects();
    ResetBattleGlobals();
    ResetMatchStats();
    g_tut.chapter = ClampI(chapter, 0, TUT_CHAPTER_COUNT - 1);
    StartTutorialStep(TUT_CHAPTER_START[g_tut.chapter]);
    g_tut.phase = TP_INTRO;
    g_tut.timer = 0;
}

static void TutorialProgress() {
    const TutStep& s = TUT_STEPS[g_tut.step];
    g_tut.progress++;
    if (g_tut.progress >= s.target) {
        g_tut.progress = s.target;
        g_tut.phase = TP_STEP_CLEAR;
        g_tut.timer = 0;
        g_tut.attackDelay = 0;
        g_flashTimer = 8;
    }
    else {
        SpawnPopup(g_me.x, g_me.y - 120, "いいね！", GREEN);
    }
}

static bool TagIs(int tag, int slotA, int slotB, int slotC) {
    return tag == TAGM(slotA) || tag == TAGM(slotB) || tag == TAGM(slotC);
}

static void TutorialEvent(int ev, int param) {
    if (g_mode != MODE_TUTORIAL || g_tut.phase != TP_PLAY) return;
    const TutStep& s = TUT_STEPS[g_tut.step];
    bool isJ = TagIs(param, MS_5J, MS_2J, MS_JJ);

    switch (s.kind) {
    case TK_JUMP:
        if (ev == TE_JUMP) TutorialProgress();
        break;
    case TK_HIT:
        if (ev != TE_HIT) break;
        if (s.param == TUT_HIT_ANY_J) { if (isJ) TutorialProgress(); }
        else if (s.param == TUT_HIT_ANY_K) { if (TagIs(param, MS_5K, MS_2K, MS_JK)) TutorialProgress(); }
        else if (s.param == TUT_HIT_ANY_L) {
            if (TagIs(param, MS_5L, MS_6L, MS_2L) || param == TAGM(MS_JL) || param == TAG_SHOT) TutorialProgress();
        }
        else if (s.param == TUT_HIT_SUPER) { if (param == TAGM(MS_SUPER) || param == TAGM(MS_JSUPER)) TutorialProgress(); }
        else if (param == s.param) TutorialProgress();
        break;
    case TK_GUARD:
        if (ev == TE_GUARD) TutorialProgress();
        break;
    case TK_LOW_GUARD:
        if (ev == TE_GUARD && param == 1) TutorialProgress();
        break;
    case TK_PARRY:
        if (ev == TE_PARRY) TutorialProgress();
        break;
    case TK_PARRY_HIT:
        if (ev == TE_PARRY) {
            g_tut.parryTimer = 90;
            SpawnPopup(g_me.x, g_me.y - 130, "今だ！ J で反撃！", YELLOW);
        }
        else if (ev == TE_HIT && isJ && g_tut.parryTimer > 0) {
            TutorialProgress();
        }
        break;
    case TK_DASH:
        if (ev != TE_DASH) break;
        if (param > 0 && !g_tut.flagA) { g_tut.flagA = true; TutorialProgress(); }
        else if (param < 0 && !g_tut.flagB) { g_tut.flagB = true; TutorialProgress(); }
        break;
    case TK_AIR:
        if (ev == TE_DOUBLE_JUMP && !g_tut.flagA) { g_tut.flagA = true; TutorialProgress(); }
        else if (ev == TE_FAST_FALL && !g_tut.flagB) { g_tut.flagB = true; TutorialProgress(); }
        break;
    case TK_DODGE:
        if (ev != TE_DODGE) break;
        if (g_tut.attackDelay > 0 || g_other.state == ACT_ATTACK) TutorialProgress();
        else SpawnPopup(g_me.x, g_me.y - 120, "「！」が出てから回避しよう", GRAY);
        break;
    case TK_COMBO:
        if (ev == TE_COMBO && param >= 4) TutorialProgress();
        break;
    case TK_BURST:
        if (ev == TE_BURST) TutorialProgress();
        break;
    default:
        break;
    }
}

// チュートリアルのダミー：ステップに合わせて止まる・ガードする・予告付きで殴ってくる
static void UpdateTutorialDummy() {
    Fighter& d = g_other;
    const TutStep& s = TUT_STEPS[g_tut.step];
    if (!UpdateFighterCommon(d, g_me, false)) return;
    UpdateStateSpecials(d, g_me);

    bool freeGuard = (d.state == ACT_GUARD && d.stateTimer == 0 && d.guardStun == 0);
    float dist = fabsf(g_me.x - d.x);

    switch (s.dummy) {
    case TD_GUARD:
        if (d.state == ACT_IDLE && d.isGrounded) {
            d.state = ACT_GUARD;
            d.stateTimer = 0;
            d.crouching = false;
        }
        break;
    case TD_ATTACK:
    case TD_LOW_ATTACK:
    case TD_COMBO:
        if (freeGuard) d.state = ACT_IDLE;
        if (g_tut.attackCooldown > 0) g_tut.attackCooldown--;
        if (d.state != ACT_IDLE || !d.isGrounded) break;
        if (g_tut.attackDelay > 0) {
            d.vx = 0;
            d.crouching = (s.dummy == TD_LOW_ATTACK);
            if (--g_tut.attackDelay == 0) {
                StartMove(d, g_me, (s.dummy == TD_LOW_ATTACK) ? MS_2J : MS_5J);
                g_tut.attackCooldown = 70;
            }
            break;
        }
        d.crouching = false;
        if (dist > 65.0f) {
            d.vx = (float)d.dir * 2.5f;
            break;
        }
        if (g_tut.attackCooldown == 0) {
            if (s.dummy == TD_COMBO) {
                StartMove(d, g_me, MS_5J);
                g_tut.attackCooldown = 4;
            }
            else {
                g_tut.attackDelay = 30;
            }
        }
        break;
    default:
        if (freeGuard) d.state = ACT_IDLE;
        break;
    }
    CheckAttackHit(d, g_me);
}

static void UpdateTutorialFrame() {
    if (g_tut.parryTimer > 0) g_tut.parryTimer--;

    switch (g_tut.phase) {
    case TP_INTRO:
        g_tut.timer++;
        UpdateEffects();
        if (g_tut.timer > 30 && KeyHit(KEY_INPUT_RETURN)) {
            g_tut.phase = TP_PLAY;
            g_tut.timer = 0;
        }
        break;

    case TP_PLAY: {
        const TutStep& s = TUT_STEPS[g_tut.step];
        UpdateControlledFighter(g_me, g_other, g_ctrl[0], ReadLocalBits());
        if (g_tut.phase != TP_PLAY) { UpdateEffects(); break; }
        UpdateTutorialDummy();
        ResolvePushboxes();
        UpdateShots();
        UpdateEffects();
        UpdateCamera(false);

        if (s.fullSp) g_me.spGauge = 100;

        if (s.kind == TK_MOVE) {
            float dx = g_me.x - g_tut.lastX;
            if (g_me.isGrounded && (g_me.state == ACT_IDLE || g_me.state == ACT_DASH)) {
                if (dx > 0) g_tut.movedR += dx;
                else g_tut.movedL -= dx;
            }
            if (!g_tut.flagA && g_tut.movedR >= 60.0f) { g_tut.flagA = true; TutorialProgress(); }
            if (g_tut.phase == TP_PLAY && !g_tut.flagB && g_tut.movedL >= 60.0f) { g_tut.flagB = true; TutorialProgress(); }
        }
        if (s.kind == TK_CROUCH && g_me.crouching && g_me.state == ACT_IDLE) {
            if (++g_tut.crouchFrames == 40) TutorialProgress();
        }
        g_tut.lastX = g_me.x;

        if (!IsHitState(g_other.state) && g_other.hp < g_other.maxHp / 2) g_other.hp = g_other.maxHp;
        if (!IsHitState(g_me.state) && g_me.hp < g_me.maxHp / 2) g_me.hp = g_me.maxHp;
        if (g_me.hp <= 0) g_me.hp = 1;
        if (g_other.hp <= 0) g_other.hp = 1;
        break;
    }

    case TP_STEP_CLEAR:
        UpdateEffects();
        if (++g_tut.timer >= 60) {
            int next = g_tut.step + 1;
            if (next >= TUT_CHAPTER_START[g_tut.chapter + 1]) {
                if (g_tutorialChapters < g_tut.chapter + 1) {
                    g_tutorialChapters = g_tut.chapter + 1;
                    SaveTutorialProgress();
                }
                g_tut.phase = (g_tut.chapter + 1 >= TUT_CHAPTER_COUNT) ? TP_ALL_CLEAR : TP_CHAPTER_CLEAR;
                g_tut.timer = 0;
            }
            else {
                StartTutorialStep(next);
                g_tut.phase = TP_PLAY;
            }
        }
        break;

    case TP_CHAPTER_CLEAR:
    case TP_ALL_CLEAR:
        g_tut.timer++;
        UpdateEffects();
        break;
    }
}

static void DrawTutorialOverlay() {
    const TutStep& s = TUT_STEPS[g_tut.step];
    int chapterStart = TUT_CHAPTER_START[g_tut.chapter];
    int stepsInChapter = TUT_CHAPTER_START[g_tut.chapter + 1] - chapterStart;

    // ダミーの攻撃予告：輪が縮みきって「！」が消えた直後に攻撃が来る
    if (g_tut.attackDelay > 0) {
        int dx, dy;
        WorldToScreen(g_other.x, g_other.y - 70.0f, dx, dy);
        DrawCircle(dx, dy + 12, 10 + g_tut.attackDelay * 2, YELLOW, FALSE, 2);
        DrawExtendString(dx - 16, dy - 4, 2.0, 2.0, "！", RED);
    }

    DrawBox(20, 70, 620, 160, GetColor(5, 10, 25), TRUE);
    DrawBox(20, 70, 620, 160, YELLOW, FALSE);
    DrawFormatString(30, 74, YELLOW, "第%d章 %s   STEP %d/%d", g_tut.chapter + 1, TUT_CHAPTER_NAMES[g_tut.chapter],
        g_tut.step - chapterStart + 1, stepsInChapter);
    DrawString(450, 74, "ESC:ポーズ TAB:技表", GRAY);
    DrawFormatString(30, 96, ORANGE, "【%s】", s.title);
    DrawFormatString(470, 96, WHITE, "%d / %d", g_tut.progress, s.target);
    DrawBox(530, 100, 610, 110, GetColor(40, 40, 40), TRUE);
    DrawBox(530, 100, 530 + 80 * g_tut.progress / s.target, 110, GREEN, TRUE);
    DrawString(30, 118, s.line1, WHITE);
    DrawString(30, 138, s.line2, CYAN);

    if (g_tut.phase == TP_INTRO) {
        DrawBox(80, 180, 560, 340, BLACK, TRUE);
        DrawBox(80, 180, 560, 340, YELLOW, FALSE);
        char buf[64];
        sprintf_s(buf, "第%d章", g_tut.chapter + 1);
        DrawExtendString(270, 196, 2.0, 2.0, buf, YELLOW);
        int w = GetDrawStringWidth(TUT_CHAPTER_NAMES[g_tut.chapter], (int)strlen(TUT_CHAPTER_NAMES[g_tut.chapter]));
        DrawString(320 - w / 2, 240, TUT_CHAPTER_NAMES[g_tut.chapter], WHITE);
        w = GetDrawStringWidth(TUT_CHAPTER_DESC[g_tut.chapter], (int)strlen(TUT_CHAPTER_DESC[g_tut.chapter]));
        DrawString(320 - w / 2, 270, TUT_CHAPTER_DESC[g_tut.chapter], CYAN);
        DrawString(250, 308, "[Enter] スタート", (g_frameCount / 20) % 2 ? WHITE : GRAY);
    }
    else if (g_tut.phase == TP_STEP_CLEAR) {
        DrawExtendString(250, 220, 3.0, 3.0, "OK!!", (g_frameCount / 4) % 2 ? YELLOW : ORANGE);
    }
    else if (g_tut.phase == TP_CHAPTER_CLEAR) {
        DrawBox(80, 180, 560, 340, BLACK, TRUE);
        DrawBox(80, 180, 560, 340, GREEN, FALSE);
        char buf[64];
        sprintf_s(buf, "第%d章 クリア！", g_tut.chapter + 1);
        DrawExtendString(200, 196, 2.0, 2.0, buf, GREEN);
        DrawFormatString(150, 250, WHITE, "次は 第%d章「%s」", g_tut.chapter + 2, TUT_CHAPTER_NAMES[g_tut.chapter + 1]);
        DrawString(110, 300, "[Enter] 次の章へ    [Esc] タイトルへ（続きから再開できます）", GRAY);
    }
    else if (g_tut.phase == TP_ALL_CLEAR) {
        DrawBox(50, 170, 590, 360, BLACK, TRUE);
        DrawBox(50, 170, 590, 360, YELLOW, FALSE);
        DrawExtendString(150, 186, 2.0, 2.0, "チュートリアル完了！", (g_frameCount / 8) % 2 ? YELLOW : ORANGE);
        DrawString(80, 240, "ストーリー・トレーニング・通信対戦が遊べるようになりました！", WHITE);
        DrawString(80, 266, "キャラ固有の技は、試合中やキャラ選択で TAB を押すと確認できます。", CYAN);
        DrawString(80, 292, "トレーニングではコンボ課題やフレーム表示で、さらに上達できます。", CYAN);
        DrawString(220, 330, "[Enter] タイトルへ", (g_frameCount / 20) % 2 ? WHITE : GRAY);
    }
}
