// ====================================================
// ワールド描画（カメラでズームされる側）
//   g_worldScreen に描き、最後にカメラの範囲を拡大して画面に貼る
// ====================================================
#pragma once

static const float CAM_MAX_ZOOM = 2.3f;

// 2人の位置と技の届く範囲から、ズームと表示範囲を決める
static void UpdateCamera(bool instant) {
    float left = (g_me.x < g_other.x ? g_me.x : g_other.x) - 70.0f;
    float right = (g_me.x > g_other.x ? g_me.x : g_other.x) + 70.0f;
    float top = (g_me.y < g_other.y ? g_me.y : g_other.y) - 110.0f;
    if (g_me.charaIndex == 4 || g_other.charaIndex == 4) top -= 60.0f;
    const Fighter* fs[2] = { &g_me, &g_other };
    for (int i = 0; i < 2; i++) {
        const Fighter& f = *fs[i];
        if (f.state != ACT_ATTACK) continue;
        const MoveDef& m = CurMove(f);
        if (!m.valid || (m.flags & MF_NOHITBOX) || m.w > 600) continue;
        float reach = f.x + (float)f.dir * (m.offX + m.w / 2);
        if (reach < left) left = reach - 20.0f;
        if (reach > right) right = reach + 20.0f;
    }
    float bottom = GROUND_Y + FLOOR_DRAW_OFF + 40.0f;
    float zw = 640.0f / (right - left);
    float zh = 480.0f / (bottom - top + 60.0f);
    float target = (zw < zh) ? zw : zh;
    if (target < 1.0f) target = 1.0f;
    if (target > CAM_MAX_ZOOM) target = CAM_MAX_ZOOM;

    g_camZoom = instant ? target : g_camZoom + (target - g_camZoom) * 0.10f;
    float vw = 640.0f / g_camZoom;
    float vh = 480.0f / g_camZoom;
    float cx = (left + right) / 2.0f;
    float tx = cx - vw / 2.0f;
    // 床の線が画面の下から 70px のところに来るようにし、ジャンプした人が見切れないよう上にずらす
    float ty = (GROUND_Y + FLOOR_DRAW_OFF) - 410.0f / g_camZoom;
    if (top - ty < 60.0f / g_camZoom) ty = top - 60.0f / g_camZoom;
    if (tx < 0) tx = 0;
    if (tx > 640.0f - vw) tx = 640.0f - vw;
    if (ty < 0) ty = 0;
    if (ty > 480.0f - vh) ty = 480.0f - vh;
    g_camX = instant ? tx : g_camX + (tx - g_camX) * 0.18f;
    g_camY = instant ? ty : g_camY + (ty - g_camY) * 0.18f;
    if (g_camX < 0) g_camX = 0;
    if (g_camY < 0) g_camY = 0;
}

static void WorldToScreen(float wx, float wy, int& sx, int& sy) {
    sx = (int)((wx + g_shakeOffsetX - g_camX) * g_camZoom);
    sy = (int)((wy + g_shakeOffsetY - g_camY) * g_camZoom);
}

static void BeginWorldDraw() {
    SetDrawScreen(g_worldScreen);
    ClearDrawScreen();
}

static void EndWorldDraw() {
    SetDrawScreen(DX_SCREEN_BACK);
    ClearDrawScreen();
    float vw = 640.0f / g_camZoom;
    float vh = 480.0f / g_camZoom;
    SetDrawMode(DX_DRAWMODE_NEAREST);
    DrawRectExtendGraph(0, 0, 640, 480, (int)g_camX, (int)g_camY, (int)vw, (int)vh, g_worldScreen, FALSE);
}

static void DrawStageBackground(int stageIdx) {
    int oy = g_shakeOffsetY;
    if (g_kokusenStrobe > 0) {
        DrawBox(0, 0, 640, 480, (g_kokusenStrobe % 4 < 2) ? GetColor(0, 0, 0) : GetColor(180, 10, 10), TRUE);
        DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, GetColor(10, 10, 10), TRUE);
        DrawLine(0, (int)GROUND_Y + 25 + oy, 640, (int)GROUND_Y + 25 + oy, RED);
        return;
    }
    unsigned int sky = STAGES[stageIdx].skyColor;
    DrawBox(0, 0, 640, 480, sky, TRUE);
    // 遠景のシルエット（簡易）
    int r, g, b;
    GetColor2(sky, &r, &g, &b);
    unsigned int far1 = GetColor(r + 12 > 255 ? 255 : r + 12, g + 12 > 255 ? 255 : g + 12, b + 16 > 255 ? 255 : b + 16);
    for (int i = 0; i < 8; i++) {
        int x = i * 84 - 20;
        int h = 60 + ((i * 37) % 70);
        DrawBox(x, (int)GROUND_Y + 25 - h + oy, x + 70, (int)GROUND_Y + 25 + oy, far1, TRUE);
    }
    DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, STAGES[stageIdx].groundColor, TRUE);
    DrawLine(0, (int)GROUND_Y + 25 + oy, 640, (int)GROUND_Y + 25 + oy, WHITE);
}

static void DrawTrapIcon(int type, int x, int y) {
    if (type == TRAP_BANANA) {
        DrawTriangle(x - 13, y, x - 3, y - 9, x, y, YELLOW, TRUE);
        DrawTriangle(x + 13, y, x + 3, y - 9, x, y, YELLOW, TRUE);
        DrawTriangle(x - 3, y, x, y - 13, x + 3, y, YELLOW, TRUE);
        DrawCircle(x, y - 2, 3, GetColor(110, 80, 20), TRUE);
    }
    else if (type == TRAP_LEGO) {
        DrawBox(x - 10, y - 9, x + 10, y, RED, TRUE);
        DrawBox(x - 8, y - 13, x - 3, y - 9, RED, TRUE);
        DrawBox(x + 3, y - 13, x + 8, y - 9, RED, TRUE);
        DrawBox(x - 10, y - 9, x + 10, y, WHITE, FALSE);
    }
    else {
        DrawTriangle(x, y - 24, x - 9, y - 2, x + 9, y - 2, ORANGE, TRUE);
        DrawLine(x - 5, y - 12, x + 5, y - 12, WHITE, 3);
        DrawBox(x - 12, y - 3, x + 12, y, ORANGE, TRUE);
    }
}

static void DrawTraps() {
    for (int i = 0; i < MAX_TRAPS; i++) {
        const Trap& t = g_traps[i];
        if (!t.active) continue;
        if ((t.armDelay > 0 || t.life < 120) && (g_frameCount / 4) % 2 == 0) continue;
        DrawTrapIcon(t.type, (int)t.x + g_shakeOffsetX, (int)GROUND_Y + 24 + g_shakeOffsetY);
    }
}

static void DrawHazardFloor() {
    if (!g_gimmick.hazardWarning && !g_gimmick.hazardActive) return;
    int ox = g_shakeOffsetX;
    int floorY = (int)GROUND_Y + 22 + g_shakeOffsetY;
    int sx1 = (int)(g_gimmick.x - HAZARD_SAFE_HALF) + ox;
    int sx2 = (int)(g_gimmick.x + HAZARD_SAFE_HALF) + ox;
    if (g_gimmick.hazardWarning) {
        if ((g_frameCount / 8) % 2 == 0) {
            DrawBox(0, floorY, sx1, floorY + 4, YELLOW, TRUE);
            DrawBox(sx2, floorY, 640, floorY + 4, YELLOW, TRUE);
            for (int x = 10; x < 640; x += 40) {
                if (x > sx1 - 10 && x < sx2 + 10) continue;
                DrawTriangle(x, floorY - 14, x - 8, floorY - 2, x + 8, floorY - 2, YELLOW, FALSE);
            }
        }
    }
    else {
        DrawBox(0, floorY, sx1, floorY + 4, CYAN, TRUE);
        DrawBox(sx2, floorY, 640, floorY + 4, CYAN, TRUE);
        for (int e = 0; e < 10; e++) {
            int rx = rand() % 640;
            if (rx > sx1 && rx < sx2) continue;
            DrawLine(rx, floorY - 2, rx + (rand() % 16 - 8), floorY - 10, YELLOW);
        }
    }
    DrawBox(sx1, floorY - 2, sx2, floorY + 4, GREEN, TRUE);
}

static void DrawGimmick(int stageIdx) {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;
    DrawTraps();
    if (!g_gimmickEnabled) return;
    if (stageIdx == 1) DrawHazardFloor();

    if (g_gimmick.itemActive) {
        int ix = (int)g_gimmick.itemX + ox;
        int iy = (int)g_gimmick.itemY - 12 + oy;
        DrawTriangle(ix, iy - 10, ix - 12, iy + 10, ix + 12, iy + 10, WHITE, TRUE);
        DrawBox(ix - 5, iy + 2, ix + 5, iy + 10, GetColor(20, 20, 20), TRUE);
    }
    if (g_gimmick.fallen) {
        int x1 = (int)g_gimmick.x + ox;
        int x2 = (int)(g_gimmick.x + g_gimmick.h * (float)g_gimmick.activeHitBox) + ox;
        DrawBox(min(x1, x2), (int)GROUND_Y + 15 + oy, max(x1, x2), (int)GROUND_Y + 25 + oy, GRAY, TRUE);
        return;
    }
    if (g_gimmick.isFalling) {
        float tipX = g_gimmick.x + sinf(g_gimmick.fallAngle) * g_gimmick.h * (float)g_gimmick.activeHitBox;
        float tipY = GROUND_Y - cosf(g_gimmick.fallAngle) * g_gimmick.h;
        DrawLine((int)g_gimmick.x + ox, (int)GROUND_Y + 25 + oy, (int)tipX + ox, (int)tipY + oy, YELLOW, 6);
        DrawCircle((int)tipX + ox, (int)tipY + oy, 15, RED, TRUE);
        return;
    }
    int x = (int)g_gimmick.x + ox;
    int y = (int)GROUND_Y + 25 + oy;
    unsigned int col = (g_gimmick.hp < g_gimmick.maxHp / 2) ? RED : GRAY;
    DrawBox(x - 17, y - 130, x + 17, y, col, TRUE);
    DrawBox(x - 17, y - 130, x + 17, y, WHITE, FALSE);
    if (g_gimmick.hp < g_gimmick.maxHp) {
        DrawLine(x - 8, y - 100, x + 5, y - 70, YELLOW);
        DrawLine(x + 5, y - 70, x - 6, y - 30, YELLOW);
    }
}

// ----------------------------------------------------
// キャラ
// ----------------------------------------------------
static int GetPose(const Fighter& f) {
    bool winner = (g_koWinner == 1 && &f == &g_me) || (g_koWinner == 2 && &f == &g_other);
    switch (f.state) {
    case ACT_IDLE:
        if (winner && f.isGrounded) return PO_WIN;
        if (!f.isGrounded) return PO_JUMP;
        return f.crouching ? PO_CROUCH : PO_IDLE;
    case ACT_GUARD:
    case ACT_PARRY:
        return f.crouching ? PO_CGUARD : PO_GUARD;
    case ACT_ATTACK:
        return CurMove(f).pose;
    case ACT_THROW:
    case ACT_THROW_HOLD:
        return PO_THROW;
    case ACT_HELD:
        return PO_HELD;
    case ACT_HIT:
    case ACT_AIR_HIT:
        return PO_HIT;
    case ACT_DOWN:
    case ACT_SLIP:
        return PO_DOWN;
    case ACT_STUN:
        return PO_STUN;
    case ACT_DASH:
        return (f.moveDir == f.dir) ? PO_DASH : PO_JUMP;
    case ACT_DODGE:
        return PO_ROLL;
    case ACT_BOAR_ROLL:
        return PO_CROUCH;
    case ACT_COUNTER_STANCE:
        return (f.stanceLevel == 3) ? PO_CGUARD : (f.stanceLevel == 2 ? PO_SUPER : PO_GUARD);
    case ACT_COUNTER_STRIKE:
        return PO_SPECIAL;
    case ACT_TAUNT:
        return PO_WIN;
    default:
        return PO_IDLE;
    }
}

static int FighterHeight(const Fighter& f) {
    return (f.charaIndex == 4) ? 128 : 64;
}

// 大前の物干し竿（ang: 0で正面水平、マイナスで上向き）
static void DrawPole(int sx, int sy, int dir, float ang, int len, bool glow) {
    int tx = sx + (int)((float)dir * cosf(ang) * (float)len);
    int ty = sy + (int)(sinf(ang) * (float)len);
    DrawLine(sx, sy, tx, ty, GetColor(110, 115, 130), 9);
    DrawLine(sx, sy, tx, ty, glow ? YELLOW : GetColor(215, 220, 230), 3);
    DrawCircle(tx, ty, 7, GetColor(190, 195, 205), TRUE);
}

static void DrawMegatonPole(const Fighter& f, int bx, int floorY) {
    int sx = bx + f.dir * 30, sy = floorY - 78;
    if (f.state != ACT_ATTACK) {
        if (f.state == ACT_IDLE || f.state == ACT_GUARD || f.state == ACT_DASH)
            DrawPole(bx - f.dir * 20, floorY - 10, f.dir, -1.25f, 150, false);
        return;
    }
    int st, act;
    GetMoveFrames(f, st, act);
    float t = (st > 0) ? (float)f.animTimer / (float)st : 1.0f;
    if (t > 1.0f) t = 1.0f;
    bool on = InActiveFrames(f);
    switch (f.move) {
    case MS_5K:
        if (f.animTimer < st) DrawPole(sx, sy, f.dir, -1.9f + 1.75f * t, 300, false);
        else if (on) {
            DrawPole(sx, sy, f.dir, 0.0f, 300, true);
            for (int k = 1; k <= 3; k++) DrawLine(sx, sy - 10 * k, sx + f.dir * 300, sy - 4 * k, ORANGE);
        }
        else DrawPole(sx, sy + 10, f.dir, 0.25f, 280, false);
        break;
    case MS_2K:
        DrawPole(sx, floorY - 40, f.dir, (f.animTimer < st) ? -1.2f + 1.3f * t : 0.18f, 230, on);
        break;
    case MS_5L:
        if (f.animTimer < st) DrawPole(sx, sy, f.dir, -1.57f - 0.35f * t, 190, false);
        else DrawPole(sx, sy, f.dir, on ? 0.55f : 0.7f, 190, on);
        break;
    case MS_6L:
        DrawPole(sx - f.dir * 20, sy + 10, f.dir, 0.0f, (f.animTimer < st) ? 120 : 170, on);
        break;
    case MS_SUPER:
        DrawPole(sx, sy, f.dir, (float)f.animTimer * 0.5f, 320, on);
        if (on) DrawCircle(sx, sy, 320, ORANGE, FALSE, 3);
        break;
    case MS_5J:
    case MS_2J:
        DrawPole(sx, (f.move == MS_2J) ? floorY - 30 : sy, f.dir, (f.move == MS_2J) ? 0.15f : -0.05f,
            (f.move == MS_2J) ? 130 : 95, on);
        break;
    default:
        break;
    }
}

// アナンダの伸びる手足（攻撃判定の長さに合わせて描く）
static void DrawStretchLimb(const Fighter& f, int bx, int floorY) {
    if (f.state != ACT_ATTACK || !InActiveFrames(f)) return;
    int len = 0, y = 0;
    switch (f.move) {
    case MS_5J: len = 130; y = floorY - 45; break;
    case MS_2J: len = 130; y = floorY - 12; break;
    case MS_5K: len = 165; y = floorY - 38; break;
    case MS_2K: len = 165; y = floorY - 8; break;
    default: return;
    }
    unsigned int skin = GetColor(222, 150, 95), dark = GetColor(110, 60, 30);
    int x0 = bx + f.dir * 10, x1 = bx + f.dir * len;
    DrawLine(x0, y, x1, y, dark, 8);
    DrawLine(x0, y, x1, y, skin, 5);
    DrawLine(x0, y - 2, x1, y - 2, GetColor(250, 200, 150), 1);
    DrawCircle(x1, y, 6, dark, TRUE);
    DrawCircle(x1, y, 5, skin, TRUE);
}

static void DrawFighterSprite(const Fighter& f, bool isYou) {
    int c = f.charaIndex;
    int bx = (int)f.x + g_shakeOffsetX;
    int floorY = (int)f.y + FLOOR_DRAW_OFF + g_shakeOffsetY;
    int groundY = (int)GROUND_Y + FLOOR_DRAW_OFF + g_shakeOffsetY;

    // 足元の影（自分は黄色・相手は赤／青の輪で見分けやすく）
    unsigned int ring = isYou ? YELLOW : (g_is_net_battle ? BLUE : RED);
    int shadowW = (c == 4) ? 44 : 24;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 120);
    DrawOval(bx, groundY - 1, shadowW, 5, BLACK, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 200);
    DrawOval(bx, groundY - 1, shadowW + 2, 6, ring, FALSE, 2);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    if (c == 4 && f.state != ACT_ATTACK) DrawMegatonPole(f, bx, floorY);

    int pose = GetPose(f);
    int gh = GetPoseGraph(c, pose);
    float angle = 0.0f;
    if (f.state == ACT_AIR_HIT && !f.isGrounded) angle = (float)(-f.dir) * 0.25f;   // 吹っ飛び中は少し回る
    if (f.state == ACT_DODGE) angle = (float)f.moveDir * (float)f.animTimer * 0.35f;

    bool translucent = f.invulFrames > 0 && (f.state == ACT_DODGE || f.state == ACT_DOWN || (g_frameCount / 2) % 2 == 0);
    bool mirrorTint = (!isYou && g_me.charaIndex == g_other.charaIndex);   // 同キャラ対戦は2P側の色を変える

    if (gh != -1) {
        int w = 0, h = 0;
        GetGraphSize(gh, &w, &h);
        if (translucent) SetDrawBlendMode(DX_BLENDMODE_ALPHA, 120);
        if (mirrorTint) SetDrawBright(255, 170, 150);
        if (f.state == ACT_STUN && (g_frameCount / 4) % 2) SetDrawBright(255, 255, 120);
        DrawRotaGraph(bx, floorY - h / 2, 1.0, angle, gh, TRUE, (f.dir < 0) ? TRUE : FALSE);
        SetDrawBright(255, 255, 255);
        // 被弾直後は白く光らせる
        if (IsHitState(f.state) && f.animTimer < 3) {
            SetDrawBlendMode(DX_BLENDMODE_ADD, 160);
            DrawRotaGraph(bx, floorY - h / 2, 1.0, angle, gh, TRUE, (f.dir < 0) ? TRUE : FALSE);
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
    else {
        // 画像が無い場合の代用（シンプルなシルエット）
        int bh = FighterHeight(f);
        unsigned int col = CHARA_PROFILES[c].baseColor;
        DrawOval(bx, floorY - bh / 2, bh / 4, bh / 2, col, TRUE);
        DrawCircle(bx, floorY - bh + 8, bh / 7, col, TRUE);
    }

    // 技ごとの小道具・エフェクト
    if (c == 4 && f.state == ACT_ATTACK) DrawMegatonPole(f, bx, floorY);
    if (c == 2) DrawStretchLimb(f, bx, floorY);
    if (c == 3 && f.state == ACT_ATTACK && (f.move == MS_5L || f.move == MS_SUPER)) {
        for (int k = 0; k < 5; k++) {
            int ly = floorY - 50 + k * 10;
            DrawLine(bx - f.dir * 24, ly, bx - f.dir * (55 + rand() % 30), ly, WHITE);
        }
    }
    if (f.state == ACT_COUNTER_STANCE) {
        int r = 34 + (int)(sinf((float)f.animTimer * 0.4f) * 5.0f);
        unsigned int ac = (f.stanceLevel == 2) ? YELLOW : CYAN;
        DrawCircle(bx, floorY - 30, r, ac, FALSE, 3);
        DrawCircle(bx, floorY - 30, r + 8, ac, FALSE, 1);
    }
    else if (f.state == ACT_COUNTER_STRIKE) {
        for (int k = 0; k < 3; k++) DrawCircle(bx + f.dir * 30, floorY - 35, 30 + k * 18 + f.animTimer, (k % 2) ? CYAN : WHITE, FALSE, 3);
    }
    else if (f.state == ACT_PARRY) {
        DrawCircle(bx + f.dir * 20, floorY - 32, 24, CYAN, FALSE, 2);
        if (c == 8) DrawCircle(bx + f.dir * 20, floorY - 32, 32, YELLOW, FALSE, 2);
    }
    else if (f.state == ACT_GUARD) {
        // ガード中の盾（ガード硬直中は明るく光る）
        int hgt = f.crouching ? FighterHeight(f) / 3 : FighterHeight(f) / 2;
        int sx = bx + f.dir * ((c == 4) ? 36 : 20);
        int sy = floorY - hgt;
        int pulse = (int)(sinf((float)g_frameCount * 0.3f) * 2.0f);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, f.guardStun > 0 ? 200 : 110);
        DrawOval(sx, sy, 9 + pulse, hgt, f.guardStun > 0 ? WHITE : CYAN, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawOval(sx, sy, 12 + pulse, hgt + 3, CYAN, FALSE);
    }
    if (f.state == ACT_ATTACK && (CurMove(f).rank == 4 || f.variant == 1)) {
        SetDrawBlendMode(DX_BLENDMODE_ADD, 90);
        DrawCircle(bx, floorY - FighterHeight(f) / 2, FighterHeight(f) / 2 + 6, CHARA_PROFILES[c].specialColor, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

static void DrawWorldEffects() {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;
    for (int i = 0; i < MAX_SHOTS; i++) {
        const Projectile& p = g_shots[i];
        if (!p.active) continue;
        int sx = (int)p.x + ox;
        int sy = (int)p.y + oy;
        switch (p.projType) {
        case 2:
            DrawCircle(sx, sy, p.radius + 3, p.color, TRUE);
            DrawCircle(sx, sy, p.radius - 2, WHITE, TRUE);
            break;
        case 4:
            DrawTrapIcon(p.subType % 3, sx, sy + 8);
            break;
        case 5:   // スイカ
            DrawCircle(sx, sy, p.radius, GetColor(40, 140, 50), TRUE);
            for (int k = -1; k <= 1; k++) DrawLine(sx + k * 4, sy - p.radius + 2, sx + k * 4, sy + p.radius - 2, GetColor(20, 70, 25), 2);
            DrawCircle(sx, sy, p.radius, GetColor(10, 40, 10), FALSE);
            break;
        case 6:   // 衝撃波
            DrawOval(sx, sy, p.radius + 4, p.radius / 2 + 6, ORANGE, TRUE);
            DrawOval(sx, sy, p.radius - 2, p.radius / 2 + 2, YELLOW, TRUE);
            break;
        default:
            DrawCircle(sx, sy, p.radius, p.color, TRUE);
            DrawCircle(sx, sy, p.radius, WHITE, FALSE);
            break;
        }
    }
    for (int i = 0; i < MAX_FX; i++) {
        if (g_fx[i].active) DrawCircle((int)g_fx[i].x + ox, (int)g_fx[i].y + oy, 2, g_fx[i].color, TRUE);
    }
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (!g_sparks[i].active) continue;
        float c = cosf(g_sparks[i].angle) * g_sparks[i].length;
        float s = sinf(g_sparks[i].angle) * g_sparks[i].length;
        DrawLine((int)(g_sparks[i].x + ox - c), (int)(g_sparks[i].y + oy - s), (int)(g_sparks[i].x + ox + c), (int)(g_sparks[i].y + oy + s), g_sparks[i].color);
    }
}

// 当たり判定の表示（トレーニング F1）
static void DrawHitboxes() {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;
    const Fighter* fs[2] = { &g_me, &g_other };
    for (int i = 0; i < 2; i++) {
        const Fighter& f = *fs[i];
        const Fighter& opp = *fs[1 - i];
        float cx, cy, w, h;
        GetHurtBox(f, cx, cy, w, h);
        DrawBox((int)(cx - w / 2) + ox, (int)(cy - h / 2) + oy, (int)(cx + w / 2) + ox, (int)(cy + h / 2) + oy,
            f.invulFrames > 0 ? WHITE : BLUE, FALSE);
        if (f.state == ACT_THROW) {
            bool on = f.animTimer >= 4 && f.animTimer <= 6;
            DrawBox((int)f.x - 54 + ox, (int)f.y - 30 + oy, (int)f.x + 54 + ox, (int)f.y + 25 + oy, on ? YELLOW : GetColor(120, 100, 30), FALSE);
            continue;
        }
        AttackBox b;
        if (!GetAttackBox(f, fabsf(f.x - opp.x), b)) continue;
        int x1 = (int)(f.x + b.offX - b.w / 2) + ox, y1 = (int)(f.y + b.offY - b.h / 2) + oy;
        int x2 = (int)(f.x + b.offX + b.w / 2) + ox, y2 = (int)(f.y + b.offY + b.h / 2) + oy;
        if (InActiveFrames(f) && !f.hasHit) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);
            DrawBox(x1, y1, x2, y2, RED, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawBox(x1, y1, x2, y2, RED, FALSE);
        }
        else {
            DrawBox(x1, y1, x2, y2, GetColor(120, 40, 40), FALSE);
        }
    }
}

// ----------------------------------------------------
// 画面側（ズームしない）に描く、キャラの頭上の表示
// ----------------------------------------------------
static void DrawFighterOverlay(const Fighter& f, bool isYou) {
    int sx, sy;
    WorldToScreen(f.x, f.y + FLOOR_DRAW_OFF - (float)FighterHeight(f) - 6.0f, sx, sy);
    if (sy < 74) sy = 74;

    const char* label = isYou ? (g_is_net_battle ? "YOU" : "1P YOU") :
        (g_is_net_battle ? "2P" : ((g_mode == MODE_TRAINING || g_mode == MODE_TUTORIAL) ? "DUMMY" : "COM"));
    unsigned int col = isYou ? YELLOW : (g_is_net_battle ? BLUE : RED);
    int lw = GetDrawStringWidth(label, (int)strlen(label));
    DrawBox(sx - lw / 2 - 3, sy - 22, sx + lw / 2 + 3, sy - 6, BLACK, TRUE);
    DrawBox(sx - lw / 2 - 3, sy - 22, sx + lw / 2 + 3, sy - 6, col, FALSE);
    DrawString(sx - lw / 2, sy - 21, label, col);
    DrawTriangle(sx - 6, sy - 5, sx + 6, sy - 5, sx, sy + 2, col, TRUE);

    // 技名（何の技が出ているか）
    if (f.labelTimer > 0) {
        const char* name = GetLabelName(f);
        if (name && name[0]) {
            int nw = GetDrawStringWidth(name, (int)strlen(name));
            int ny = sy - 44;
            unsigned int nc = (f.labelSlot >= 0 && SlotRank(f.labelSlot) >= 3) ? ORANGE : WHITE;
            if (f.labelSlot < 0) nc = CYAN;
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
            DrawBox(sx - nw / 2 - 5, ny - 2, sx + nw / 2 + 5, ny + 17, BLACK, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawBox(sx - nw / 2 - 5, ny - 2, sx + nw / 2 + 5, ny + 17, col, FALSE);
            DrawString(sx - nw / 2, ny, name, nc);
        }
    }

    // 状態の文字
    const char* st = NULL;
    unsigned int stc = YELLOW;
    if (f.state == ACT_STUN) st = "*PIYO*";
    else if (f.state == ACT_SLIP) st = "@_@";
    else if (f.state == ACT_HELD) { st = "つかまれ中!"; stc = RED; }
    else if (f.state == ACT_COUNTER_STANCE) { st = (f.stanceLevel == 2) ? "極・当身" : ((f.stanceLevel == 3) ? "下段当身" : "当身"); stc = CYAN; }
    else if (f.state == ACT_TAUNT) { st = "ほらほら？"; stc = CYAN; }
    else if (f.charaIndex == 8 && f.state == ACT_IDLE && (g_frameCount / 40) % 3 == 0) { st = "ﾆﾔﾆﾔ"; stc = CYAN; }
    if (st) {
        int w = GetDrawStringWidth(st, (int)strlen(st));
        DrawString(sx - w / 2 + 1, sy + 7, st, BLACK);
        DrawString(sx - w / 2, sy + 6, st, stc);
    }

    // いのししのサイコロと目押しバー
    if (f.state == ACT_BOAR_ROLL || f.diceShowTimer > 0) {
        int dx = sx + 40, dy = sy + 10;
        DrawBox(dx - 13, dy - 13, dx + 13, dy + 13, WHITE, TRUE);
        DrawBox(dx - 13, dy - 13, dx + 13, dy + 13, (f.diceFace <= 2) ? RED : BLACK, FALSE);
        DrawFormatString(dx - 4, dy - 8, (f.diceFace <= 2) ? RED : BLACK, "%d", f.diceFace);
        if (f.state == ACT_BOAR_ROLL && !f.boarSuper) {
            int barX = dx - 40, barY = dy + 18, barW = 80;
            DrawBox(barX, barY, barX + barW, barY + 7, GetColor(40, 40, 40), TRUE);
            int gz1 = barX + barW * BOAR_TIMING_START / BOAR_ROLL_FRAMES;
            int gz2 = barX + barW * (BOAR_TIMING_END + 1) / BOAR_ROLL_FRAMES;
            DrawBox(gz1, barY, gz2, barY + 7, GREEN, TRUE);
            int cur = barX + barW * ClampI(f.animTimer, 0, BOAR_ROLL_FRAMES) / BOAR_ROLL_FRAMES;
            DrawLine(cur, barY - 3, cur, barY + 10, WHITE, 2);
            DrawBox(barX, barY, barX + barW, barY + 7, WHITE, FALSE);
        }
    }
}

static void DrawPopups() {
    // トレーニング・チュートリアルは上に情報パネルがあるので、その下に出す
    int minY = (g_mode == MODE_TRAINING) ? 146 : ((g_mode == MODE_TUTORIAL) ? 166 : 66);
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!g_popups[i].active) continue;
        int px, py;
        WorldToScreen(g_popups[i].x, g_popups[i].y, px, py);
        int w = GetDrawStringWidth(g_popups[i].text, (int)strlen(g_popups[i].text));
        px -= w / 2;
        if (px < 4) px = 4;
        if (px + w > 636) px = 636 - w;
        if (py < minY) py = minY;
        DrawString(px + 1, py + 1, g_popups[i].text, BLACK);
        DrawString(px, py, g_popups[i].text, g_popups[i].color);
    }
}

// 感電床の予告など、ステージの文字
static void DrawStageOverlay(int stageIdx) {
    if (stageIdx == 1 && g_gimmickEnabled && g_gimmick.hazardWarning) {
        char buf[32];
        sprintf_s(buf, "感電まで %d", g_gimmick.hazardCountdown);
        DrawExtendString(232, 150, 2.0, 2.0, buf, (g_frameCount / 8) % 2 ? YELLOW : RED);
        DrawString(150, 188, "ジャンプ・ガード・緑の安全地帯・コンテナ破壊で回避！", WHITE);
    }
    if (stageIdx == 1 && g_gimmickEnabled && (g_gimmick.hazardWarning || g_gimmick.hazardActive)) {
        int sx, sy;
        WorldToScreen(g_gimmick.x, GROUND_Y + 8.0f, sx, sy);
        DrawString(sx - 16, sy, "SAFE", GREEN);
    }
    if (g_gimmick.itemActive) {
        int sx, sy;
        WorldToScreen(g_gimmick.itemX, g_gimmick.itemY - 40.0f, sx, sy);
        DrawString(sx - 16, sy, "HEAL", GREEN);
    }
}
