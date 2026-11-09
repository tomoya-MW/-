// ====================================================
// 物理・技の発動・投げ・プレイヤー操作・CPU
// ====================================================
#pragma once

// 壁バウンド：強い吹っ飛びで画面端に激突すると跳ね返り、1回だけ追撃できる
static void HitWall(Fighter& f, int pushDir) {
    if (IsHitState(f.state) && f.wallBounceReady && !f.wallBounceUsed && fabsf(f.vx) > 2.5f) {
        f.wallBounceUsed = true;
        f.wallBounceReady = false;
        f.state = ACT_AIR_HIT;
        f.isGrounded = false;
        f.vx = (float)pushDir * 3.5f;
        f.vy = -9.5f;
        if (f.stateTimer < 45) f.stateTimer = 45;
        f.hitStop = 10;
        TriggerScreenShake(12);
        SpawnHitFX(f.x - (float)pushDir * 10.0f, f.y - 25, WHITE, 25);
        SpawnSparks(f.x - (float)pushDir * 10.0f, f.y - 25, ORANGE, 14, 55.0f);
        SpawnPopup(f.x + (float)pushDir * 60.0f, f.y - 110, "WALL BOUNCE!! 追撃チャンス", ORANGE);
        if (&f == &g_other) LogComboTag(TAG_WALL);
    }
    else if (f.state == ACT_AIR_HIT) {
        f.vx = (float)pushDir * 4.0f;
    }
}

static bool UpdateFighterCommon(Fighter& f, Fighter& opp, bool isPlayer) {
    if (f.labelTimer > 0) f.labelTimer--;
    if (f.bufTimer > 0) f.bufTimer--;
    if (f.hitStop > 0) { f.hitStop--; return false; }

    if (f.armorFrames > 0) f.armorFrames--;
    if (f.parryWindow > 0) f.parryWindow--;
    if (f.invulFrames > 0) f.invulFrames--;
    if (f.burstCooldown > 0) f.burstCooldown--;
    if (f.diceShowTimer > 0) f.diceShowTimer--;
    if (f.guardStun > 0) f.guardStun--;
    if (f.techWindow > 0) f.techWindow--;
    if (f.floatTimer > 0) f.floatTimer--;

    if (f.comboDisplayTimer > 0) {
        f.comboDisplayTimer--;
        if (f.comboDisplayTimer == 0) f.comboCount = 0;
    }

    if ((f.state == ACT_IDLE || f.state == ACT_GUARD) && f.isGrounded) {
        f.dir = (opp.x > f.x) ? 1 : -1;
    }

    f.animTimer++;

    if (f.stateTimer > 0) {
        f.stateTimer--;
        if (f.stateTimer == 0 && f.state != ACT_IDLE) {
            f.state = ACT_IDLE;
            f.animTimer = 0;
            f.hasHit = false;
            f.connected = false;
            f.stanceLevel = 0;
            f.boarSuper = false;
            f.variant = 0;
            f.chainCount = 0;
        }
    }

    if (f.state == ACT_HELD) return true;   // 位置はつかんでいる側が決める

    if (isPlayer && g_currentStageIdx == 3 && !f.isGrounded) f.vx -= 0.15f;

    f.vy += GRAVITY;
    if (f.floatTimer > 0 && f.vy > 1.2f) f.vy = 1.2f;
    // 長い空中コンボほど落ちるのが速くなる
    if (f.state == ACT_AIR_HIT && f.comboTaken > 6) f.vy += 0.08f * (float)(f.comboTaken - 6);
    f.x += f.vx;
    f.y += f.vy;

    if (f.y >= GROUND_Y) {
        f.y = GROUND_Y;
        if (f.state == ACT_AIR_HIT && f.vy > 10.0f) {
            f.vy = -6.5f;
            TriggerScreenShake(6);
            SpawnHitFX(f.x, GROUND_Y, WHITE, 16);
            SpawnSparks(f.x, GROUND_Y, YELLOW, 6, 25.0f);
        }
        else {
            bool wasAir = !f.isGrounded;
            f.vy = 0;
            f.isGrounded = true;
            f.airDashUsed = false;
            f.doubleJumpUsed = false;
            f.floatTimer = 0;
            if (f.state == ACT_AIR_HIT) {
                // 吹っ飛びから着地したらダウン（起き上がるまで無敵）
                f.state = ACT_DOWN;
                f.stateTimer = 26;
                f.invulFrames = 30;
                f.animTimer = 0;
                f.vx *= 0.3f;
                TriggerScreenShake(4);
                SpawnHitFX(f.x, GROUND_Y + 10, GRAY, 8);
            }
            else if (wasAir && f.state == ACT_ATTACK && IsAirSlot(f.move)) {
                f.state = ACT_IDLE;   // 空中技は着地でキャンセル
                f.animTimer = 0;
                f.hasHit = false;
                f.variant = 0;
            }
        }
    }
    else {
        f.isGrounded = false;
    }

    if (f.x < 35.0f) { f.x = 35.0f; HitWall(f, 1); }
    if (f.x > 605.0f) { f.x = 605.0f; HitWall(f, -1); }

    if (f.isGrounded) f.vx *= 0.82f;

    if (!IsHitState(f.state)) {
        f.wallBounceUsed = false;
        f.wallBounceReady = false;
        if (f.state != ACT_STUN && f.state != ACT_SLIP) f.comboTaken = 0;
    }
    return true;
}

// 押し合い判定：体が重なったら左右に押し戻す（ジャンプで飛び越す・回避ですり抜けるのはOK）
static void ResolvePushboxes() {
    Fighter& a = g_me;
    Fighter& b = g_other;
    if (a.state == ACT_HELD || b.state == ACT_HELD || a.state == ACT_DODGE || b.state == ACT_DODGE) return;
    if (a.state == ACT_DOWN || b.state == ACT_DOWN) return;
    if (fabsf(a.y - b.y) > 45.0f) return;
    float half = ((a.charaIndex == 4) ? 28.0f : 16.0f) + ((b.charaIndex == 4) ? 28.0f : 16.0f);
    float dx = b.x - a.x;
    float overlap = half - fabsf(dx);
    if (overlap <= 0.0f) return;
    int side = (dx > 0.0f) ? 1 : ((dx < 0.0f) ? -1 : ((a.dir > 0) ? 1 : -1));
    float pushA = overlap * 0.5f, pushB = overlap * 0.5f;
    // 壁際では壁でない側だけが押される
    if (a.x - side * pushA < 35.0f || a.x - side * pushA > 605.0f) { pushB += pushA; pushA = 0; }
    if (b.x + side * pushB < 35.0f || b.x + side * pushB > 605.0f) { pushA += pushB; pushB = 0; }
    a.x -= side * pushA;
    b.x += side * pushB;
    if (a.x < 35.0f) a.x = 35.0f;
    if (a.x > 605.0f) a.x = 605.0f;
    if (b.x < 35.0f) b.x = 35.0f;
    if (b.x > 605.0f) b.x = 605.0f;
}

static void StartDash(Fighter& f, int moveDir) {
    f.state = ACT_DASH;
    f.moveDir = moveDir;
    f.animTimer = 0;
    f.hasHit = false;
    bool back = (moveDir != f.dir);
    f.stateTimer = back ? 12 : 14;
    if (back) f.invulFrames = 6;
    f.vx = (float)moveDir * CHARA_PROFILES[f.charaIndex].dashSpeed;
    SpawnHitFX(f.x - (float)moveDir * 12.0f, f.y + 15, GRAY, 5);
    if (&f == &g_me) TutorialEvent(TE_DASH, moveDir);
}

static void StartDodge(Fighter& f, int moveDir) {
    f.state = ACT_DODGE;
    f.moveDir = moveDir;
    f.stateTimer = 22;
    f.animTimer = 0;
    f.hasHit = false;
    f.crouching = false;
    f.invulFrames = 15;
    f.vx = (float)moveDir * 7.5f;
    SpawnHitFX(f.x, f.y + 10, WHITE, 6);
    if (&f == &g_me) TutorialEvent(TE_DODGE, moveDir);
}

// コンボ抜け（バースト）：被弾中にSP50を払って相手を弾き飛ばす
static bool TryBurst(Fighter& f, Fighter& opp) {
    if (f.spGauge < 50 || f.burstCooldown > 0) return false;
    f.spGauge -= 50;
    f.state = ACT_IDLE;
    f.stateTimer = 0;
    f.animTimer = 0;
    f.hitStop = 0;
    f.invulFrames = 30;
    f.burstCooldown = 240;
    f.comboTaken = 0;
    f.vx = 0;
    if (!f.isGrounded) f.vy = -4.0f;

    int push = (opp.x > f.x) ? 1 : -1;
    opp.state = ACT_AIR_HIT;
    opp.stateTimer = 28;
    opp.animTimer = 0;
    opp.hasHit = true;
    opp.vx = (float)push * 9.0f;
    opp.vy = -6.0f;
    opp.isGrounded = false;
    opp.comboCount = 0;
    opp.comboDisplayTimer = 0;
    opp.wallBounceReady = false;

    TriggerScreenShake(10);
    SpawnHitFX(f.x, f.y - 20, CYAN, 30);
    SpawnHitFX(f.x, f.y - 20, WHITE, 20);
    SpawnSparks(f.x, f.y - 20, CYAN, 16, 60.0f);
    SpawnPopup(f.x, f.y - 100, "BURST!! コンボ抜け", CYAN);
    if (&f == &g_me) TutorialEvent(TE_BURST, 0);
    return true;
}

static void RecordMoveStart(Fighter& f, int key) {
    int i = FighterIndex(f);
    g_stats[i].moves++;
    g_moveUse[i][ClampI(key, 0, MS_COUNT)]++;
}

// 技を出す（プレイヤー・CPU共通）
static bool StartMove(Fighter& f, Fighter& opp, int slot) {
    int c = f.charaIndex;
    if (slot < 0 || slot >= MS_COUNT) return false;
    const MoveDef& m = g_moves[c][slot];
    if (!m.valid) return false;
    if (IsAirSlot(slot) == f.isGrounded) return false;
    bool super = (slot == MS_SUPER || slot == MS_JSUPER);
    if (super && f.spGauge < 100) return false;

    bool chain = (f.state == ACT_ATTACK && m.rank == 1 && CurMove(f).rank == 1);
    bool prevJJConnected = (f.state == ACT_ATTACK && f.move == MS_JJ && f.connected);
    f.chainCount = chain ? f.chainCount + 1 : 0;

    f.state = ACT_ATTACK;
    f.move = slot;
    f.variant = 0;
    f.stateTimer = m.total;
    f.animTimer = 0;
    f.hasHit = false;
    f.connected = false;
    f.whiffChecked = false;
    f.launchCancelUsed = false;
    f.riseVy = m.rise;
    f.crouching = IsLowSlot(slot);
    if (m.vx != 0.0f && f.isGrounded) f.vx = (float)f.dir * m.vx;
    if (m.flags & MF_INVUL) f.invulFrames = m.startup + 3;
    if (m.armor > 0) f.armorFrames = m.armor;
    SetLabel(f, slot);

    if (super) {
        f.spGauge = 0;
        g_superCutinTimer = 45;
        g_superIsUltimate = false;
        g_cutinIsCounter = false;
        g_superCutinName = m.name;
        TriggerScreenShake(14);
        SpawnHitFX(f.x, f.y - 20, CHARA_PROFILES[c].specialColor, 30);
    }

    switch (m.behavior) {
    case BH_DRAGON:
        if ((float)f.hp / (float)f.maxHp < 0.25f) {
            f.variant = 1;
            f.vx = (float)f.dir * 7.5f;
            f.vy = -16.0f;
            f.isGrounded = false;
            f.stateTimer = 55;
            g_superCutinName = "=== DRAGON SUPLEX LIFT !!! ===";
        }
        break;
    case BH_KOKUSEN:
        if (prevJJConnected) {
            // 空中J命中からの黒閃（空間反転の大当たり演出）
            f.variant = 1;
            f.vx = (float)f.dir * 11.5f;
            f.vy = 12.0f;
            f.stateTimer = 55;
            g_superCutinTimer = 60;
            g_superIsUltimate = true;
            g_kokusenStrobe = 45;
            g_superCutinName = "【大当り】真・極点昇天破!! (確変突入)";
            TriggerScreenShake(22);
            SpawnSparks(f.x, f.y, RED, 30, 80.0f);
            SpawnSparks(f.x, f.y, YELLOW, 30, 80.0f);
        }
        else {
            f.vx = (float)f.dir * 9.5f;
            f.vy = 14.0f;
        }
        break;
    case BH_DIVE:
        f.vx = (float)f.dir * ((c == 3) ? 3.0f : 6.0f);
        f.vy = (c == 3) ? 14.0f : 16.0f;
        break;
    case BH_BOAR_DICE:
    case BH_BOAR_SUPER:
        f.state = ACT_BOAR_ROLL;
        f.stateTimer = 999;
        f.boarSuper = (m.behavior == BH_BOAR_SUPER);
        f.boarTiming = 0;
        f.vx = 0;
        break;
    case BH_COUNTER:
    case BH_COUNTER_LOW:
    case BH_COUNTER_SUPER:
        f.state = ACT_COUNTER_STANCE;
        f.stanceLevel = (m.behavior == BH_COUNTER) ? 1 : ((m.behavior == BH_COUNTER_LOW) ? 3 : 2);
        f.vx = 0;
        break;
    case BH_TAUNT:
        f.state = ACT_TAUNT;
        f.vx = 0;
        break;
    case BH_TELEPORT_FRONT:
        f.x = opp.x - (float)f.dir * 50.0f;
        SpawnHitFX(f.x, f.y - 20, YELLOW, 30);
        break;
    case BH_FLOAT:
        f.floatTimer = 70;
        if (f.vy > 0) f.vy = 0;
        SpawnHitFX(f.x, f.y, CYAN, 10);
        break;
    default:
        break;
    }

    RecordMoveStart(f, slot);
    if (f.state == ACT_ATTACK) RecordMoveInfo(f);
    return true;
}

static void StartThrow(Fighter& f) {
    f.state = ACT_THROW;
    f.stateTimer = 26;
    f.animTimer = 0;
    f.hasHit = false;
    f.crouching = false;
    f.vx = (float)f.dir * 1.5f;
    SetLabel(f, -1);
    RecordMoveStart(f, MS_COUNT);
}

// 投げの判定（出かかりの数フレームだけつかめる）
static void UpdateThrowAttempt(Fighter& atk, Fighter& def) {
    if (atk.state != ACT_THROW || atk.hasHit) return;
    if (atk.animTimer < 4 || atk.animTimer > 6) return;
    if (!def.isGrounded || def.invulFrames > 0 || IsHitState(def.state) || def.guardStun > 0) return;
    if (def.state == ACT_DOWN || def.state == ACT_HELD || def.state == ACT_THROW_HOLD) return;
    if (fabsf(atk.x - def.x) > 54.0f || fabsf(atk.y - def.y) > 30.0f) return;

    atk.hasHit = true;
    atk.state = ACT_THROW_HOLD;
    atk.stateTimer = 999;
    atk.animTimer = 0;
    atk.vx = 0;
    atk.throwDir = (&atk == &g_other && rand() % 3 == 0) ? -1 : 1;
    def.state = ACT_HELD;
    def.stateTimer = 999;
    def.animTimer = 0;
    def.vx = def.vy = 0;
    def.crouching = false;
    def.techWindow = 10;
    def.dir = -atk.dir;
    SpawnPopup(atk.x, atk.y - 110, "つかんだ!", YELLOW);
    SpawnHitFX(def.x, def.y - 20, WHITE, 10);
}

// 投げ抜け：つかまれた直後に投げボタン
static void ThrowTech(Fighter& holder, Fighter& held) {
    holder.state = ACT_HIT;
    holder.stateTimer = 14;
    holder.animTimer = 0;
    holder.vx = (float)-holder.dir * 6.0f;
    held.state = ACT_HIT;
    held.stateTimer = 14;
    held.animTimer = 0;
    held.vx = (float)-held.dir * 6.0f;
    held.y = GROUND_Y;
    held.techWindow = 0;
    TriggerScreenShake(6);
    SpawnSparks((holder.x + held.x) / 2, held.y - 25, WHITE, 12, 40.0f);
    SpawnPopup((holder.x + held.x) / 2, held.y - 110, "投げ抜け!", CYAN);
}

// つかんでいる間の演出と、投げ飛ばし
static void UpdateThrowHold(Fighter& atk, Fighter& def) {
    if (atk.state != ACT_THROW_HOLD) return;
    if (def.state != ACT_HELD) {   // 投げ抜けされた等
        atk.state = ACT_IDLE;
        atk.stateTimer = 0;
        return;
    }
    int t = atk.animTimer;
    float lift = (float)((t < 12) ? t : 12) * 1.5f;
    def.x = atk.x + (float)atk.dir * 30.0f;
    def.y = GROUND_Y - lift;
    def.dir = -atk.dir;
    if (t % 6 == 0) SpawnHitFX(def.x, def.y - 25, WHITE, 2);

    if (t >= 24) {
        bool back = (atk.throwDir < 0);
        int throwX = atk.dir * (back ? -1 : 1);
        if (back) def.x = atk.x - (float)atk.dir * 26.0f;
        def.state = ACT_HIT;           // ApplyDamage を通すためいったん地上やられに戻す
        def.isGrounded = false;
        def.y = GROUND_Y - 20.0f;
        def.techWindow = 0;
        atk.hitStop = 8;
        ApplyDamage(def, atk, 110, 40, (float)throwX * 7.0f, -10.0f, MF_LAUNCH, false, TAG_THROW);
        if (&atk == &g_me) LogComboTag(TAG_THROW);
        SetLabel(atk, back ? -2 : -1);
        TriggerScreenShake(12);
        SpawnSparks(def.x, def.y - 20, YELLOW, 14, 50.0f);
        atk.state = ACT_THROW;
        atk.stateTimer = 12;
        atk.animTimer = 99;   // つかみ判定はもう出ない
    }
}

// 状態ごとの毎フレーム処理（発生時の行動・空振り・サイコロ・ダッシュ等）
static void UpdateStateSpecials(Fighter& f, Fighter& opp) {
    int c = f.charaIndex;

    if (f.state == ACT_ATTACK) {
        const MoveDef& m = CurMove(f);
        int st, act;
        GetMoveFrames(f, st, act);

        // 発生フレームで跳び上がる（このフレームの判定はまだ地上で出る）
        if (f.riseVy != 0.0f && f.animTimer >= st) {
            f.vy = f.riseVy;
            f.isGrounded = false;
            f.riseVy = 0.0f;
            if (c == 0) TriggerScreenShake(6);
        }

        // 発生フレームで出る飛び道具・罠など
        if (f.animTimer == st) {
            float d = (float)f.dir;
            switch (m.behavior) {
            case BH_SHOT:
                FighterShot(f, f.x + d * 32, f.y - 16, d * 10.5f, 0, 60, CYAN, 12, 2);
                break;
            case BH_TRAP_DROP:
                FighterShot(f, f.x - d * 10, f.y - 20, -d * 1.5f, -3.0f, 0, ORANGE, 10, 4, f.trapCycle, 0.45f);
                f.trapCycle = (f.trapCycle + 1) % 3;
                f.vx = -d * 6.0f;
                break;
            case BH_TRAP_THROW:
                FighterShot(f, f.x + d * 20, f.y - 30, d * 6.5f, -7.5f, 40, ORANGE, 10, 4, f.trapCycle, 0.45f);
                f.trapCycle = (f.trapCycle + 1) % 3;
                break;
            case BH_TRAP_AIR:
                FighterShot(f, f.x, f.y - 10, d * 1.0f, 2.0f, 30, ORANGE, 10, 4, f.trapCycle, 0.45f);
                f.trapCycle = (f.trapCycle + 1) % 3;
                break;
            case BH_TRAP_SCATTER:
                for (int i = 0; i < 5; i++) {
                    FighterShot(f, f.x + d * 15, f.y - 30, d * (2.5f + i * 2.0f), -8.0f - i * 0.5f, 30, ORANGE, 10, 4, i % 3, 0.45f);
                }
                break;
            case BH_SMOKE:
                f.vx = -d * 9.0f;
                f.invulFrames = 14;
                SpawnHitFX(f.x, f.y - 20, GRAY, 25);
                SpawnPopup(f.x, f.y - 100, "ドロン!", GRAY);
                break;
            case BH_WATERMELON:
                FighterShot(f, f.x + d * 24, f.y - 30, d * 7.0f, -6.0f, 55, GREEN, 11, 5, 0, 0.35f);
                break;
            case BH_SHOCKWAVE:
                FighterShot(f, f.x + d * 40, f.y + 14, d * 7.0f, 0, 70, ORANGE, 12, 6, 0, 0.0f, true);
                TriggerScreenShake(10);
                SpawnHitFX(f.x, GROUND_Y + 10, ORANGE, 14);
                break;
            case BH_8WAY:
                for (int an = 0; an < 8; an++) {
                    float ang = (float)an * 3.14159f / 4.0f;
                    FighterShot(f, f.x, f.y - 20, cosf(ang) * 9.0f, sinf(ang) * 9.0f, 40, CYAN, 14, 2);
                }
                break;
            case BH_TELEPORT_BEHIND:
                f.x = opp.x - (float)opp.dir * 55.0f;
                if (f.x < 40.0f) f.x = 40.0f;
                if (f.x > 600.0f) f.x = 600.0f;
                f.dir = (opp.x > f.x) ? 1 : -1;
                SpawnHitFX(f.x, f.y - 20, CYAN, 25);
                break;
            default:
                break;
            }
        }

        // 空振りすると大スキ（大前など）
        if (m.whiff > 0 && !f.whiffChecked && f.animTimer >= st + act) {
            f.whiffChecked = true;
            if (!f.hasHit) {
                f.stateTimer += m.whiff;
                MoveInfo& mi = g_moveInfo[FighterIndex(f)];
                if (mi.valid && mi.slot == f.move) { mi.recovery += m.whiff; mi.total += m.whiff; }
                SpawnPopup(f.x, f.y - 110, "スカッ…（大スキ!!）", GRAY);
            }
        }

        // 多段ヒット
        if ((m.flags & MF_MULTI) && m.multi > 0 && f.animTimer > st && (f.animTimer - st) % m.multi == 0) f.hasHit = false;

        // 前進し続ける技
        if (m.vx != 0.0f && f.isGrounded && f.animTimer < st + act && !f.hasHit &&
            (m.flags & MF_MULTI || c == 3)) f.vx = (float)f.dir * m.vx;

        // いのしし突進
        if (c == 3 && (f.move == MS_5L || f.move == MS_SUPER)) {
            int face = f.boarSuper ? 6 : ClampI(f.diceFace, 3, 6);
            if (!f.hasHit && f.animTimer < act) f.vx = (float)f.dir * BOAR_FACES[face].speed;
            if (f.animTimer % 3 == 0) SpawnHitFX(f.x - (float)f.dir * 20.0f, f.y + 15, GRAY, 2);
        }
        return;
    }

    switch (f.state) {
    case ACT_BOAR_ROLL: {
        f.vx = 0;
        if (f.animTimer % 3 == 0) f.diceFace = 1 + rand() % 6;
        if (&f == &g_other && !g_is_net_battle && f.boarTiming == 0 && f.animTimer == BOAR_TIMING_START) {
            f.boarTiming = (rand() % 100 < 40) ? 1 : 2;
        }
        int rollLen = f.boarSuper ? 16 : BOAR_ROLL_FRAMES;
        if (f.animTimer >= rollLen) {
            int face;
            if (f.boarSuper) face = 6;
            else {
                face = 1 + rand() % 6;
                if (f.boarTiming == 1) {
                    int second = 1 + rand() % 6;   // 目押し成功：2回振って良い方
                    if (second > face) face = second;
                }
            }
            f.diceFace = face;
            f.diceShowTimer = 45;
            f.animTimer = 0;
            f.hasHit = false;
            f.connected = false;
            const BoarFace& bf = BOAR_FACES[face];
            if (face >= 3) {
                f.state = ACT_ATTACK;
                f.move = f.boarSuper ? MS_SUPER : MS_5L;
                f.stateTimer = (f.boarSuper ? 40 : bf.duration) + 8;
                f.vx = (float)f.dir * bf.speed;
                if (face >= 5) TriggerScreenShake(8);
                SetLabel(f, f.move);
                RecordMoveInfo(f);
            }
            else {
                f.state = ACT_SLIP;   // 失敗しても小転倒で済む
                f.stateTimer = (face == 1) ? 40 : 24;
                f.vx = (float)f.dir * ((face == 1) ? 3.0f : 1.5f);
            }
            SpawnPopup(f.x, f.y - 110, bf.label, (face >= 5) ? YELLOW : ((face >= 3) ? WHITE : GRAY));
        }
        break;
    }
    case ACT_DASH:
        if (f.animTimer < 10) {
            float spd = CHARA_PROFILES[c].dashSpeed * ((f.moveDir == f.dir) ? 1.0f : 0.85f);
            f.vx = (float)f.moveDir * spd;
        }
        break;
    case ACT_DODGE:
        if (f.animTimer < 16) f.vx = (float)f.moveDir * 7.5f;
        break;
    case ACT_TAUNT:
        if (f.animTimer == 2) SpawnPopup(f.x, f.y - 110, "ほらほら、打ってきなよ？", CYAN);
        if (f.animTimer == 40) {
            f.spGauge += 25; if (f.spGauge > 100) f.spGauge = 100;
            SpawnPopup(f.x, f.y - 90, "SP UP!", YELLOW);
            if (&f == &g_me && IsStoryMode()) {
                AddCpuTilt(2);
                SpawnPopup(opp.x, opp.y - 120, "ｲﾗｲﾗ…!!", RED);
            }
        }
        break;
    case ACT_THROW_HOLD:
        UpdateThrowHold(f, opp);
        break;
    default:
        break;
    }
}

// ----------------------------------------------------
// 先行入力とキャンセル
// ----------------------------------------------------
static int ResolveSlot(const Fighter& f, int btn, bool down, bool fwd) {
    int c = f.charaIndex;
    bool air = !f.isGrounded;
    int s = -1;
    switch (btn) {
    case BTN_J: s = air ? MS_JJ : (down ? MS_2J : MS_5J); break;
    case BTN_K: s = air ? MS_JK : (down ? MS_2K : MS_5K); break;
    case BTN_L:
        if (air) s = MS_JL;
        else if (down && g_moves[c][MS_2L].valid) s = MS_2L;
        else if (fwd && g_moves[c][MS_6L].valid) s = MS_6L;
        else s = MS_5L;
        break;
    case BTN_SUPER: s = air ? MS_JSUPER : MS_SUPER; break;
    default: break;
    }
    if (s < 0 || !g_moves[c][s].valid) return -1;
    return s;
}

static bool IsNeutral(const Fighter& f) {
    return f.state == ACT_IDLE || f.state == ACT_DASH;
}

// 今この技を出せるか（ニュートラル、またはキャンセル）
static bool CanStartNow(const Fighter& f, int slot) {
    if (IsNeutral(f)) return true;
    if (f.state != ACT_ATTACK || !f.hasHit) return false;
    const MoveDef& cur = CurMove(f);
    const MoveDef& next = g_moves[f.charaIndex][slot];
    if (cur.rank == 1 && next.rank == 1) return f.chainCount < 2;   // 弱は3回まで
    if (next.rank > cur.rank) return true;                           // 弱→強→必殺→超必殺
    // 打ち上げが当たったら空中技でキャンセルして追撃できる
    if ((cur.flags & MF_LAUNCH) && f.connected && !f.isGrounded && !f.launchCancelUsed && IsAirSlot(slot)) return true;
    return false;
}

static void BufferButtons(Fighter& f, const ControlState& cs) {
    unsigned short p = cs.pressed;
    int btn = BTN_NONE;
    if (p & IN_SUPER) btn = BTN_SUPER;
    else if (p & IN_THROW) btn = BTN_THROW;
    else if (p & IN_L) btn = BTN_L;
    else if (p & IN_K) btn = BTN_K;
    else if (p & IN_J) btn = BTN_J;
    if (btn == BTN_NONE) return;
    f.bufBtn = btn;
    f.bufTimer = 8;
    f.bufDown = (cs.held & IN_DOWN) != 0;
    f.bufFwd = (f.dir == 1) ? (cs.held & IN_RIGHT) != 0 : (cs.held & IN_LEFT) != 0;
}

// 先行入力された技を出せれば出す
static bool TryBufferedMove(Fighter& f, Fighter& opp) {
    if (f.bufTimer <= 0 || f.bufBtn == BTN_NONE) return false;
    if (f.bufBtn == BTN_THROW) {
        if (IsNeutral(f) && f.isGrounded) {
            f.bufTimer = 0;
            StartThrow(f);
            return true;
        }
        return false;
    }
    int slot = ResolveSlot(f, f.bufBtn, f.bufDown, f.bufFwd);
    if (slot < 0) { f.bufTimer = 0; return false; }
    if (!CanStartNow(f, slot)) return false;
    bool fromLaunch = (f.state == ACT_ATTACK && (CurMove(f).flags & MF_LAUNCH) && IsAirSlot(slot));
    if (StartMove(f, opp, slot)) {
        if (fromLaunch) f.launchCancelUsed = true;
        f.bufTimer = 0;
        return true;
    }
    return false;
}

// ----------------------------------------------------
// 操作されるファイター（ストーリー・トレーニング・通信のホスト側で共通）
// ----------------------------------------------------
static void UpdateControlledFighter(Fighter& f, Fighter& opp, ControlState& cs, unsigned short bits) {
    UpdateControl(cs, bits);
    BufferButtons(f, cs);
    if (!UpdateFighterCommon(f, opp, true)) return;
    if (g_readyFightTimer < 80) return;

    UpdateStateSpecials(f, opp);
    int c = f.charaIndex;
    bool hSpace = (cs.held & IN_SPACE) != 0;
    bool hLeft = (cs.held & IN_LEFT) != 0;
    bool hRight = (cs.held & IN_RIGHT) != 0;
    bool hDown = (cs.held & IN_DOWN) != 0;

    // つかまれた直後に投げボタンで投げ抜け
    if (f.state == ACT_HELD && f.techWindow > 0 && (cs.pressed & IN_THROW)) {
        ThrowTech(opp, f);
        return;
    }
    // つかんでいる間に後ろを押していれば裏投げ（相手と位置が入れ替わる）
    if (f.state == ACT_THROW_HOLD) {
        bool back = (f.dir == 1) ? hLeft : hRight;
        f.throwDir = back ? -1 : 1;
    }

    // コンボ抜け（被弾中に SPACE）
    if (IsHitState(f.state) && (cs.pressed & IN_SPACE)) {
        if (!TryBurst(f, opp)) {
            SpawnPopup(f.x, f.y - 100, (f.burstCooldown > 0) ? "バースト再使用まで待機" : "バーストにはSP50必要", GRAY);
        }
    }

    // いのしし：サイコロ回転中の目押し
    if (f.state == ACT_BOAR_ROLL && !f.boarSuper && f.boarTiming == 0 && (cs.pressed & (IN_J | IN_K | IN_L))) {
        bool good = (f.animTimer >= BOAR_TIMING_START && f.animTimer <= BOAR_TIMING_END);
        f.boarTiming = good ? 1 : 2;
        SpawnPopup(f.x, f.y - 130, good ? "目押し成功!!" : "ズレた…", good ? GREEN : GRAY);
        f.bufTimer = 0;
    }

    // 空中アクション：W で2段ジャンプ / S で急降下 / AA・DD で空中ダッシュ
    if (!f.isGrounded && f.state == ACT_IDLE) {
        if ((cs.pressed & IN_UP) && !f.doubleJumpUsed && c != 4) {
            f.vy = CHARA_PROFILES[c].jumpPower * 0.85f;
            f.doubleJumpUsed = true;
            SpawnHitFX(f.x, f.y + 10, WHITE, 8);
            if (&f == &g_me) TutorialEvent(TE_DOUBLE_JUMP, 0);
        }
        if ((cs.pressed & IN_DOWN) && f.vy > -6.0f) {
            f.vy = 17.0f;
            f.floatTimer = 0;
            SpawnHitFX(f.x, f.y - 30, WHITE, 6);
            if (&f == &g_me) TutorialEvent(TE_FAST_FALL, 0);
        }
        if (!f.airDashUsed && (cs.dbl & (IN_LEFT | IN_RIGHT))) {
            int md = (cs.dbl & IN_LEFT) ? -1 : 1;
            f.vx = (float)md * CHARA_PROFILES[c].dashSpeed * 0.9f;
            if (f.vy > -2.0f) f.vy = -2.0f;
            f.airDashUsed = true;
            SpawnHitFX(f.x - (float)md * 15.0f, f.y - 10, WHITE, 6);
        }
    }

    // 打ち上げが当たったら W で追いかけジャンプ
    if (f.state == ACT_ATTACK && (CurMove(f).flags & MF_LAUNCH) && f.connected && f.isGrounded &&
        (cs.pressed & IN_UP) && c != 4) {
        f.state = ACT_IDLE;
        f.stateTimer = 0;
        f.vy = CHARA_PROFILES[c].jumpPower * 1.1f;
        f.vx = (float)f.dir * 2.5f;
        f.isGrounded = false;
        f.hasHit = false;
        if (&f == &g_me) TutorialEvent(TE_JUMP, 0);
    }

    // パリィの受付が切れても SPACE を押し続けていればガードへ
    if (f.state == ACT_PARRY && f.parryWindow == 0 && hSpace && f.isGrounded) {
        f.state = ACT_GUARD;
        f.stateTimer = 0;
        f.animTimer = 0;
    }
    if (f.state == ACT_GUARD) {
        f.crouching = hDown;
        // ガード中に A/D で回避ロール
        if (f.guardStun == 0 && (cs.pressed & (IN_LEFT | IN_RIGHT))) {
            StartDodge(f, (cs.pressed & IN_LEFT) ? -1 : 1);
        }
        else if (f.stateTimer == 0 && f.guardStun == 0 && (!hSpace || !f.isGrounded)) {
            f.state = ACT_IDLE;
            f.animTimer = 0;
        }
    }

    // 地上ダッシュ（AA / DD）
    if (f.isGrounded && f.state == ACT_IDLE && !hDown && (cs.dbl & (IN_LEFT | IN_RIGHT))) {
        StartDash(f, (cs.dbl & IN_LEFT) ? -1 : 1);
    }

    // 先行入力・キャンセル
    if (TryBufferedMove(f, opp)) {
        CheckAttackHit(f, opp);
        return;
    }

    if (IsNeutral(f)) {
        bool dashing = (f.state == ACT_DASH);
        if (f.isGrounded && (cs.pressed & IN_SPACE)) {
            // タイミング押しでパリィ（たけしは受付が約3倍）
            f.state = ACT_PARRY;
            f.stateTimer = (c == 8) ? 22 : 16;
            f.animTimer = 0;
            f.parryWindow = (c == 8) ? 14 : 5;
            f.vx = 0;
            f.crouching = hDown;
        }
        else if (f.isGrounded && hSpace) {
            f.state = ACT_GUARD;   // 押しっぱなしはガード
            f.stateTimer = 0;
            f.animTimer = 0;
            f.vx = 0;
            f.crouching = hDown;
        }
        else {
            if (f.isGrounded && !dashing) f.crouching = hDown;
            if (!dashing && f.isGrounded && !f.crouching) {
                float speed = CHARA_PROFILES[c].walkSpeed;
                if (hLeft) f.vx = -speed;
                if (hRight) f.vx = speed;
            }
            if ((cs.held & IN_UP) && f.isGrounded && c != 4) {
                f.vy = CHARA_PROFILES[c].jumpPower;
                f.isGrounded = false;
                f.crouching = false;
                if (dashing) { f.state = ACT_IDLE; f.stateTimer = 0; f.animTimer = 0; }
                if (&f == &g_me) TutorialEvent(TE_JUMP, 0);
            }
        }
    }

    CheckAttackHit(f, opp);
    UpdateThrowAttempt(f, opp);
}

// ----------------------------------------------------
// CPU思考
// ----------------------------------------------------
static void CpuStart(int slot) {
    if (CanStartNow(g_other, slot)) StartMove(g_other, g_me, slot);
}

// プレイヤーが技を出した瞬間の反応（ガード・パリィ）。1回の攻撃につき1回だけ判定する
static void CpuReactToAttack(int level, int parryBonus) {
    static const int guardPct[9] = { 10, 25, 40, 52, 62, 72, 80, 86, 90 };
    static const int heightPct[9] = { 30, 45, 60, 72, 82, 90, 95, 98, 99 };
    static const int parryPct[9] = { 0, 0, 3, 6, 10, 14, 18, 22, 26 };
    Fighter& d = g_other;
    bool canBlock = d.isGrounded && (d.state == ACT_IDLE || d.state == ACT_DASH || (d.state == ACT_GUARD && d.guardStun == 0));
    if (!canBlock) return;
    float dist = fabsf(g_me.x - d.x);
    bool attackStarted = (g_me.state == ACT_ATTACK && g_me.animTimer == 1) || (g_me.state == ACT_THROW && g_me.animTimer == 1);
    if (!attackStarted || dist > 340.0f) return;
    if (g_me.state == ACT_THROW) return;   // 投げはガードできない

    const MoveDef& m = CurMove(g_me);
    if ((m.flags & MF_NOHITBOX) && m.behavior != BH_BOAR_DICE) return;
    int lv = ClampI(level - g_cpuTilt, 0, 8);
    if (rand() % 100 < ClampI(parryPct[lv] + parryBonus, 0, 40)) {
        d.state = ACT_PARRY;
        d.stateTimer = m.startup + 10;
        d.animTimer = 0;
        d.parryWindow = m.startup + 4;
        return;
    }
    if (rand() % 100 < guardPct[lv]) {
        bool low = (m.flags & MF_LOW) != 0;
        bool over = (m.flags & MF_OVERHEAD) != 0 || !g_me.isGrounded;
        bool correct = rand() % 100 < heightPct[lv];
        bool crouch = low ? correct : (over ? !correct : (rand() % 2 == 0));
        d.state = ACT_GUARD;
        d.stateTimer = m.total + 4;
        d.animTimer = 0;
        d.crouching = crouch;
        d.vx = 0;
    }
}

// 当たったら次の技へ繋ぐ（2周目以降・むずかしいのみ）
static void CpuContinueCombo(int level) {
    static const int comboPct[9] = { 0, 0, 0, 35, 50, 62, 72, 82, 90 };
    Fighter& d = g_other;
    if (d.state != ACT_ATTACK || !d.connected || !CpuMayCombo()) return;
    if (d.animTimer % 2 != 0 || rand() % 100 >= comboPct[level] / 4) return;
    const MoveDef& cur = CurMove(d);
    int c = d.charaIndex;
    int next = -1;
    if (cur.rank == 1 && d.chainCount < 1) next = d.move;
    else if (cur.rank == 1) next = d.isGrounded ? MS_5K : MS_JK;
    else if (cur.rank == 2) next = d.isGrounded ? (g_moves[c][MS_6L].valid ? MS_6L : MS_5L) : MS_JL;
    else if (cur.rank == 3 && d.spGauge >= 100 && level >= 5) next = d.isGrounded ? MS_SUPER : MS_JSUPER;
    if (next >= 0 && g_moves[c][next].valid && !(g_moves[c][next].flags & MF_NOHITBOX && next != MS_SUPER)) CpuStart(next);
}

static void CpuThink(int level) {
    static const int delay[9] = { 42, 34, 28, 22, 18, 14, 11, 9, 7 };
    Fighter& d = g_other;
    const AiPref& pref = AI_PREFS[d.charaIndex];
    int c = d.charaIndex;
    float dist = fabsf(g_me.x - d.x);
    float speed = CHARA_PROFILES[c].walkSpeed;
    int tilt = g_cpuTilt;

    if (d.state == ACT_GUARD && d.stateTimer == 0 && d.guardStun == 0) { d.state = ACT_IDLE; d.crouching = false; }
    if (!IsNeutral(d)) return;

    // 相手の空振り・転倒・挑発には差し込む
    bool playerOpen = (g_me.state == ACT_ATTACK && g_me.whiffChecked && !g_me.hasHit) ||
        g_me.state == ACT_SLIP || g_me.state == ACT_TAUNT || (g_me.state == ACT_THROW && !g_me.hasHit && g_me.animTimer > 8);
    if (playerOpen && level >= 2) {
        d.vx = (float)d.dir * speed * 2.0f;
        if (dist <= 80.0f) CpuStart(MS_5K);
        return;
    }

    // 周回が進むと、大技を見てから回避する
    if (level >= 4 && g_me.state == ACT_ATTACK && g_me.animTimer <= 2 && CurMove(g_me).rank >= 3 &&
        dist < 300.0f && rand() % 10 < level - 3) {
        StartDodge(d, (level >= 6 && rand() % 2 == 0) ? d.dir : -d.dir);
        return;
    }

    if (g_cpuThinkTimer > 0) {
        // 考え中は間合いを調整するだけ
        if (pref.keepAway && dist < pref.prefDist - 40.0f) d.vx = (float)-d.dir * speed;
        else if (dist > pref.prefDist + 20.0f) d.vx = (float)d.dir * speed;
        return;
    }
    g_cpuThinkTimer = delay[level] + rand() % 12 - tilt * 2;

    // 対空
    if (!g_me.isGrounded && dist < 110.0f && pref.antiAir >= 0 && level >= 2 && rand() % 10 < 3 + level / 2) {
        CpuStart(pref.antiAir);
        return;
    }
    // 超必殺
    if (d.spGauge >= 100 && level >= 2 && dist < 100.0f && rand() % 100 < 8 + level * 2) {
        CpuStart(MS_SUPER);
        return;
    }

    if (dist < 75.0f) {
        int r = rand() % 100;
        if (level >= 2 && g_me.state == ACT_GUARD && r < 30) { StartThrow(d); return; }
        if (r < 35 + tilt * 3) CpuStart(MS_5J);
        else if (r < 55) CpuStart(MS_2J);
        else if (r < 70 + tilt * 4) CpuStart(MS_5K);
        else if (r < 82 && level >= 1) CpuStart(MS_2K);
        else if (level >= 2 && r < 92) StartThrow(d);
        else d.vx = (float)-d.dir * speed;
    }
    else if (dist < pref.prefDist + 50.0f) {
        int r = rand() % 100;
        if (r < 35 && pref.midSlot >= 0) CpuStart(pref.midSlot);
        else if (r < 50 && g_moves[c][MS_2K].valid) CpuStart(MS_2K);
        else if (pref.keepAway) d.vx = (float)-d.dir * speed;
        else if (level >= 3 && r < 62 && c != 4) {
            d.vy = CHARA_PROFILES[c].jumpPower;   // 飛び込み
            d.vx = (float)d.dir * speed;
            d.isGrounded = false;
        }
        else d.vx = (float)d.dir * speed;
    }
    else {
        if (pref.farSlot >= 0 && rand() % 100 < 30 + level * 4) CpuStart(pref.farSlot);
        else if (level >= 3 && rand() % 3 == 0) StartDash(d, d.dir);
        else d.vx = (float)d.dir * speed;
    }
}

// 空中にいるCPUの攻撃
static void CpuAirAttack(int level) {
    Fighter& d = g_other;
    if (d.isGrounded || d.state != ACT_IDLE || d.vy < 0) return;
    float dx = fabsf(g_me.x - d.x);
    if (dx < 70.0f && d.y > g_me.y - 90.0f && rand() % 6 == 0) CpuStart(rand() % 2 ? MS_JK : MS_JJ);
    (void)level;
}

static void UpdateCpu() {
    if (!UpdateFighterCommon(g_other, g_me, false)) return;

    int stage = ClampI(g_slots[g_currentSlot].stageProgress, 0, LAST_STAGE);
    int level = GetAiLevel();
    int parryBonus = IsStoryMode() ? CPU_ROSTER[stage].aiParryRate / 2 : 0;

    if (g_cpuTilt > 0 && --g_cpuTiltTimer <= 0) {
        g_cpuTilt--;
        g_cpuTiltTimer = 300;
    }
    if (g_cpuThinkTimer > 0) g_cpuThinkTimer--;
    if (g_readyFightTimer < 80) return;

    UpdateStateSpecials(g_other, g_me);

    // 投げ抜け
    if (g_other.state == ACT_HELD && g_other.techWindow == 6 && rand() % 100 < level * 9) {
        ThrowTech(g_me, g_other);
        return;
    }

    // コンボ抜け（2周目以降）
    if (IsHitState(g_other.state) && GetCurrentLap() >= 2 && g_other.comboTaken >= 3 &&
        g_other.spGauge >= 50 && rand() % 100 < 4 + level * 3) {
        TryBurst(g_other, g_me);
    }

    CpuReactToAttack(level, parryBonus);
    CpuContinueCombo(level);
    CpuAirAttack(level);
    CpuThink(level);

    CheckAttackHit(g_other, g_me);
    UpdateThrowAttempt(g_other, g_me);
}

// トレーニングのダミー（7:CPU行動 以外は攻撃してこない）
static void UpdateDummy() {
    Fighter& d = g_other;
    if (!UpdateFighterCommon(d, g_me, false)) return;
    UpdateStateSpecials(d, g_me);

    bool freeGuard = (d.state == ACT_GUARD && d.stateTimer == 0 && d.guardStun == 0);
    const MoveDef& pm = CurMove(g_me);
    bool playerLow = g_me.state == ACT_ATTACK && (pm.flags & MF_LOW);
    bool playerOver = g_me.state == ACT_ATTACK && ((pm.flags & MF_OVERHEAD) || !g_me.isGrounded);

    switch (g_dummyMode) {
    case DUMMY_STOP:
        if (freeGuard) d.state = ACT_IDLE;
        d.crouching = false;
        break;
    case DUMMY_STAND_GUARD:
    case DUMMY_CROUCH_GUARD:
    case DUMMY_AUTO_GUARD:
        if (d.state == ACT_IDLE && d.isGrounded) {
            d.state = ACT_GUARD;
            d.stateTimer = 0;
        }
        if (d.state == ACT_GUARD) {
            if (g_dummyMode == DUMMY_STAND_GUARD) d.crouching = false;
            else if (g_dummyMode == DUMMY_CROUCH_GUARD) d.crouching = true;
            else if (playerLow) d.crouching = true;
            else if (playerOver) d.crouching = false;
        }
        break;
    case DUMMY_RANDOM_JUMP:
        if (freeGuard) d.state = ACT_IDLE;
        d.crouching = false;
        if (d.state == ACT_IDLE && d.isGrounded && d.charaIndex != 4 && rand() % 40 == 0) {
            d.vy = CHARA_PROFILES[d.charaIndex].jumpPower;
            d.isGrounded = false;
        }
        break;
    case DUMMY_RANDOM_GUARD:
        if (freeGuard) { d.state = ACT_IDLE; d.crouching = false; }
        if (d.state == ACT_IDLE && d.isGrounded && g_me.state == ACT_ATTACK && g_me.animTimer == 0 && rand() % 2 == 0) {
            d.state = ACT_GUARD;
            d.stateTimer = 30;
            d.crouching = (rand() % 2 == 0);
        }
        break;
    case DUMMY_CPU:
        CpuReactToAttack(4, 0);
        CpuContinueCombo(4);
        CpuAirAttack(4);
        if (g_cpuThinkTimer > 0) g_cpuThinkTimer--;
        CpuThink(4);
        CheckAttackHit(d, g_me);
        UpdateThrowAttempt(d, g_me);
        break;
    default:
        break;
    }
}

// ----------------------------------------------------
// エフェクト・弾の更新
// ----------------------------------------------------
static void UpdateEffects() {
    if (g_superCutinTimer > 0) g_superCutinTimer--;
    if (g_kokusenStrobe > 0) g_kokusenStrobe--;
    if (g_flashTimer > 0) g_flashTimer--;
    for (int i = 0; i < 2; i++) if (g_counterLabelTimer[i] > 0) g_counterLabelTimer[i]--;

    if (g_screenShakeTimer > 0) {
        g_screenShakeTimer--;
        g_shakeOffsetX = (rand() % 7) - 3;
        g_shakeOffsetY = (rand() % 7) - 3;
    }
    else {
        g_shakeOffsetX = 0;
        g_shakeOffsetY = 0;
    }

    for (int i = 0; i < MAX_FX; i++) {
        if (!g_fx[i].active) continue;
        g_fx[i].x += g_fx[i].vx;
        g_fx[i].y += g_fx[i].vy;
        if (--g_fx[i].life <= 0) g_fx[i].active = false;
    }
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (!g_sparks[i].active) continue;
        g_sparks[i].length *= 0.88f;
        if (--g_sparks[i].life <= 0) g_sparks[i].active = false;
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!g_popups[i].active) continue;
        g_popups[i].y -= 0.6f;
        if (--g_popups[i].life <= 0) g_popups[i].active = false;
    }
}

static void UpdateShots() {
    for (int i = 0; i < MAX_SHOTS; i++) {
        Projectile& p = g_shots[i];
        if (!p.active) continue;
        p.x += p.vx;
        p.y += p.vy;
        p.vy += p.gravity;
        p.life--;
        p.angle += 0.35f;

        // 罠は着地点に設置、スイカは割れて消える
        if ((p.projType == 4 || p.projType == 5) && p.y >= GROUND_Y + 10.0f) {
            if (p.projType == 4) PlaceTrap(p.isFromMe, p.subType, p.x);
            else SpawnHitFX(p.x, GROUND_Y + 10, RED, 12);
            p.active = false;
            continue;
        }

        Fighter& def = p.isFromMe ? g_other : g_me;
        Fighter& atk = p.isFromMe ? g_me : g_other;

        float hcx, hcy, hw, hh;
        GetHurtBox(def, hcx, hcy, hw, hh);

        if (p.damage > 0 && def.invulFrames == 0 && def.state != ACT_HELD && def.state != ACT_DOWN &&
            fabsf(p.x - hcx) < (hw / 2.0f + p.radius) && fabsf(p.y - hcy) < hh / 2.0f + p.radius) {
            bool reflect = (def.state == ACT_PARRY && def.parryWindow > 0) ||
                (def.state == ACT_COUNTER_STANCE && def.stanceLevel == 2);
            if (reflect) {
                p.vx *= -1.3f;
                p.isFromMe = !p.isFromMe;
                def.spGauge += 20; if (def.spGauge > 100) def.spGauge = 100;
                g_stats[FighterIndex(def)].parries++;
                if (&def == &g_me) {
                    LogComboTag(def.state == ACT_PARRY ? TAG_PARRY : TAG_COUNTER_SUPER);
                    if (def.state == ACT_PARRY) TutorialEvent(TE_PARRY, 0);
                }
                SpawnHitFX(def.x, def.y - 20, CYAN, 20);
                SpawnSparks(def.x, def.y - 20, CYAN, 10, 35.0f);
                SpawnPopup(def.x, def.y - 100, (def.state == ACT_PARRY) ? "JUST PARRY!!" : "見切り!", CYAN);
                continue;
            }
            bool blocked = (def.state == ACT_GUARD) && (!p.low || def.crouching);
            if (def.state == ACT_GUARD && !blocked) SpawnPopup(def.x, def.y - 100, "下段!", ORANGE);
            atk.hitStop = 0;
            ApplyDamage(def, atk, p.damage, 22, (p.vx > 0 ? 1.0f : -1.0f) * 5.0f, 0, 0, blocked, TAG_SHOT);
            p.active = false;
            continue;
        }

        if (p.life <= 0 || p.x < 10 || p.x > 630) p.active = false;
    }
}
