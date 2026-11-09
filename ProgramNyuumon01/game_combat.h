// ====================================================
// エフェクト・罠・当たり判定・ダメージ
// ====================================================
#pragma once

static void TriggerScreenShake(int frames) {
    if (frames > g_screenShakeTimer) g_screenShakeTimer = frames;
}

static void QueueNetFx(float x, float y, unsigned int col, int count, int kind, float len) {
    if (!g_netRecordFx || g_netFxCount >= NET_MAX_FX) return;
    NetFxEvent& e = g_netFxQueue[g_netFxCount++];
    e.x = (short)x; e.y = (short)y; e.color = col;
    e.count = (unsigned char)ClampI(count, 0, 255);
    e.kind = (unsigned char)kind;
    e.len = (unsigned char)ClampI((int)len, 0, 255);
}

static void SpawnHitFX(float x, float y, unsigned int col, int count) {
    QueueNetFx(x, y, col, count, 0, 0);
    for (int i = 0; i < count; i++) {
        for (int p = 0; p < MAX_FX; p++) {
            if (!g_fx[p].active) {
                g_fx[p].active = true;
                g_fx[p].x = x;
                g_fx[p].y = y;
                float ang = (float)(rand() % 360) * 3.14159f / 180.0f;
                float spd = (float)(rand() % 50 + 20) / 10.0f;
                g_fx[p].vx = cosf(ang) * spd;
                g_fx[p].vy = sinf(ang) * spd;
                g_fx[p].life = 14 + rand() % 12;
                g_fx[p].color = col;
                break;
            }
        }
    }
}

static void SpawnSparks(float x, float y, unsigned int col, int count, float baseLen) {
    QueueNetFx(x, y, col, count, 1, baseLen);
    for (int i = 0; i < count; i++) {
        for (int s = 0; s < MAX_SPARKS; s++) {
            if (!g_sparks[s].active) {
                g_sparks[s].active = true;
                g_sparks[s].x = x;
                g_sparks[s].y = y;
                g_sparks[s].angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                g_sparks[s].length = baseLen + (float)(rand() % 20);
                g_sparks[s].color = col;
                g_sparks[s].life = 8 + rand() % 6;
                break;
            }
        }
    }
}

static void SpawnPopup(float x, float y, const char* text, unsigned int col) {
    if (g_netRecordFx && g_netPopupCount < NET_MAX_POPUPS) {
        NetPopupEvent& e = g_netPopupQueue[g_netPopupCount++];
        e.x = (short)x; e.y = (short)y; e.color = col;
        strncpy_s(e.text, text, _TRUNCATE);
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!g_popups[i].active) {
            g_popups[i].active = true;
            g_popups[i].x = x;
            g_popups[i].y = y;
            strncpy_s(g_popups[i].text, text, _TRUNCATE);
            g_popups[i].color = col;
            g_popups[i].life = 55;
            return;
        }
    }
}

static void SpawnShot(bool fromMe, float x, float y, float vx, float vy, int dmg, unsigned int col, int rad, int type, int sub = 0, float gravity = 0.0f, bool low = false) {
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!g_shots[i].active) {
            Projectile& p = g_shots[i];
            p.active = true;
            p.isFromMe = fromMe;
            p.x = x; p.y = y;
            p.vx = vx; p.vy = vy;
            p.gravity = gravity;
            p.damage = dmg;
            p.color = col;
            p.life = (type == 4) ? 240 : ((type == 6) ? 55 : 100);
            p.radius = rad;
            p.projType = type;
            p.subType = sub;
            p.low = low;
            p.angle = 0.0f;
            return;
        }
    }
}

static void FighterShot(Fighter& f, float x, float y, float vx, float vy, int dmg, unsigned int col, int rad, int type, int sub = 0, float gravity = 0.0f, bool low = false) {
    SpawnShot(&f == &g_me, x, y, vx, vy, dmg, col, rad, type, sub, gravity, low);
}

// ----------------------------------------------------
// 統計・技名・フレーム記録
// ----------------------------------------------------
static void ResetMatchStats() {
    memset(g_stats, 0, sizeof(g_stats));
    memset(g_moveUse, 0, sizeof(g_moveUse));
}

static const char* MostUsedMove(int idx, int chara) {
    int best = -1, bestCount = 0;
    for (int s = 0; s <= MS_COUNT; s++) {
        if (g_moveUse[idx][s] > bestCount) { bestCount = g_moveUse[idx][s]; best = s; }
    }
    if (best < 0) return "なし";
    if (best == MS_COUNT) return "投げ";
    return g_moves[chara][best].name;
}

static void LogComboTag(int tag) {
    if (g_mode != MODE_TRAINING) return;
    if (g_comboLogCount < 32) g_comboLog[g_comboLogCount++] = tag;

    const ComboChallenge& ch = CHALLENGES[g_me.charaIndex][g_challengeIdx];
    int j = 0;
    for (int i = 0; i < g_comboLogCount && j < ch.tagCount; i++) {
        if (g_comboLog[i] == ch.tags[j]) j++;
    }
    if (j == ch.tagCount && !g_challengeHitThisCombo) {
        g_challengeHitThisCombo = true;
        g_challengeDone[g_challengeIdx] = true;
        g_flashTimer = 8;
        SpawnPopup(g_me.x, g_me.y - 140, "コンボ課題 達成!!", YELLOW);
    }
}

static void RecordAdvantage(Fighter& atk, Fighter& def, int defStun, bool guarded, bool launch) {
    if (g_mode != MODE_TRAINING) return;
    g_adv.valid = true;
    g_adv.byDummy = (&atk == &g_other);
    g_adv.guarded = guarded;
    g_adv.launch = launch;
    g_adv.adv = defStun + (def.hitStop - atk.hitStop) - atk.stateTimer;
    int fastest = 99;
    for (int s = MS_5J; s <= MS_2J; s++) {
        const MoveDef& m = g_moves[def.charaIndex][s];
        if (m.valid && m.startup < fastest) fastest = m.startup;
    }
    g_adv.punishable = guarded && g_adv.adv < 0 && -g_adv.adv > fastest;
}

static void RecordMoveInfo(Fighter& f) {
    MoveInfo& m = g_moveInfo[FighterIndex(f)];
    int st, act;
    GetMoveFrames(f, st, act);
    m.valid = true;
    m.chara = f.charaIndex;
    m.slot = f.move;
    m.total = f.stateTimer;
    m.startup = st;
    m.active = act;
    if (m.startup + m.active > m.total) m.active = m.total - m.startup;
    if (m.active < 0) m.active = 0;
    m.recovery = m.total - m.startup - m.active;
    if (m.recovery < 0) m.recovery = 0;
}

static void SetLabel(Fighter& f, int slot) {
    f.labelSlot = slot;
    f.labelTimer = 45;
}

static void SetCounterLabel(Fighter& atk, int type) {
    int i = FighterIndex(atk);
    g_counterLabelTimer[i] = 70;
    g_counterLabelType[i] = type;
}

// ----------------------------------------------------
// ステージ
// ----------------------------------------------------
static void InitStageGimmick(int stageIdx) {
    g_gimmick.x = 320.0f + (float)(rand() % 140 - 70);
    g_gimmick.y = GROUND_Y;
    g_gimmick.w = 34.0f;
    g_gimmick.h = 130.0f;
    g_gimmick.maxHp = 160;
    g_gimmick.hp = g_gimmick.maxHp;
    g_gimmick.isFalling = false;
    g_gimmick.fallAngle = 0.0f;
    g_gimmick.fallen = false;
    g_gimmick.activeHitBox = 0;
    g_gimmick.stageTimer = 0;
    g_gimmick.hazardActive = false;
    g_gimmick.hazardWarning = false;
    g_gimmick.hazardCountdown = 0;
    g_gimmick.itemActive = false;
    g_gimmick.itemX = 0;
    g_gimmick.itemY = 0;
}

static bool InHazardSafeZone(float x) {
    return !g_gimmick.fallen && !g_gimmick.isFalling && fabsf(x - g_gimmick.x) < HAZARD_SAFE_HALF;
}

static void ResetFighter(Fighter& f, float x, int dir, int chara, int maxHp) {
    f = Fighter();
    f.x = x; f.y = GROUND_Y;
    f.dir = dir;
    f.isGrounded = true;
    f.charaIndex = chara;
    f.maxHp = maxHp;
    f.hp = maxHp;
    f.state = ACT_IDLE;
    f.moveDir = dir;
    f.labelSlot = -3;
}

// 天輪太子はプレイヤーが使うと「封印解放版」：体力が大きく下がる
static int PlayableMaxHp(int chara) {
    return (chara == 6) ? 780 : CHARA_PROFILES[chara].maxHp;
}

static void ClearBattleObjects() {
    for (int i = 0; i < MAX_SHOTS; i++) g_shots[i].active = false;
    for (int i = 0; i < MAX_FX; i++) g_fx[i].active = false;
    for (int i = 0; i < MAX_SPARKS; i++) g_sparks[i].active = false;
    for (int i = 0; i < MAX_TRAPS; i++) g_traps[i].active = false;
    for (int i = 0; i < MAX_POPUPS; i++) g_popups[i].active = false;
}

static void ResetBattleGlobals() {
    g_parryNoticeTimer = 0;
    g_readyFightTimer = 0;
    g_battleTimerFrames = 0;
    g_roundTime = 99;
    g_isDraw = false;
    g_superCutinTimer = 0;
    g_superIsUltimate = false;
    g_cutinIsCounter = false;
    g_kokusenStrobe = 0;
    g_isKokusenKoFinish = false;
    g_flashTimer = 0;
    g_screenShakeTimer = 0;
    g_cpuTilt = 0;
    g_cpuTiltTimer = 0;
    g_cpuThinkTimer = 30;
    g_cmdListOpen = false;
    g_paused = false;
    g_koWinner = 0;
    g_counterLabelTimer[0] = g_counterLabelTimer[1] = 0;
    g_camZoom = 1.0f;
    g_camX = 0.0f;
    g_camY = 0.0f;
    ResetControl(g_ctrl[0]);
    ResetControl(g_ctrl[1]);
}

static void AddCpuTilt(int amount) {
    if (g_is_net_battle) return;
    g_cpuTilt = ClampI(g_cpuTilt + amount, 0, 6);
    g_cpuTiltTimer = 480;
}

// ----------------------------------------------------
// 罠
// ----------------------------------------------------
static void PlaceTrap(bool ownerIsMe, int type, float x) {
    if (x < 30.0f) x = 30.0f;
    if (x > 610.0f) x = 610.0f;
    int count = 0, oldest = -1;
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (!g_traps[i].active || g_traps[i].ownerIsMe != ownerIsMe) continue;
        count++;
        if (oldest < 0 || g_traps[i].life < g_traps[oldest].life) oldest = i;
    }
    if (count >= 4 && oldest >= 0) g_traps[oldest].active = false;

    for (int i = 0; i < MAX_TRAPS; i++) {
        if (!g_traps[i].active) {
            g_traps[i].active = true;
            g_traps[i].ownerIsMe = ownerIsMe;
            g_traps[i].type = type % 3;
            g_traps[i].x = x;
            g_traps[i].life = 900;
            g_traps[i].armDelay = 20;
            SpawnHitFX(x, GROUND_Y + 10, GRAY, 5);
            return;
        }
    }
}

static void UpdateTraps() {
    for (int i = 0; i < MAX_TRAPS; i++) {
        Trap& t = g_traps[i];
        if (!t.active) continue;
        if (--t.life <= 0) { t.active = false; continue; }
        if (t.armDelay > 0) { t.armDelay--; continue; }

        Fighter& victim = t.ownerIsMe ? g_other : g_me;
        Fighter& owner = t.ownerIsMe ? g_me : g_other;
        if (!victim.isGrounded || victim.invulFrames > 0) continue;
        if (IsHitState(victim.state) || victim.state == ACT_SLIP || victim.state == ACT_STUN ||
            victim.state == ACT_DOWN || victim.state == ACT_HELD || victim.state == ACT_COUNTER_STRIKE) continue;
        if (fabsf(victim.x - t.x) > 22.0f) continue;

        victim.animTimer = 0;
        victim.hasHit = false;
        victim.hitStop = 6;
        victim.stanceLevel = 0;
        victim.guardStun = 0;
        victim.crouching = false;
        if (t.type == TRAP_BANANA) {
            victim.state = ACT_SLIP;
            victim.stateTimer = 55;
            victim.vx = (victim.vx != 0.0f) ? ((victim.vx > 0) ? 6.0f : -6.0f) : (float)victim.dir * 6.0f;
            victim.hp -= 15;
            SpawnPopup(victim.x, victim.y - 90, "ツルッ!!", YELLOW);
        }
        else if (t.type == TRAP_LEGO) {
            victim.state = ACT_STUN;
            victim.stateTimer = 45;
            victim.vx = 0;
            victim.hp -= 30;
            SpawnPopup(victim.x, victim.y - 90, "イッッテェ!!", RED);
        }
        else {
            victim.state = ACT_SLIP;
            victim.stateTimer = 40;
            victim.vx = (victim.x < t.x) ? -4.0f : 4.0f;
            victim.hp -= 20;
            SpawnPopup(victim.x, victim.y - 90, "ガシャーン!", ORANGE);
        }
        SpawnHitFX(t.x, GROUND_Y, YELLOW, 14);
        TriggerScreenShake(5);
        owner.spGauge += 12; if (owner.spGauge > 100) owner.spGauge = 100;
        if (&victim == &g_other) {
            if (IsStoryMode()) {
                AddCpuTilt(1);
                SpawnPopup(victim.x + 30, victim.y - 115, "ｲﾗｯ!!", RED);
            }
            LogComboTag(TAG_TRAP);
        }
        t.active = false;
    }
}

// ----------------------------------------------------
// 攻撃判定・やられ判定
// ----------------------------------------------------
struct AttackBox {
    float offX, offY, w, h;
    int damage;
    int stun;
    float kx, ky;
    int flags;
};

static bool GetAttackBox(const Fighter& atk, float distToDef, AttackBox& b) {
    memset(&b, 0, sizeof(b));
    if (atk.state != ACT_ATTACK) return false;
    const MoveDef& m = CurMove(atk);
    if (!m.valid || (m.flags & MF_NOHITBOX)) return false;
    float d = (float)atk.dir;
    b.w = m.w; b.h = m.h; b.offX = d * m.offX; b.offY = m.offY;
    b.damage = m.damage; b.stun = m.stun;
    b.kx = d * m.kx; b.ky = m.ky;
    b.flags = m.flags;

    int c = atk.charaIndex;
    if (c == 3 && (atk.move == MS_5L || atk.move == MS_SUPER)) {
        // いのしし突進：サイコロの出目で性能が変わる
        int face = atk.boarSuper ? 6 : ClampI(atk.diceFace, 3, 6);
        const BoarFace& bf = BOAR_FACES[face];
        b.damage = atk.boarSuper ? 260 : bf.damage;
        b.kx = d * bf.knockX;
        b.flags = MF_LAUNCH | (bf.unblockable ? MF_UNBLOCK : 0);
    }
    if (c == 2 && (atk.move == MS_5J || atk.move == MS_2J || atk.move == MS_5K || atk.move == MS_2K)) {
        // アナンダ：伸ばした手足の先端は威力1.5倍
        float tip = (atk.move == MS_5K || atk.move == MS_2K) ? 120.0f : 100.0f;
        if (distToDef > tip) b.damage = b.damage * 3 / 2;
    }
    if (c == 0 && atk.variant == 1) {
        if (atk.move == MS_JSUPER) {          // 黒閃
            b.w = 100; b.h = 110; b.offX = d * 45; b.offY = -25;
            b.damage = 450; b.stun = 80; b.kx = d * 5.0f; b.ky = -24.0f; b.flags = MF_LAUNCH;
        }
        else if (atk.move == MS_SUPER) {      // ドラゴンスープレックス
            b.w = 95; b.h = 90; b.offX = d * 45; b.offY = -35;
            b.damage = 330; b.stun = 70; b.kx = d * 3.5f; b.ky = -22.0f; b.flags = MF_LAUNCH;
        }
    }
    return b.damage > 0;
}

// やられ判定（論理座標。下端が床の線に一致する）
static void GetHurtBox(const Fighter& f, float& cx, float& cy, float& w, float& h) {
    bool big = (f.charaIndex == 4);
    w = big ? 60.0f : 30.0f;
    h = big ? 100.0f : 56.0f;
    cx = f.x;
    cy = f.y + 25.0f - h / 2.0f;
    bool low = (f.crouching && (f.state == ACT_IDLE || f.state == ACT_GUARD || f.state == ACT_PARRY)) ||
        (f.state == ACT_ATTACK && IsLowSlot(f.move)) ||
        (f.state == ACT_COUNTER_STANCE && f.stanceLevel == 3);
    if (f.state == ACT_SLIP || f.state == ACT_DOWN) {
        w = big ? 100.0f : 50.0f; h = big ? 40.0f : 24.0f;
        cy = f.y + 25.0f - h / 2.0f;
    }
    else if (low) {
        h = h * 0.58f;
        cy = f.y + 25.0f - h / 2.0f;
    }
    if (f.charaIndex == 2 && f.state == ACT_ATTACK && InActiveFrames(f)) {
        // アナンダは伸ばした手足にもやられ判定
        if (f.move == MS_5J || f.move == MS_2J) { w = 130.0f; cx = f.x + (float)f.dir * 65.0f; }
        else if (f.move == MS_5K || f.move == MS_2K) { w = 160.0f; cx = f.x + (float)f.dir * 80.0f; }
    }
}

// ----------------------------------------------------
// ダメージ
// ----------------------------------------------------
static int CpuDamageRate() {
    if (!IsStoryMode()) return 100;
    static const int byDiff[3] = { 75, 100, 115 };
    return byDiff[g_slots[g_currentSlot].difficulty] + GetHardLevel() * 10;
}

static void ApplyDamage(Fighter& def, Fighter& atk, int dmg, int stun, float kx, float ky, int flags, bool blocked, int srcTag = -1) {
    if (&atk == &g_other) dmg = dmg * CpuDamageRate() / 100;
    if (def.wallBounceUsed) dmg = dmg * 7 / 10;

    int ai = FighterIndex(atk);
    int di = FighterIndex(def);

    if (blocked) {
        int chipDmg = dmg / 6;
        if (chipDmg < 3) chipDmg = 3;
        def.hp -= chipDmg;
        def.hitStop = atk.hitStop > 0 ? atk.hitStop - 1 : 5;
        def.vx = kx * 0.5f;
        int gs = stun * 6 / 10;
        if (gs < 8) gs = 8;
        if (def.guardStun < gs) def.guardStun = gs;
        g_stats[di].guards++;
        if (&def == &g_me) TutorialEvent(TE_GUARD, (flags & MF_LOW) ? 1 : 0);
        SpawnHitFX(def.x + (float)def.dir * 16.0f, def.y - 20, BLUE, 6);
        SpawnSparks(def.x + (float)def.dir * 16.0f, def.y - 20, CYAN, 3, 18.0f);
        if (srcTag < 0) RecordAdvantage(atk, def, def.guardStun, true, false);
        return;
    }

    if (def.armorFrames > 0 && !(flags & MF_UNBLOCK)) {
        def.hp -= dmg * 7 / 10;
        def.hitStop = 4;
        SpawnHitFX(def.x, def.y - 20, ORANGE, 12);
        SpawnSparks(def.x, def.y - 20, YELLOW, 4, 25.0f);
        SpawnPopup(def.x, def.y - 100, "アーマー!", ORANGE);
        return;
    }

    // カウンター：技の出始めを潰すと COUNTER、硬直に当てると PUNISH COUNTER
    int counterType = 0;
    if (def.state == ACT_ATTACK) {
        int st, act;
        GetMoveFrames(def, st, act);
        counterType = (def.animTimer < st + act) ? 1 : 2;
    }
    else if (def.state == ACT_THROW || def.state == ACT_TAUNT || def.state == ACT_BOAR_ROLL) {
        counterType = 2;
    }
    if (counterType > 0) {
        dmg = dmg * 12 / 10;
        stun += (counterType == 1) ? 6 : 10;
        SetCounterLabel(atk, counterType);
    }

    // コンボ補正：長いコンボほど1発の威力とのけぞりが下がる（永久コンボ防止）
    static const int SCALE[12] = { 100, 100, 90, 80, 70, 60, 55, 50, 45, 40, 35, 30 };
    dmg = dmg * SCALE[ClampI(def.comboTaken, 0, 11)] / 100;
    stun = stun * ClampI(100 - def.comboTaken * 4, 60, 100) / 100;

    def.hp -= dmg;
    def.hitStop = atk.hitStop > 0 ? atk.hitStop : 6;
    def.stateTimer = stun;
    def.animTimer = 0;
    def.vx = kx;
    def.vy = ky;
    def.stanceLevel = 0;
    def.guardStun = 0;
    def.crouching = false;
    def.comboTaken++;
    def.wallBounceReady = !def.wallBounceUsed && fabsf(kx) >= 6.5f;

    atk.connected = true;
    atk.comboCount++;
    atk.comboDisplayTimer = 60;

    g_stats[ai].hits++;
    g_stats[ai].damage += dmg;
    if (atk.comboCount > g_stats[ai].maxCombo) g_stats[ai].maxCombo = atk.comboCount;
    g_stats[di].taken++;

    int tag = (srcTag >= 0) ? srcTag : ((atk.state == ACT_ATTACK) ? TAGM(atk.move) : -1);
    if (&atk == &g_me) {
        TutorialEvent(TE_HIT, tag);
        TutorialEvent(TE_COMBO, atk.comboCount);
    }
    if (g_mode == MODE_TRAINING && &atk == &g_me) {
        g_comboDmg += dmg;
        g_comboHits++;
        if (tag >= 0) LogComboTag(tag);
        if (srcTag < 0 && atk.charaIndex == 3 && (atk.move == MS_5L || atk.move == MS_SUPER) &&
            (atk.diceFace == 6 || atk.boarSuper)) LogComboTag(TAG_DICE6);
    }

    int spMul = (atk.charaIndex == 0) ? 5 : 3;
    atk.spGauge += 4 + atk.comboCount * spMul; if (atk.spGauge > 100) atk.spGauge = 100;
    def.spGauge += 4;                           if (def.spGauge > 100) def.spGauge = 100;

    SpawnHitFX(def.x, def.y - 20, RED, 16);
    SpawnHitFX(def.x, def.y - 20, YELLOW, 12);
    SpawnSparks(def.x, def.y - 20, WHITE, 8, 35.0f);

    bool launch = (flags & (MF_LAUNCH | MF_SMASH)) != 0;
    if (launch) {
        TriggerScreenShake(8);
        SpawnSparks(def.x, def.y - 20, YELLOW, 12, 50.0f);
    }

    if (flags & MF_LAUNCH) {
        def.state = ACT_AIR_HIT;
        def.isGrounded = false;
    }
    else if (flags & MF_SMASH) {
        def.state = ACT_AIR_HIT;
    }
    else if (flags & MF_KNOCKDOWN) {
        def.state = ACT_AIR_HIT;
        def.isGrounded = false;
        if (def.vy > -4.0f) def.vy = -4.0f;
    }
    else {
        def.state = def.isGrounded ? ACT_HIT : ACT_AIR_HIT;
    }

    if (srcTag < 0) RecordAdvantage(atk, def, stun, false, launch || (flags & MF_KNOCKDOWN));
}

// たけしの当身成功：受けた技が大きいほど返しも大きい
static void TriggerCounter(Fighter& def, Fighter& atk, int incoming) {
    bool sup = (def.stanceLevel == 2);
    int dmg = (sup ? 300 : 190) + (int)((float)incoming * (sup ? 1.5f : 1.0f));

    if (&def == &g_me) LogComboTag(sup ? TAG_COUNTER_SUPER : TAG_COUNTER);
    g_stats[FighterIndex(def)].parries++;

    atk.armorFrames = 0;
    atk.invulFrames = 0;
    def.dir = (atk.x > def.x) ? 1 : -1;
    def.state = ACT_COUNTER_STRIKE;
    def.stateTimer = 30;
    def.animTimer = 0;
    def.invulFrames = 35;
    def.hasHit = true;
    def.stanceLevel = 0;
    def.spGauge += 25; if (def.spGauge > 100) def.spGauge = 100;
    def.hitStop = 0;
    atk.hitStop = 0;

    ApplyDamage(atk, def, dmg, 70, (float)def.dir * 9.0f, -17.0f, MF_LAUNCH, false, TAG_COUNTER);
    atk.hitStop = 22;
    def.hitStop = 22;
    SetLabel(def, MS_5L);

    g_flashTimer = 24;
    g_superCutinTimer = 55;
    g_superIsUltimate = false;
    g_cutinIsCounter = true;
    g_superCutinName = sup ? "【奥義】無防備の極み ―― 全部見えてたよ？" : "当身成功!!『……見えてたよ？』";
    TriggerScreenShake(25);
    SpawnHitFX(atk.x, atk.y - 20, CYAN, 40);
    SpawnHitFX(atk.x, atk.y - 20, WHITE, 30);
    SpawnSparks(atk.x, atk.y - 20, CYAN, 20, 70.0f);
    SpawnSparks(atk.x, atk.y - 20, WHITE, 16, 55.0f);
    SpawnPopup(def.x, def.y - 110, "COUNTER!!", CYAN);
}

static void DamageGimmick(Fighter& atk, int damage, float ax, float ay) {
    if (!g_gimmickEnabled || g_gimmick.fallen || g_gimmick.isFalling) return;
    if (fabsf(ax - g_gimmick.x) >= 50.0f || ay <= GROUND_Y - 140.0f) return;

    g_gimmick.hp -= damage;
    SpawnHitFX(g_gimmick.x, GROUND_Y - 60, YELLOW, 8);
    if (g_gimmick.hp > 0) return;

    g_gimmick.isFalling = true;
    g_gimmick.fallAngle = 0.0f;
    g_gimmick.activeHitBox = atk.dir;
    TriggerScreenShake(12);
    SpawnHitFX(g_gimmick.x, GROUND_Y - 40, ORANGE, 30);
    SpawnSparks(g_gimmick.x, GROUND_Y - 40, YELLOW, 10, 45.0f);
    if (g_currentStageIdx == 1) SpawnPopup(g_gimmick.x, GROUND_Y - 160, "感電コンテナ破壊!! 電流停止", CYAN);
    if (g_currentStageIdx == 2) {
        g_gimmick.itemActive = true;
        g_gimmick.itemX = g_gimmick.x;
        g_gimmick.itemY = GROUND_Y;
    }
}

// パリィ成功（攻撃側をピヨらせる）
static void ParrySuccess(Fighter& def, Fighter& atk) {
    bool master = (def.charaIndex == 8);
    atk.state = ACT_STUN;
    atk.stateTimer = master ? 75 : 42;
    atk.animTimer = 0;
    atk.hitStop = master ? 14 : 8;
    atk.vx = (float)-atk.dir * 4.5f;
    def.isCounter = true;
    def.spGauge += master ? 40 : 20; if (def.spGauge > 100) def.spGauge = 100;
    g_stats[FighterIndex(def)].parries++;
    if (&def == &g_me) {
        LogComboTag(TAG_PARRY);
        TutorialEvent(TE_PARRY, 0);
    }
    g_parryNoticeTimer = 35;
    TriggerScreenShake(master ? 12 : 6);
    SpawnHitFX(def.x, def.y - 20, CYAN, 25);
    SpawnSparks(def.x, def.y - 20, CYAN, 12, 40.0f);
    SpawnPopup(def.x, def.y - 110, "JUST PARRY!!", CYAN);
    if (master) {
        g_flashTimer = 14;
        g_superCutinTimer = 40;
        g_superIsUltimate = false;
        g_cutinIsCounter = true;
        g_superCutinName = "JUST PARRY・極 ――『ニヤリ』";
        SpawnSparks(def.x, def.y - 20, YELLOW, 16, 55.0f);
    }
}

static void CheckAttackHit(Fighter& atk, Fighter& def) {
    if (atk.state != ACT_ATTACK || atk.hasHit) return;
    if (!InActiveFrames(atk)) return;
    if (def.invulFrames > 0 || def.state == ACT_HELD || def.state == ACT_DOWN) return;

    AttackBox b;
    if (!GetAttackBox(atk, fabsf(atk.x - def.x), b)) return;
    int damage = b.damage;
    int hitStun = b.stun;

    float ax = atk.x + b.offX;
    float ay = atk.y + b.offY;
    DamageGimmick(atk, damage, ax, ay);

    float hcx, hcy, hw, hh;
    GetHurtBox(def, hcx, hcy, hw, hh);
    if (fabsf(ax - hcx) >= (b.w + hw) / 2.0f || fabsf(ay - hcy) >= (b.h + hh) / 2.0f) return;

    atk.hasHit = true;
    const MoveDef& m = CurMove(atk);
    int stop = 4 + m.rank * 2;
    atk.hitStop = stop;

    bool low = (b.flags & MF_LOW) != 0;
    bool over = (b.flags & MF_OVERHEAD) != 0 || !atk.isGrounded;

    // 当身（たけし）：構えの高さが合っていれば即反撃
    if (def.state == ACT_COUNTER_STANCE && def.animTimer >= 2) {
        bool ok = (def.stanceLevel == 2) || (def.stanceLevel == 1 && !low) || (def.stanceLevel == 3 && low);
        if (ok) { TriggerCounter(def, atk, damage); return; }
    }

    if (def.state == ACT_PARRY && def.parryWindow > 0) {
        ParrySuccess(def, atk);
        return;
    }

    bool blocked = false;
    if (def.state == ACT_GUARD) {
        if (b.flags & MF_UNBLOCK) SpawnPopup(def.x, def.y - 100, "ガード不能!!", RED);
        else if (low && !def.crouching) SpawnPopup(def.x, def.y - 100, "下段!", ORANGE);
        else if (over && def.crouching) SpawnPopup(def.x, def.y - 100, "中段!", ORANGE);
        else blocked = true;
    }

    if (!blocked && atk.isCounter) {
        damage = (int)((float)damage * ((atk.charaIndex == 8) ? 2.5f : 1.5f));
        hitStun += 10;
        atk.isCounter = false;
        SpawnSparks(def.x, def.y - 20, YELLOW, 8, 30.0f);
    }

    if (!blocked && def.hp <= damage && atk.charaIndex == 0 && atk.variant == 1) g_isKokusenKoFinish = true;

    ApplyDamage(def, atk, damage, hitStun, b.kx, b.ky, b.flags, blocked);
    if (atk.charaIndex == 3 && (atk.move == MS_5L || atk.move == MS_SUPER)) atk.vx *= 0.3f;
}

static void UpdateStageGimmick(int stageIdx) {
    if (!g_gimmickEnabled) {
        g_gimmick.hazardActive = false;
        g_gimmick.hazardWarning = false;
        return;
    }
    g_gimmick.stageTimer++;

    g_gimmick.hazardActive = false;
    g_gimmick.hazardWarning = false;
    if (stageIdx == 1 && !g_gimmick.fallen && !g_gimmick.isFalling) {
        int phase = g_gimmick.stageTimer % HAZARD_PERIOD;
        if (phase >= HAZARD_ACTIVE_START) {
            g_gimmick.hazardActive = true;
            Fighter* fList[2] = { &g_me, &g_other };
            for (int i = 0; i < 2; i++) {
                Fighter& f = *fList[i];
                if (!f.isGrounded || f.state == ACT_GUARD || f.invulFrames > 0 || InHazardSafeZone(f.x)) continue;
                f.hp -= 1;
                if (rand() % 12 == 0) SpawnHitFX(f.x, GROUND_Y, CYAN, 4);
            }
        }
        else if (phase >= HAZARD_WARN_START) {
            g_gimmick.hazardWarning = true;
            g_gimmick.hazardCountdown = (HAZARD_ACTIVE_START - phase + 59) / 60;
        }
    }

    if (g_gimmick.itemActive) {
        Fighter* fList[2] = { &g_me, &g_other };
        for (int i = 0; i < 2; i++) {
            if (fabsf(fList[i]->x - g_gimmick.itemX) < 30.0f) {
                fList[i]->hp += 180;
                if (fList[i]->hp > fList[i]->maxHp) fList[i]->hp = fList[i]->maxHp;
                SpawnHitFX(g_gimmick.itemX, g_gimmick.itemY - 20, GREEN, 20);
                g_gimmick.itemActive = false;
                break;
            }
        }
    }

    if (!g_gimmick.isFalling) return;

    g_gimmick.fallAngle += 0.06f;
    float tipX = g_gimmick.x + sinf(g_gimmick.fallAngle) * g_gimmick.h * (float)g_gimmick.activeHitBox;
    float tipY = GROUND_Y - cosf(g_gimmick.fallAngle) * g_gimmick.h;

    if (g_gimmick.fallAngle > 0.4f && g_gimmick.fallAngle < 1.4f) {
        Fighter* targets[2] = { &g_me, &g_other };
        for (int i = 0; i < 2; i++) {
            Fighter& f = *targets[i];
            if (f.invulFrames > 0 || f.state == ACT_HELD) continue;
            if (fabsf(f.x - tipX) < 45.0f && fabsf(f.y - tipY) < 40.0f) {
                f.hp -= 120;
                f.state = ACT_STUN;
                f.stateTimer = 65;
                f.animTimer = 0;
                f.hitStop = 10;
                f.vx = (float)g_gimmick.activeHitBox * 4.0f;
                TriggerScreenShake(14);
                SpawnHitFX(f.x, f.y - 20, YELLOW, 30);
                SpawnSparks(f.x, f.y - 20, ORANGE, 10, 45.0f);
            }
        }
    }

    if (g_gimmick.fallAngle >= 1.57f) {
        g_gimmick.isFalling = false;
        g_gimmick.fallen = true;
        TriggerScreenShake(10);
        SpawnHitFX(tipX, GROUND_Y, GRAY, 35);
        SpawnSparks(tipX, GROUND_Y, YELLOW, 8, 35.0f);
    }
}
