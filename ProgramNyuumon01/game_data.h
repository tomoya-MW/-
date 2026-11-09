// ====================================================
// キャラ設定・技データ・入力・セーブ・画像読み込み
// ====================================================
#pragma once

static int ClampI(int v, int lo, int hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }
static bool IsHitState(ActionState s) { return s == ACT_HIT || s == ACT_AIR_HIT; }
static int FighterIndex(const Fighter& f) { return (&f == &g_me) ? 0 : 1; }
static bool IsAirSlot(int s) { return s == MS_JJ || s == MS_JK || s == MS_JL || s == MS_JSUPER; }
static bool IsLowSlot(int s) { return s == MS_2J || s == MS_2K || s == MS_2L; }

static int SlotRank(int s) {
    switch (s) {
    case MS_5J: case MS_2J: case MS_JJ: return 1;
    case MS_5K: case MS_2K: case MS_JK: return 2;
    case MS_SUPER: case MS_JSUPER: return 4;
    default: return 3;
    }
}

// ----------------------------------------------------
// 技データ
// ----------------------------------------------------
static void SetMove(int c, int s, const char* name, int st, int act, int tot, int dmg, int stun,
    float kx, float ky, int w, int h, int ox, int oy, int flags, float vx, float rise, int beh, int pose) {
    MoveDef& m = g_moves[c][s];
    m.valid = true;
    m.name = name;
    m.startup = (short)st; m.active = (short)act; m.total = (short)tot;
    m.damage = (short)dmg; m.stun = (short)stun;
    m.kx = kx; m.ky = ky;
    m.w = (short)w; m.h = (short)h; m.offX = (short)ox; m.offY = (short)oy;
    m.flags = flags;
    m.vx = vx; m.rise = rise;
    m.behavior = (short)beh;
    m.rank = (short)SlotRank(s);
    m.pose = (short)pose;
    m.whiff = 0; m.multi = 0; m.armor = 0;
}

static void SetExtra(int c, int s, int whiff, int multi, int armor) {
    g_moves[c][s].whiff = (short)whiff;
    g_moves[c][s].multi = (short)multi;
    g_moves[c][s].armor = (short)armor;
}

// 全キャラ共通のひな形（弱・強の6技）
static void SetBasicMoves(int c) {
    SetMove(c, MS_5J, "立ち弱", 5, 3, 16, 40, 16, 2.2f, 0, 44, 28, 32, -12, 0, 2, 0, BH_NONE, PO_LIGHT);
    SetMove(c, MS_2J, "しゃがみ弱", 5, 3, 16, 32, 15, 1.8f, 0, 46, 20, 30, 14, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    SetMove(c, MS_JJ, "空中弱", 5, 8, 20, 45, 18, 2.0f, 0, 46, 36, 26, 4, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(c, MS_5K, "立ち強", 9, 4, 28, 75, 22, 4.5f, 0, 64, 30, 44, -10, 0, 2, 0, BH_NONE, PO_HEAVY);
    SetMove(c, MS_2K, "足払い", 9, 4, 30, 65, 20, 2.0f, -4, 66, 18, 42, 16, MF_LOW | MF_KNOCKDOWN, 1, 0, BH_NONE, PO_LOW);
    SetMove(c, MS_JK, "空中強", 7, 8, 24, 80, 22, 3.5f, 6, 56, 44, 30, 16, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
}

static void InitMoves() {
    memset(g_moves, 0, sizeof(g_moves));
    for (int c = 0; c < CHARA_COUNT; c++) SetBasicMoves(c);

    // 0 大乗殿 力也：バランス型の空手家（初心者向け）
    SetMove(0, MS_5J, "正拳突き", 5, 3, 16, 42, 16, 2.2f, 0, 46, 28, 32, -12, 0, 2, 0, BH_NONE, PO_LIGHT);
    SetMove(0, MS_2J, "下段突き", 5, 3, 16, 34, 15, 1.8f, 0, 46, 20, 30, 14, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    SetMove(0, MS_JJ, "空中突き", 4, 6, 18, 48, 20, 1.8f, -2, 52, 38, 28, 0, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(0, MS_5K, "前蹴り", 8, 4, 26, 78, 22, 4.8f, 0, 64, 30, 44, -6, 0, 2, 0, BH_NONE, PO_HEAVY);
    SetMove(0, MS_JK, "飛び蹴り", 7, 8, 24, 90, 24, 4.0f, 15, 58, 50, 32, 22, MF_OVERHEAD | MF_SMASH, 0, 0, BH_NONE, PO_AIR);
    SetMove(0, MS_5L, "昇天脚", 10, 6, 34, 125, 48, 1.5f, -17.5f, 64, 110, 34, -25, MF_LAUNCH | MF_INVUL, 2, -16.5f, BH_NONE, PO_UPPER);
    SetMove(0, MS_6L, "気弾", 12, 1, 32, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_SHOT, PO_SPECIAL);
    SetMove(0, MS_2L, "鉄拳突き", 12, 5, 36, 110, 30, 9.0f, -4, 60, 34, 40, -10, MF_SMASH, 9, 0, BH_NONE, PO_SPECIAL);
    SetMove(0, MS_JL, "急降下蹴り", 6, 10, 26, 100, 26, 4.0f, 16, 56, 50, 30, 22, MF_OVERHEAD | MF_SMASH, 0, 0, BH_DIVE, PO_AIR);
    SetMove(0, MS_SUPER, "ローリングソバット", 8, 8, 45, 190, 60, 7.0f, -15, 80, 50, 45, -15, MF_LAUNCH, 12, 0, BH_DRAGON, PO_SUPER);
    SetMove(0, MS_JSUPER, "天翔・金剛螺旋脚", 6, 10, 45, 240, 55, 8.0f, 18, 85, 75, 45, 20, MF_SMASH, 0, 0, BH_KOKUSEN, PO_SUPER);

    // 1 罠太郎：罠を置いて逃げ回る（中級者向け）
    SetMove(1, MS_5J, "ぺちっ", 5, 3, 15, 24, 14, 1.5f, 0, 32, 24, 24, -12, 0, 1, 0, BH_NONE, PO_LIGHT);
    SetMove(1, MS_2J, "すね蹴り", 5, 3, 15, 22, 13, 1.2f, 0, 36, 18, 24, 16, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    SetMove(1, MS_JJ, "空中ぺち", 5, 6, 18, 26, 16, 1.5f, -1, 36, 30, 22, 0, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(1, MS_5K, "ほうき突き", 9, 4, 26, 48, 18, 3.0f, 0, 70, 24, 46, -10, 0, 1, 0, BH_NONE, PO_LIGHT);
    SetMove(1, MS_2K, "ほうき払い", 9, 4, 28, 40, 16, 2.0f, -4, 70, 16, 46, 18, MF_LOW | MF_KNOCKDOWN, 0, 0, BH_NONE, PO_LOW);
    SetMove(1, MS_JK, "空中ほうき", 7, 6, 22, 50, 18, 2.5f, 5, 56, 36, 30, 12, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(1, MS_5L, "罠設置", 8, 1, 20, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_TRAP_DROP, PO_CROUCH);
    SetMove(1, MS_6L, "罠投げ", 10, 1, 26, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_TRAP_THROW, PO_SPECIAL);
    SetMove(1, MS_2L, "煙玉バックステップ", 3, 1, 24, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_SMOKE, PO_ROLL);
    SetMove(1, MS_JL, "空中罠落とし", 6, 1, 18, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_TRAP_AIR, PO_AIR);
    SetMove(1, MS_SUPER, "大特価!! 罠バラまきセール", 10, 1, 40, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_TRAP_SCATTER, PO_SUPER);

    // 2 アナンダ：伸びる手足と浮遊（中級者向け）
    SetMove(2, MS_5J, "伸びる突き", 6, 4, 20, 46, 18, 3.0f, 0, 120, 26, 80, -12, 0, 0, 0, BH_NONE, PO_LIGHT);
    SetMove(2, MS_2J, "伸びる下段突き", 7, 4, 22, 40, 16, 2.5f, 0, 120, 18, 80, 16, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    SetMove(2, MS_JJ, "伸びる蹴り下ろし", 6, 8, 22, 60, 22, 3.0f, 10, 70, 60, 40, 22, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(2, MS_5K, "伸びる回し蹴り", 10, 5, 30, 70, 24, 5.0f, 0, 160, 30, 105, -8, 0, 0, 0, BH_NONE, PO_HEAVY);
    SetMove(2, MS_2K, "伸びる足払い", 10, 5, 32, 60, 20, 2.0f, -4, 160, 18, 105, 18, MF_LOW | MF_KNOCKDOWN, 0, 0, BH_NONE, PO_LOW);
    SetMove(2, MS_JK, "伸びる空中蹴り", 8, 8, 26, 90, 24, 4.0f, 14, 85, 70, 45, 25, MF_OVERHEAD | MF_SMASH, 0, 0, BH_NONE, PO_AIR);
    SetMove(2, MS_5L, "ヨガ突進", 10, 6, 34, 140, 48, 2.2f, -16.5f, 75, 75, 45, -28, MF_LAUNCH, 8.5f, 0, BH_NONE, PO_SPECIAL);
    SetMove(2, MS_6L, "ヨガテレポート", 14, 1, 30, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX | MF_INVUL, 0, 0, BH_TELEPORT_BEHIND, PO_SPECIAL);
    SetMove(2, MS_2L, "伸縮アッパー", 6, 6, 30, 90, 40, 1.0f, -15, 50, 100, 20, -40, MF_LAUNCH | MF_INVUL, 0, 0, BH_NONE, PO_UPPER);
    SetMove(2, MS_JL, "浮遊", 2, 1, 6, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_FLOAT, PO_JUMP);
    SetMove(2, MS_SUPER, "ブラフマン・クンダリニー", 8, 1, 50, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_8WAY, PO_SUPER);

    // 3 いのししマン：手数で押すゲーム＆ウォッチ風（中級者向け）
    SetMove(3, MS_5J, "牙突き", 4, 3, 14, 30, 15, 1.5f, 0, 40, 26, 30, -10, 0, 1.5f, 0, BH_NONE, PO_LIGHT);
    SetMove(3, MS_2J, "鼻先すくい", 5, 3, 16, 30, 15, 1.0f, -1, 44, 18, 28, 16, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    SetMove(3, MS_JJ, "ひづめ蹴り", 4, 6, 16, 40, 16, 2.0f, 0, 44, 34, 24, 6, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(3, MS_5K, "スイカ投げ", 11, 1, 30, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_WATERMELON, PO_LIGHT);
    SetMove(3, MS_2K, "泥すべり", 8, 10, 30, 55, 18, 3.0f, -4, 60, 18, 30, 18, MF_LOW | MF_KNOCKDOWN, 7, 0, BH_NONE, PO_LOW);
    SetMove(3, MS_JK, "スイカ割り", 8, 8, 24, 85, 22, 3.0f, 14, 56, 46, 28, 20, MF_OVERHEAD | MF_SMASH, 0, 0, BH_NONE, PO_AIR);
    SetMove(3, MS_5L, "猪突猛進", 0, 30, 38, 160, 50, 11.0f, -9, 64, 54, 30, -10, MF_LAUNCH, 0, 0, BH_BOAR_DICE, PO_DASH);
    SetMove(3, MS_6L, "ぶちかまし", 9, 6, 30, 80, 40, 3.0f, -15, 60, 50, 34, -12, MF_LAUNCH, 9, 0, BH_NONE, PO_DASH);
    SetMove(3, MS_2L, "ぬた打ち回転", 8, 24, 38, 26, 12, 2.0f, 0, 60, 30, 20, 6, MF_LOW | MF_MULTI, 5, 0, BH_NONE, PO_ROLL);
    SetMove(3, MS_JL, "ボディプレス", 6, 14, 26, 90, 26, 3.0f, 14, 70, 40, 0, 20, MF_OVERHEAD | MF_SMASH, 0, 0, BH_DIVE, PO_AIR);
    SetMove(3, MS_SUPER, "確変!! 出目6確定突進", 0, 40, 48, 260, 60, 13.0f, -9, 64, 54, 30, -10, MF_LAUNCH | MF_UNBLOCK, 0, 0, BH_BOAR_SUPER, PO_SUPER);
    SetExtra(3, MS_2L, 0, 6, 0);

    // 4 メガトン大前：物干し竿の超リーチ。跳躍不可（中級者向け）
    SetMove(4, MS_5J, "石突き", 7, 4, 20, 50, 20, 3.0f, 0, 60, 40, 45, -12, 0, 0, 0, BH_NONE, PO_LIGHT);
    SetMove(4, MS_2J, "竿払い", 9, 4, 26, 45, 18, 2.5f, 0, 120, 18, 75, 18, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    g_moves[4][MS_JJ].valid = false;
    SetMove(4, MS_5K, "横薙ぎ", 18, 5, 30, 165, 42, 10.0f, -5, 300, 55, 165, -15, MF_SMASH, 0, 0, BH_NONE, PO_HEAVY);
    SetMove(4, MS_2K, "足元薙ぎ", 16, 5, 30, 120, 20, 3.0f, -4, 220, 20, 125, 18, MF_LOW | MF_KNOCKDOWN, 0, 0, BH_NONE, PO_LOW);
    g_moves[4][MS_JK].valid = false;
    SetMove(4, MS_5L, "振り下ろし", 22, 5, 34, 190, 50, 4.0f, -17, 170, 130, 115, -25, MF_LAUNCH | MF_OVERHEAD, 0, 0, BH_NONE, PO_SPECIAL);
    SetMove(4, MS_6L, "竿突き", 14, 6, 34, 120, 30, 8.0f, -3, 160, 30, 100, -12, MF_ARMOR, 6, 0, BH_NONE, PO_LIGHT);
    SetMove(4, MS_2L, "四股踏み", 16, 1, 36, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_SHOCKWAVE, PO_CROUCH);
    SetMove(4, MS_SUPER, "電柱フルスイング・天地轟断", 24, 12, 55, 270, 60, 12.0f, -10, 1280, 90, 0, -15, MF_LAUNCH | MF_ARMOR, 0, 0, BH_NONE, PO_SUPER);
    SetExtra(4, MS_5K, 55, 0, 0);
    SetExtra(4, MS_2K, 45, 0, 0);
    SetExtra(4, MS_5L, 55, 0, 0);
    SetExtra(4, MS_6L, 30, 0, 14);
    SetExtra(4, MS_SUPER, 40, 0, 25);

    // 5 望月ママ（CPU専用）
    SetMove(5, MS_5J, "椅子はたき", 6, 4, 18, 55, 22, 3.5f, 0, 62, 35, 44, -12, 0, 1, 0, BH_NONE, PO_LIGHT);
    SetMove(5, MS_5K, "椅子フルスイング", 10, 5, 30, 85, 24, 5.5f, 0, 70, 40, 46, -10, 0, 2, 0, BH_NONE, PO_HEAVY);
    SetMove(5, MS_5L, "椅子振り上げ", 10, 6, 34, 120, 46, 2.0f, -16.5f, 68, 65, 44, -32, MF_LAUNCH, 3, 0, BH_NONE, PO_UPPER);
    SetMove(5, MS_6L, "猛突進", 10, 10, 36, 110, 30, 8.0f, -4, 60, 40, 36, -10, MF_ARMOR, 9.5f, 0, BH_NONE, PO_DASH);
    SetMove(5, MS_SUPER, "商店街大乱舞", 8, 8, 45, 190, 60, 7.0f, -15, 80, 50, 45, -15, MF_LAUNCH, 10, 0, BH_NONE, PO_SUPER);
    SetExtra(5, MS_6L, 0, 0, 12);

    // 6 真・天輪太子：神速・高火力・低体力（玄人向け）
    SetMove(6, MS_5J, "神速拳", 4, 3, 14, 50, 18, 3.0f, 0, 50, 30, 38, -12, 0, 2, 0, BH_NONE, PO_LIGHT);
    SetMove(6, MS_2J, "下段拳", 4, 3, 14, 42, 16, 2.0f, 0, 50, 18, 34, 16, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    SetMove(6, MS_JJ, "空中拳", 4, 6, 16, 55, 18, 2.0f, 0, 50, 36, 28, 2, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(6, MS_5K, "神速蹴り", 7, 4, 24, 85, 24, 5.0f, 0, 70, 30, 48, -10, 0, 2, 0, BH_NONE, PO_HEAVY);
    SetMove(6, MS_2K, "下段払い", 7, 4, 26, 70, 20, 2.0f, -4, 70, 18, 46, 18, MF_LOW | MF_KNOCKDOWN, 0, 0, BH_NONE, PO_LOW);
    SetMove(6, MS_JK, "空中蹴り", 6, 8, 22, 95, 24, 4.0f, 14, 60, 48, 32, 20, MF_OVERHEAD | MF_SMASH, 0, 0, BH_NONE, PO_AIR);
    SetMove(6, MS_5L, "天輪掌", 8, 6, 32, 150, 55, 2.0f, -18, 80, 80, 45, -35, MF_LAUNCH, 2, 0, BH_NONE, PO_SPECIAL);
    SetMove(6, MS_6L, "瞬動", 2, 10, 22, 80, 22, 4.0f, 0, 60, 45, 35, -12, MF_INVUL, 15, 0, BH_NONE, PO_DASH);
    SetMove(6, MS_2L, "天昇脚", 5, 10, 34, 50, 30, 1.0f, -14, 60, 100, 30, -40, MF_LAUNCH | MF_MULTI | MF_INVUL, 0, -12, BH_NONE, PO_UPPER);
    SetMove(6, MS_JL, "空中瞬動", 2, 8, 20, 70, 20, 4.0f, 0, 60, 40, 34, 0, MF_INVUL, 12, 0, BH_NONE, PO_DASH);
    SetMove(6, MS_SUPER, "天輪・神速百烈拳", 6, 30, 45, 70, 30, 3.0f, -2, 80, 60, 40, -15, MF_MULTI, 6, 0, BH_TELEPORT_FRONT, PO_SUPER);
    SetExtra(6, MS_2L, 0, 5, 0);
    SetExtra(6, MS_SUPER, 0, 6, 0);

    // 7 吉田：リーチの長い蹴り（初心者向け）
    SetMove(7, MS_5J, "ローキック", 5, 4, 18, 50, 18, 2.6f, 0, 70, 28, 50, 0, 0, 1, 0, BH_NONE, PO_HEAVY);
    SetMove(7, MS_2J, "下段蹴り", 5, 4, 18, 42, 16, 2.0f, 0, 70, 18, 46, 18, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    SetMove(7, MS_JJ, "かかと落とし", 6, 6, 22, 70, 24, 3.0f, 15, 60, 50, 32, 20, MF_OVERHEAD | MF_SMASH, 0, 0, BH_NONE, PO_AIR);
    SetMove(7, MS_5K, "上段回し蹴り", 8, 5, 28, 85, 24, 5.0f, 0, 80, 36, 55, -20, 0, 1, 0, BH_NONE, PO_HEAVY);
    SetMove(7, MS_2K, "足払い", 8, 5, 30, 75, 22, 2.0f, -4, 80, 18, 52, 18, MF_LOW | MF_KNOCKDOWN, 0, 0, BH_NONE, PO_LOW);
    SetMove(7, MS_JK, "旋風脚", 7, 8, 24, 110, 26, 7.5f, -6, 90, 50, 30, 10, MF_OVERHEAD | MF_SMASH, 0, 0, BH_NONE, PO_AIR);
    SetMove(7, MS_5L, "サマーソルト", 5, 8, 30, 130, 48, 2.0f, -18, 60, 110, 30, -25, MF_LAUNCH | MF_INVUL, 2.5f, -13, BH_NONE, PO_UPPER);
    SetMove(7, MS_6L, "飛び横蹴り", 9, 7, 32, 105, 36, 9.5f, -4, 90, 40, 60, -12, MF_SMASH, 8, 0, BH_NONE, PO_HEAVY);
    SetMove(7, MS_2L, "下段三連脚", 6, 18, 34, 30, 14, 1.5f, 0, 80, 18, 50, 18, MF_LOW | MF_MULTI, 3, 0, BH_NONE, PO_LOW);
    SetMove(7, MS_JL, "空中三段蹴り", 5, 15, 26, 40, 18, 3.0f, -2, 70, 40, 36, 6, MF_MULTI, 0, 0, BH_NONE, PO_AIR);
    SetMove(7, MS_SUPER, "吉田式・百烈脚", 6, 42, 55, 42, 30, 2.5f, -1, 80, 60, 45, -10, MF_MULTI, 2.5f, 0, BH_NONE, PO_SUPER);
    SetExtra(7, MS_2L, 0, 6, 0);
    SetExtra(7, MS_JL, 0, 5, 0);
    SetExtra(7, MS_SUPER, 0, 6, 0);

    // 8 無防備たけし：当身とパリィで返す（玄人向け）
    SetMove(8, MS_5J, "ちょい突き", 4, 3, 14, 26, 14, 1.2f, 0, 26, 22, 20, -12, 0, 1, 0, BH_NONE, PO_LIGHT);
    SetMove(8, MS_2J, "ちょい下段", 4, 3, 14, 22, 13, 1.0f, 0, 28, 16, 20, 16, MF_LOW, 0, 0, BH_NONE, PO_LOW);
    SetMove(8, MS_JJ, "空中ちょい", 4, 4, 14, 28, 14, 1.2f, 0, 30, 26, 20, 0, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(8, MS_5K, "ちょい蹴り", 7, 3, 22, 40, 16, 2.0f, 0, 36, 26, 24, -8, 0, 0, 0, BH_NONE, PO_HEAVY);
    SetMove(8, MS_2K, "すね払い", 7, 3, 24, 34, 14, 1.5f, -4, 36, 16, 24, 18, MF_LOW | MF_KNOCKDOWN, 0, 0, BH_NONE, PO_LOW);
    SetMove(8, MS_JK, "空中ちょい蹴り", 6, 5, 18, 40, 16, 2.0f, 4, 36, 32, 22, 10, MF_OVERHEAD, 0, 0, BH_NONE, PO_AIR);
    SetMove(8, MS_5L, "当身（上・中段）", 2, 31, 40, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_COUNTER, PO_GUARD);
    SetMove(8, MS_6L, "挑発", 1, 1, 45, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_TAUNT, PO_WIN);
    SetMove(8, MS_2L, "下段当身", 2, 31, 40, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_COUNTER_LOW, PO_CGUARD);
    SetMove(8, MS_SUPER, "奥義・無防備の極み", 2, 88, 90, 0, 0, 0, 0, 0, 0, 0, 0, MF_NOHITBOX, 0, 0, BH_COUNTER_SUPER, PO_SUPER);
}

static void InitMasterData() {
    CHARA_PROFILES[0] = { "大乗殿 力也",   "【町内会代表】 本格空手道",               "L昇天脚で対空、前+Lで気弾。迷ったらこのキャラ",      5.0f, -14.0f, 1000, RED,    ORANGE, 1, 9.5f, 0 };
    CHARA_PROFILES[1] = { "罠太郎",        "【イライラ製造職人】 罠設置・逃げ回り",   "Lで足元・前+Lで遠くに罠。近接は激弱",               4.6f, -13.0f,  900, YELLOW, ORANGE, 1, 10.0f, 1 };
    CHARA_PROFILES[2] = { "アナンダ・シャンカラン", "【ヨガマスター】 伸縮肢体・浮遊",   "J・Kが長く伸びる(先端1.5倍)。前+Lで背後へワープ",    5.4f, -14.5f,  880, CYAN,   WHITE,  1, 9.0f, 1 };
    CHARA_PROFILES[3] = { "いのししマン",  "【畑を荒らすスイカ泥棒】 運と腕の猪突猛進", "J連打→前+L→空中コンボ。Lはサイコロ突進",          4.8f, -14.0f,  980, GetColor(150, 95, 55), GREEN, 1, 10.0f, 1 };
    CHARA_PROFILES[4] = { "メガトン大前",  "【元大横綱】 物干し竿の超リーチ・跳躍不可", "Kは画面半分届く横薙ぎ。外すと約1秒の大スキ",      2.6f, 0.0f, 1400, ORANGE, RED, 1, 6.0f, 1 };
    CHARA_PROFILES[5] = { "望月ママ",      "【商店街の自警団長】 パイプ椅子乱舞",     "強引な椅子制圧と猛突進",                           4.8f, -13.5f, 1150, BLUE,   WHITE,  99, 9.0f, 0 };
    CHARA_PROFILES[6] = { "真・天輪太子",  "【武神降臨】 封印解放・神速",             "全技が速く高火力だが体力780。前+Lで無敵の瞬動",    5.6f, -15.0f, 1500, WHITE,  YELLOW, 3, 13.0f, 2 };
    CHARA_PROFILES[7] = { "吉田",          "【足技の鬼】 リーチの長い蹴り",           "前+L飛び横蹴りで壁バウンド。Lサマーソルトで対空",  5.0f, -14.0f,  950, GREEN,  WHITE,  1, 10.0f, 0 };
    CHARA_PROFILES[8] = { "無防備 たけし", "『ノーガード戦法研究家』 当身・パリィ特化", "L当身/S+L下段当身/前+L挑発。攻撃力は最低",        4.2f, -13.0f,  980, PURPLE, CYAN, 2, 9.0f, 2 };

    STAGES[0] = { "STAGE 1: 竜崎のシマ（駄菓子屋前）",   GetColor(25, 20, 35), GetColor(45, 45, 50), "サビ自転車",     0 };
    STAGES[1] = { "STAGE 2: 電脳ガレージ研究所",         GetColor(15, 30, 45), GetColor(35, 50, 60), "感電コンテナ",   1 };
    STAGES[2] = { "STAGE 3: 夕暮れの商店街特売広場",     GetColor(45, 30, 20), GetColor(60, 50, 45), "特売ワゴン",     2 };
    STAGES[3] = { "STAGE 4: ウランバートル郊外ゲル",     GetColor(35, 45, 20), GetColor(60, 55, 35), "干し肉ラック",   3 };
    STAGES[4] = { "FINAL STAGE: ガンジス河畔・覚醒の祭壇", GetColor(40, 10, 35), GetColor(50, 35, 20), "黄金の蓮華石柱", 4 };
    STAGES[5] = { "EX STAGE: 天下一武道会・裏本殿",      GetColor(10, 20, 50), GetColor(40, 30, 60), "天輪の大神柱",   5 };

    InitMoves();
}

// いのししマンのサイコロ出目ごとの突進性能
struct BoarFace {
    float speed;
    int duration;
    int damage;
    bool unblockable;
    float knockX;
    const char* label;
};
static const BoarFace BOAR_FACES[7] = {
    { 0.0f,  0,   0, false,  0.0f, "" },
    { 0.0f,  0,   0, false,  0.0f, "1: ズコッ… 小転倒" },
    { 0.0f,  0,   0, false,  0.0f, "2: つまずいた!" },
    { 13.0f, 20, 120, false,  7.0f, "3: 様子見突進（ガード可）" },
    { 16.0f, 26, 150, false,  9.0f, "4: 全力突進（ガード可）" },
    { 17.0f, 30, 165, true,  10.0f, "5: ガード不能突進!!" },
    { 20.0f, 34, 210, true,  13.0f, "6: 大当たり!! 超高速ガード不能!!" }
};
static const int BOAR_ROLL_FRAMES = 30;
static const int BOAR_TIMING_START = 20;
static const int BOAR_TIMING_END = 25;

static const MoveDef& CurMove(const Fighter& f) {
    return g_moves[f.charaIndex][ClampI(f.move, 0, MS_COUNT - 1)];
}

// 技の発生と持続（いのししの突進や黒閃など派生はここで差し替え）
static void GetMoveFrames(const Fighter& f, int& st, int& act) {
    const MoveDef& m = CurMove(f);
    st = m.startup;
    act = m.active;
    if (f.charaIndex == 3 && (f.move == MS_5L || f.move == MS_SUPER)) {
        int face = f.boarSuper ? 6 : ClampI(f.diceFace, 3, 6);
        st = 0;
        act = f.boarSuper ? 40 : BOAR_FACES[face].duration;
    }
    if (f.variant == 1 && f.charaIndex == 0 && (f.move == MS_SUPER || f.move == MS_JSUPER)) {
        st = 5;
        act = 14;
    }
}

static bool InActiveFrames(const Fighter& f) {
    if (f.state != ACT_ATTACK) return false;
    int st, act;
    GetMoveFrames(f, st, act);
    return f.animTimer >= st && f.animTimer < st + act;
}

static const char* GetLabelName(const Fighter& f) {
    if (f.labelSlot == -1) return "投げ";
    if (f.labelSlot == -2) return "裏投げ";
    if (f.labelSlot < 0 || f.labelSlot >= MS_COUNT) return "";
    if (f.charaIndex == 0 && f.labelSlot == MS_JSUPER && f.variant == 1) return "黒閃・真極点昇天破!!";
    if (f.charaIndex == 0 && f.labelSlot == MS_SUPER && f.variant == 1) return "ドラゴンスープレックス";
    return g_moves[f.charaIndex][f.labelSlot].name;
}

// ----------------------------------------------------
// 入力
// ----------------------------------------------------
static bool KeyDown(int k) { return g_keyNow[k] != 0; }
static bool KeyHit(int k) { return g_keyNow[k] != 0 && g_keyPrev[k] == 0; }

static unsigned short ReadLocalBits() {
    unsigned short b = 0;
    if (KeyDown(KEY_INPUT_A)) b |= IN_LEFT;
    if (KeyDown(KEY_INPUT_D)) b |= IN_RIGHT;
    if (KeyDown(KEY_INPUT_W)) b |= IN_UP;
    if (KeyDown(KEY_INPUT_S)) b |= IN_DOWN;
    if (KeyDown(KEY_INPUT_J)) b |= IN_J;
    if (KeyDown(KEY_INPUT_K)) b |= IN_K;
    if (KeyDown(KEY_INPUT_L)) b |= IN_L;
    if (KeyDown(KEY_INPUT_SPACE)) b |= IN_SPACE;
    if (KeyDown(KEY_INPUT_LSHIFT)) b |= IN_THROW;
    if (KeyDown(KEY_INPUT_I)) b |= IN_SUPER;
    return b;
}

static const int DOUBLE_TAP_FRAMES = 14;

static void ResetControl(ControlState& cs) {
    cs.held = cs.pressed = cs.dbl = 0;
    for (int i = 0; i < 4; i++) cs.lastTap[i] = -1000;
}

static void UpdateControl(ControlState& cs, unsigned short bits) {
    cs.pressed = (unsigned short)(bits & ~cs.held);
    cs.dbl = 0;
    for (int b = 0; b < 4; b++) {
        unsigned short bit = (unsigned short)(1 << b);
        if (!(cs.pressed & bit)) continue;
        if (g_frameCount - cs.lastTap[b] <= DOUBLE_TAP_FRAMES) {
            cs.dbl |= bit;
            cs.lastTap[b] = -1000;
        }
        else {
            cs.lastTap[b] = g_frameCount;
        }
    }
    cs.held = bits;
}

// ----------------------------------------------------
// セーブデータ
// ----------------------------------------------------
static void ResetSlot(int idx) {
    memset(&g_slots[idx], 0, sizeof(SaveSlot));
    g_slots[idx].saveVersion = SAVE_VERSION;
    g_slots[idx].difficulty = 1;
}

static void SanitizeSlot(SaveSlot& s) {
    if (s.chosenChara < 0 || s.chosenChara >= CHARA_COUNT) s.chosenChara = 0;
    if (s.stageProgress > LAST_STAGE) {
        s.stageProgress = 0;
        s.needCharaSelect = true;
    }
    if (s.stageProgress < 0) s.stageProgress = 0;
    s.loopCount = ClampI(s.loopCount, 0, 99);
    s.difficulty = ClampI(s.difficulty, 0, 2);
}

static void LoadAllSlots() {
    for (int i = 0; i < 3; i++) {
        ResetSlot(i);
        char fn[32];
        sprintf_s(fn, "save_slot%d.dat", i + 1);
        FILE* fp = NULL;
        fopen_s(&fp, fn, "rb");
        if (!fp) continue;

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        SaveSlot& s = g_slots[i];
        if (size == (long)sizeof(SaveSlot)) {
            fread(&s, sizeof(SaveSlot), 1, fp);
        }
        else if (size == (long)sizeof(PrevSaveSlot)) {
            PrevSaveSlot p;
            fread(&p, sizeof(p), 1, fp);
            memcpy(&s, &p, sizeof(p));
            s.difficulty = 1;
        }
        else if (size == (long)sizeof(LegacySaveSlot)) {
            LegacySaveSlot old;
            fread(&old, sizeof(old), 1, fp);
            s.inUse = old.inUse;
            s.chosenChara = old.chosenChara;
            s.stageProgress = old.stageProgress;
            s.totalFights = old.totalFights;
            s.wins = old.wins;
            s.losses = old.losses;
            s.draws = old.draws;
            s.hasContinued = old.hasContinued;
            s.hadTimeOver = old.hadTimeOver;
            for (int c = 0; c < 5; c++) {
                if (old.charaCleared[c]) {
                    s.clearRank[c] = 1;
                    s.clearCount[c] = 1;
                    s.loopCount++;
                }
            }
            if (old.stageProgress > LAST_STAGE && old.chosenChara >= 0 && old.chosenChara < 5) {
                s.clearRank[old.chosenChara] = 2;
                if (s.loopCount == 0) s.loopCount = 1;
            }
        }
        s.saveVersion = SAVE_VERSION;
        fclose(fp);
        SanitizeSlot(s);
    }
}

static void SaveCurrentSlot() {
    if (g_currentSlot < 0 || g_currentSlot >= 3) return;
    char fn[32];
    sprintf_s(fn, "save_slot%d.dat", g_currentSlot + 1);
    FILE* fp = NULL;
    fopen_s(&fp, fn, "wb");
    if (fp) {
        g_slots[g_currentSlot].saveVersion = SAVE_VERSION;
        fwrite(&g_slots[g_currentSlot], sizeof(SaveSlot), 1, fp);
        fclose(fp);
    }
}

static void DeleteSlot(int idx) {
    if (idx < 0 || idx >= 3) return;
    ResetSlot(idx);
    char fn[32];
    sprintf_s(fn, "save_slot%d.dat", idx + 1);
    remove(fn);
}

static int GetCurrentLap() {
    return g_slots[g_currentSlot].loopCount + 1;
}

static bool IsStoryMode() {
    return !g_is_net_battle && g_mode != MODE_TRAINING && g_mode != MODE_TRAINING_SELECT && g_mode != MODE_TUTORIAL;
}

// 周回難易度（2周目で+1、最大+5）。ストーリー以外では0
static int GetHardLevel() {
    if (!IsStoryMode()) return 0;
    return ClampI(GetCurrentLap() - 1, 0, 5);
}

// CPUの強さ（0〜8）：難易度 + ステージの進み具合 + 周回
static int GetAiLevel() {
    if (g_mode == MODE_TRAINING) return 4;
    if (!IsStoryMode()) return 3;
    static const int base[3] = { 0, 1, 3 };
    static const int ramp[6] = { 0, 0, 1, 1, 2, 2 };
    const SaveSlot& s = g_slots[g_currentSlot];
    int stage = ClampI(s.stageProgress, 0, LAST_STAGE);
    return ClampI(base[s.difficulty] + ramp[stage] + (GetCurrentLap() - 1), 0, 8);
}

// コンボやハメを使ってくるのは2周目以降か「むずかしい」だけ
static bool CpuMayCombo() {
    if (g_mode == MODE_TRAINING) return true;
    if (!IsStoryMode()) return true;
    return GetCurrentLap() >= 2 || g_slots[g_currentSlot].difficulty >= 2;
}

static bool IsCharaUnlocked(int c) {
    return CHARA_PROFILES[c].unlockLap <= GetCurrentLap();
}

static bool IsCharaUnlockedAnySlot(int c) {
    for (int i = 0; i < 3; i++) {
        if (CHARA_PROFILES[c].unlockLap <= g_slots[i].loopCount + 1) return true;
    }
    return false;
}

static int RosterPosOf(int c) {
    for (int i = 0; i < ROSTER_COUNT; i++) if (g_rosterOrder[i] == c) return i;
    return 0;
}

static int CountTitles(const SaveSlot& s) {
    int n = 0;
    for (int c = 0; c < CHARA_COUNT; c++) if (s.clearRank[c] > 0) n++;
    return n;
}

// ----------------------------------------------------
// 画像（立ち絵・ポーズ・顔絵とも .bmp。黒が透過色）
//   実行フォルダ（Debug）に置く。プロジェクトフォルダから起動しても見つかるよう Debug\ も探す
// ----------------------------------------------------
static int TryLoadGraph(const char* name) {
    const char* dirs[2] = { "", "Debug\\" };
    for (int d = 0; d < 2; d++) {
        char path[MAX_PATH];
        sprintf_s(path, "%s%s", dirs[d], name);
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) continue;
        int h = LoadGraph(path);
        if (h != -1) return h;
    }
    return -1;
}

static void LoadCharaGraphs() {
    for (int c = 0; c < CHARA_COUNT; c++) {
        char name[64];
        sprintf_s(name, "%s.bmp", CHARA_IMAGE_BASE[c]);
        g_charaGraph[c] = TryLoadGraph(name);
        if (g_charaGraph[c] == -1) {
            sprintf_s(name, "%s.png", CHARA_IMAGE_BASE[c]);
            g_charaGraph[c] = TryLoadGraph(name);
        }
        g_poseGraph[c][PO_IDLE] = g_charaGraph[c];
        // ポーズと顔絵は黒背景の 24bit BMP（このDxLibはPNG読み込みが無効）
        for (int p = 1; p < PO_COUNT; p++) {
            sprintf_s(name, "%s_%s.bmp", CHARA_IMAGE_BASE[c], POSE_FILES[p]);
            g_poseGraph[c][p] = TryLoadGraph(name);
        }
        sprintf_s(name, "%s_face.bmp", CHARA_IMAGE_BASE[c]);
        g_faceGraph[c] = TryLoadGraph(name);
    }
}

static void DeleteCharaGraphs() {
    for (int c = 0; c < CHARA_COUNT; c++) {
        for (int p = 1; p < PO_COUNT; p++) {
            if (g_poseGraph[c][p] != -1) DeleteGraph(g_poseGraph[c][p]);
            g_poseGraph[c][p] = -1;
        }
        if (g_charaGraph[c] != -1) DeleteGraph(g_charaGraph[c]);
        if (g_faceGraph[c] != -1) DeleteGraph(g_faceGraph[c]);
        g_charaGraph[c] = g_faceGraph[c] = g_poseGraph[c][PO_IDLE] = -1;
    }
}

// 無いポーズは近いポーズ → 立ち絵の順で代用する
static int GetPoseGraph(int c, int pose) {
    static const int fallback[PO_COUNT] = {
        PO_IDLE, PO_IDLE, PO_IDLE, PO_IDLE, PO_LIGHT, PO_CROUCH, PO_JUMP, PO_LIGHT, PO_SPECIAL,
        PO_IDLE, PO_GUARD, PO_IDLE, PO_HIT, PO_IDLE, PO_LIGHT, PO_HIT, PO_IDLE, PO_CROUCH, PO_SPECIAL, PO_IDLE
    };
    int p = pose;
    for (int i = 0; i < 4; i++) {
        if (g_poseGraph[c][p] != -1) return g_poseGraph[c][p];
        p = fallback[p];
    }
    return g_charaGraph[c];
}
