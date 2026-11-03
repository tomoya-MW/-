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

#define WHITE   GetColor(255, 255, 255)
#define YELLOW  GetColor(255, 220, 60)
#define GRAY    GetColor(150, 150, 160)
#define RED     GetColor(255, 60, 60)
#define BLUE    GetColor(60, 130, 255)
#define GREEN   GetColor(50, 220, 110)
#define PURPLE  GetColor(190, 70, 240)
#define CYAN    GetColor(50, 220, 255)
#define ORANGE  GetColor(255, 140, 20)
#define BG_DARK GetColor(15, 15, 25)
#define BLACK   GetColor(0, 0, 0)

static const unsigned short PORT = 27015;
static const unsigned int MAGIC = 0x4455454C;
static const float GROUND_Y = 380.0f;
static const float GRAVITY = 0.65f;
static const unsigned int SAVE_VERSION = 20261103;

enum GameMode {
    MODE_TITLE,
    MODE_SLOT_SELECT,
    MODE_CHARA_SELECT,
    MODE_VS_SCREEN,
    MODE_BATTLE_PLAY,
    MODE_KO_DISPLAY,
    MODE_TIME_OVER,
    MODE_WIN_QUOTE,
    MODE_GAME_OVER,
    MODE_ENDING,
    MODE_NET_MENU,
    MODE_HOST_WAIT,
    MODE_CLIENT_INPUT,
    MODE_NET_STAGE_SELECT,
    MODE_NET_CHARA_SELECT,
    MODE_NET_BATTLE_PLAY,
    MODE_NET_RESULT,
    MODE_NET_ERROR
};

enum ActionState {
    ACT_IDLE,
    ACT_GUARD,
    ACT_THROW,
    ACT_LIGHT1,
    ACT_LIGHT2,
    ACT_MID_ATTACK,
    ACT_SKILL_ATTACK,
    ACT_AIR_KICK,
    ACT_AIR_LIGHT,
    ACT_SUPER_ATTACK,
    ACT_SUPER_AIR,
    ACT_SUPER_ENHANCED,
    ACT_PARRY,
    ACT_HIT,
    ACT_AIR_HIT,
    ACT_STUN,
    ACT_DASH,            // 2度押しダッシュ
    ACT_DODGE,           // 回避（無敵ロール）
    ACT_SLIP,            // 転倒・スリップ（無防備）
    ACT_BOAR_ROLL,       // いのししマン：サイコロ演出中
    ACT_BOAR_CHARGE,     // いのししマン：ガード不能突進
    ACT_COUNTER_STANCE,  // たけし：当身構え
    ACT_COUNTER_STRIKE,  // たけし：当身反撃
    ACT_TAUNT            // たけし：挑発
};

enum PacketType {
    PKT_HELLO = 1,
    PKT_STAGE_SYNC,
    PKT_CHARA_SYNC,
    PKT_INPUT,
    PKT_STATE,
    PKT_SPAWN_SHOT
};

#pragma pack(push, 1)
struct NetPacket {
    unsigned int magic;
    unsigned char type;
    unsigned int sequence;
    float x, y;
    float vx, vy;
    int hp;
    int state;
    int dir;
    int data1;
    int data2;
};
#pragma pack(pop)

struct FrameData {
    int startup;
    int active;
    int recovery;
};

struct CharacterProfile {
    const char* charaId;
    const char* archetype;
    const char* specialDesc;
    float walkSpeed;
    float jumpPower;
    int maxHp;
    unsigned int baseColor;
    unsigned int specialColor;
    int unlockLap;     // 何周目から使えるか（1 = 最初から）
    float dashSpeed;
};

static const int CHARA_COUNT = 9;
static CharacterProfile CHARA_PROFILES[CHARA_COUNT];

// キャラ選択画面の並び順（5:望月ママはCPU専用）
static const int g_rosterOrder[] = { 0, 1, 2, 3, 4, 7, 8, 6 };
static const int ROSTER_COUNT = sizeof(g_rosterOrder) / sizeof(g_rosterOrder[0]);

// クリア称号 [キャラ][0:ラスボス撃破 / 1:裏ボス撃破]
static const char* CHARA_TITLES[CHARA_COUNT][2] = {
    { "町内最強の拳",       "黒閃の申し子" },
    { "イライラ製造機",     "罠の帝王" },
    { "伸びる求道者",       "解脱せし手足" },
    { "運だけで勝った猪",   "天運の猪神" },
    { "物干し竿の豪傑",     "電柱一本で天下取り" },
    { "商店街の守護神",     "伝説のおかん" },
    { "始祖を継ぐ者",       "武神そのもの" },
    { "蹴りの吉田",         "伝説の右脚" },
    { "ノーガードの達人",   "全てを見切る男" }
};

struct CpuBossData {
    const char* name;
    const char* alias;
    int charaIndex;
    int aiAggro;
    int aiParryRate;
    int aiZoneKeep;
    const char* winQuote;
};

static const CpuBossData CPU_ROSTER[6] = {
    { "竜崎 猛",                           "地元では負けなしの不良",             0, 8, 2, 0, "くそっ…！ 地元の連中には見せられねぇ…！" },
    { "ドクター・ハインリヒ・フォン・メーア",   "深夜の半田ごて中毒者",               1, 2, 3, 9, "昨日基板をショートさせたのが敗因じゃ…！" },
    { "望月ママ",                           "商店街の自警団長",                   5, 8, 6, 1, "夕方のタイムセールに遅れちゃうじゃないの！" },
    { "バトバヤル・エルデネバト",             "大草原の物干し竿巨漢",               4, 8, 3, 0, "オォォ…！ 俺の大振りを見切るとは恐ろしい奴だ！" },
    { "ヴィシュヌヴァルダン・チャクラヴァルティー", "全宇宙の輪廻を統べる絶対無上の大阿羅漢", 2, 9, 8, 4, "我が解脱の境地に一撃を入れるとは…見事。" },
    { "真・天輪太子",                       "武神降臨・伝説の始祖",               6, 10, 9, 3, "……見事だ。数百年ぶりに全力を出せたよ。" }
};
static const int LAST_STAGE = 5;

struct StageDefinition {
    const char* name;
    unsigned int skyColor;
    unsigned int groundColor;
    const char* objectName;
    int objectType;
};

static StageDefinition STAGES[6];

struct StageGimmick {
    float x, y;
    float w, h;
    int maxHp;
    int hp;
    bool isFalling;
    float fallAngle;
    bool fallen;
    int activeHitBox;
    int stageTimer;
    bool hazardActive;
    float itemX, itemY;
    bool itemActive;
};
static StageGimmick g_gimmick;

static const int MAX_CHARA_SLOTS = 12;

struct SaveSlot {
    unsigned int saveVersion;
    bool inUse;
    int chosenChara;
    int stageProgress;
    int totalFights;
    int wins;
    int losses;
    int draws;
    bool hasContinued;
    bool hadTimeOver;
    int loopCount;                               // 周回クリア回数（0 = 1周目プレイ中）
    bool needCharaSelect;                        // クリア後、次の周回のキャラ選択待ち
    unsigned char clearRank[MAX_CHARA_SLOTS];    // 0:未クリア 1:ラスボス撃破 2:裏ボス撃破
    int clearCount[MAX_CHARA_SLOTS];
    bool noContinueClear[MAX_CHARA_SLOTS];
};

// 旧バージョン(〜20261102)のセーブ形式。読み込み時に移行する
struct LegacySaveSlot {
    unsigned int saveVersion;
    bool inUse;
    int chosenChara;
    int stageProgress;
    int totalFights;
    int wins;
    int losses;
    int draws;
    bool hasContinued;
    bool hadTimeOver;
    bool charaCleared[5];
};

static SaveSlot g_slots[3];
static int g_currentSlot = 0;

struct Projectile {
    bool active;
    bool isFromMe;
    float x, y;
    float vx, vy;
    int damage;
    unsigned int color;
    int life;
    int radius;
    int projType;   // 0:通常 1:ピザ 2:気弾 3:手裏剣 4:罠（着地で設置）
    int subType;    // 罠の種類
    float angle;
};
static const int MAX_SHOTS = 24;
static Projectile g_shots[MAX_SHOTS];

// 罠太郎の設置トラップ
enum TrapType { TRAP_BANANA, TRAP_LEGO, TRAP_CONE };
struct Trap {
    bool active;
    bool ownerIsMe;
    int type;
    float x;
    int life;
    int armDelay;
};
static const int MAX_TRAPS = 8;
static Trap g_traps[MAX_TRAPS];

struct HitEffect {
    bool active;
    float x, y;
    float vx, vy;
    unsigned int color;
    int life;
};
static const int MAX_FX = 180;
static HitEffect g_fx[MAX_FX];

struct SparkLine {
    bool active;
    float x, y;
    float length;
    float angle;
    unsigned int color;
    int life;
};
static const int MAX_SPARKS = 80;
static SparkLine g_sparks[MAX_SPARKS];

// 頭上に浮かぶ文字演出（WALL BOUNCE!! / BURST!! など）
struct Popup {
    bool active;
    float x, y;
    char text[96];
    unsigned int color;
    int life;
};
static const int MAX_POPUPS = 12;
static Popup g_popups[MAX_POPUPS];

static int g_screenShakeTimer = 0;
static int g_shakeOffsetX = 0;
static int g_shakeOffsetY = 0;

struct Fighter {
    float x, y;
    float vx, vy;
    int dir;
    bool isGrounded;
    int hp, maxHp;
    int charaIndex;

    ActionState state;
    int stateTimer;
    int animTimer;
    int parryWindow;
    int comboCount;
    int comboDisplayTimer;
    int hitStop;
    bool hasHit;
    bool isCounter;
    int armorFrames;

    bool fromAirLightCombo;

    float stamina;
    float maxStamina;
    int spGauge;

    int invulFrames;       // 無敵時間（回避・バースト）
    int moveDir;           // ダッシュ・回避の移動方向
    bool airDashUsed;
    bool doubleJumpUsed;
    bool wallBounceReady;  // 強い吹っ飛びを受けた直後（壁に当たるとバウンド）
    bool wallBounceUsed;   // 1コンボ中1回まで
    int comboTaken;        // 被コンボ数
    int burstCooldown;
    int stanceLevel;       // たけし当身 1:通常 2:超必
    bool boarSuper;        // いのしし確定突進
    int diceFace;
    int diceShowTimer;
    bool whiffChecked;     // 大前の空振り判定済み
    int trapCycle;
};

static Fighter g_me;
static Fighter g_other;

// 超必殺技・黒閃・パチンコ演出用変数
static int g_superCutinTimer = 0;
static const char* g_superCutinName = "";
static bool g_superIsUltimate = false;
static bool g_cutinIsCounter = false;    // 当身・極パリィ用のカットイン
static int g_kokusenStrobe = 0;          // 黒閃・空間反転ストロボタイマー
static bool g_isKokusenKoFinish = false;  // 最強技でKOしたかどうかのフラグ
static int g_rainbowCycle = 0;
static int g_flashTimer = 0;             // 当身成功時の白フラッシュ

// CPUのイライラ度（罠や挑発で上がり、ガードが雑になって大技を振りがちになる）
static int g_cpuTilt = 0;
static int g_cpuTiltTimer = 0;

static SOCKET g_listen = INVALID_SOCKET;
static SOCKET g_peer = INVALID_SOCKET;
static bool g_is_host = false;
static bool g_is_net_battle = false;
static unsigned int g_sequence = 0;
static char g_rx[sizeof(NetPacket) * 32];
static int g_rx_used = 0;

static int g_selectedNetStage = 0;
static int g_myNetChara = 0;
static int g_enemyNetChara = 1;
static int g_netCharaCursor = 0;
static bool g_myNetReady = false;
static bool g_enemyNetReady = false;
static char g_net_status[256] = "";
static int g_net_room_code = 0;
static char g_client_ip_input[64] = "127.0.0.1";

static int g_readyFightTimer = 0;
static int g_battleTimerFrames = 0;
static int g_roundTime = 99;

static GameMode g_mode = MODE_TITLE;
static int g_titleMenu = 0;
static int g_netMenu = 0;
static int g_slotCursor = 0;
static int g_charaCursor = 0;
static float g_cardScrollX = 0.0f;
static int g_parryNoticeTimer = 0;
static int g_phaseTimer = 0;
static bool g_roundWon = false;
static bool g_isDraw = false;
static int s_keyCooldown = 0;
static int g_frameCount = 0;

// 周回・クリア演出
static bool g_endingSecret = false;
static int g_endingRank = 1;
static int g_endingUnlockChara = -1;
static int g_unlockBannerTimer = 0;
static int g_unlockBannerChara = -1;
static int g_lockedMsgTimer = 0;

static void SetNonBlocking(SOCKET s) { u_long on = 1; ioctlsocket(s, FIONBIO, &on); }
static void CloseSocket(SOCKET& s) { if (s != INVALID_SOCKET) { closesocket(s); s = INVALID_SOCKET; } }

static void StopNetwork(const char* message) {
    CloseSocket(g_peer);
    CloseSocket(g_listen);
    g_mode = MODE_NET_ERROR;
    sprintf_s(g_net_status, "%s (Escでメニューへ戻る)", message);
}

static int ClampI(int v, int lo, int hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }

static bool IsHitState(ActionState s) { return s == ACT_HIT || s == ACT_AIR_HIT; }

static FrameData GetFrameData(int charaIdx, ActionState st) {
    if (st == ACT_BOAR_CHARGE) return { 0, 999, 0 };

    // キャラ固有の発生・持続
    if (charaIdx == 4) {
        if (st == ACT_LIGHT1 || st == ACT_LIGHT2) return { 7, 4, 10 };
        if (st == ACT_MID_ATTACK)   return { 18, 5, 0 };
        if (st == ACT_SKILL_ATTACK) return { 22, 5, 0 };
        if (st == ACT_SUPER_ATTACK) return { 24, 12, 0 };
    }
    else if (charaIdx == 6) {
        if (st == ACT_MID_ATTACK)   return { 2, 10, 0 };
        if (st == ACT_SUPER_ATTACK) return { 6, 30, 0 };
    }
    else if (charaIdx == 7) {
        if (st == ACT_MID_ATTACK)   return { 7, 7, 0 };
        if (st == ACT_SKILL_ATTACK) return { 5, 8, 0 };
        if (st == ACT_SUPER_ATTACK) return { 6, 42, 0 };
    }
    else if (charaIdx == 8) {
        if (st == ACT_LIGHT1) return { 4, 3, 0 };
    }

    switch (st) {
    case ACT_LIGHT1:        return { 5, 4, 9 };
    case ACT_LIGHT2:        return { 6, 4, 10 };
    case ACT_MID_ATTACK:    return { 8, 6, 14 };
    case ACT_SKILL_ATTACK:  return { 10, 6, 18 };
    case ACT_AIR_KICK:      return { 7, 8, 12 };
    case ACT_AIR_LIGHT:     return { 4, 6, 8 };
    case ACT_THROW:         return { 3, 3, 16 };
    case ACT_SUPER_ATTACK:  return { 8, 8, 25 };
    case ACT_SUPER_AIR:     return { 6, 10, 20 };
    case ACT_SUPER_ENHANCED:return { 5, 14, 28 };
    default:                return { 1, 999, 1 };
    }
}

static void InitMasterData() {
    CHARA_PROFILES[0] = { "大乗殿 力也",   "【町内会代表】 (本格空手道)",               "空N>空SPで黒閃覚醒 / SP技はノースタミナ",          5.0f, -14.0f, 1000, RED,    ORANGE, 1, 9.5f };
    CHARA_PROFILES[1] = { "罠太郎",        "【イライラ製造職人】 (罠設置・逃げ回り)",   "K:足元に罠設置 L:罠投げ(着地で設置) / 近接は激弱", 4.6f, -13.0f,  900, YELLOW, ORANGE, 1, 10.0f };
    CHARA_PROFILES[2] = { "アナンダ・シャンカラン", "【ヨガマスター】 (伸縮肢体・軟体浮遊)", "手足の先端ヒットで威力1.5倍 / 隙に被弾判定有",     5.4f, -14.5f,  880, CYAN,   WHITE,  1, 9.0f };
    CHARA_PROFILES[3] = { "いのししマン",  "【畑を荒らすスイカ泥棒】 (運任せ猪突猛進)", "J/K/L全部が突進! サイコロで ガード不能 or 大転倒", 4.6f, -13.2f,  980, GetColor(150, 95, 55), GREEN, 1, 10.0f };
    CHARA_PROFILES[4] = { "メガトン大前",  "【元大横綱の物干し竿使い】 (超リーチ・跳躍不可)", "K:画面半分届く横薙ぎ L:振り下ろし / 空振り大スキ", 2.6f, 0.0f, 1400, ORANGE, RED, 1, 6.0f };
    CHARA_PROFILES[5] = { "望月ママ",      "【商店街の自警団長】 (パイプ椅子乱舞)",     "強引な椅子制圧と猛突進 / J:通常 K:中距離 L:戦技",  4.8f, -13.5f, 1150, BLUE,   WHITE,  99, 9.0f };
    CHARA_PROFILES[6] = { "真・天輪太子",  "【武神降臨】 (封印解放・上級者向け)",       "体力激減の代わりに神速・高火力 / K:瞬動(無敵)",    5.6f, -15.0f, 1500, WHITE,  YELLOW, 3, 13.0f };
    CHARA_PROFILES[7] = { "吉田",          "【足技の鬼】 (リーチの長い蹴り主体)",       "K:飛び横蹴り(壁バウンド誘発) L:サマーソルト",       5.0f, -14.0f,  950, GREEN,  WHITE,  1, 10.0f };
    CHARA_PROFILES[8] = { "無防備 たけし", "『ノーガード戦法研究家』 (当身・パリィ特化)", "K:当身構え L:挑発(SP増) / パリィ判定が全キャラ最強", 4.2f, -13.0f,  980, PURPLE, CYAN, 2, 9.0f };

    STAGES[0] = { "STAGE 1: 竜崎のシマ（駄菓子屋前）",   GetColor(25, 20, 35), GetColor(45, 45, 50), "サビ自転車",     0 };
    STAGES[1] = { "STAGE 2: 電脳ガレージ研究所",         GetColor(15, 30, 45), GetColor(35, 50, 60), "感電コンテナ",   1 };
    STAGES[2] = { "STAGE 3: 夕暮れの商店街特売広場",     GetColor(45, 30, 20), GetColor(60, 50, 45), "特売ワゴン",     2 };
    STAGES[3] = { "STAGE 4: ウランバートル郊外ゲル",     GetColor(35, 45, 20), GetColor(60, 55, 35), "干し肉ラック",   3 };
    STAGES[4] = { "FINAL STAGE: ガンジス河畔・覚醒の祭壇", GetColor(40, 10, 35), GetColor(50, 35, 20), "黄金の蓮華石柱", 4 };
    STAGES[5] = { "EX STAGE: 天下一武道会・裏本殿",      GetColor(10, 20, 50), GetColor(40, 30, 60), "天輪の大神柱",   5 };
}

// ----------------------------------------------------
// セーブデータ
// ----------------------------------------------------
static void ResetSlot(int idx) {
    memset(&g_slots[idx], 0, sizeof(SaveSlot));
    g_slots[idx].saveVersion = SAVE_VERSION;
}

static void SanitizeSlot(SaveSlot& s) {
    if (s.chosenChara < 0 || s.chosenChara >= CHARA_COUNT) s.chosenChara = 0;
    // 旧版では裏ボス撃破後に stageProgress が 6 のまま保存され、配列外参照でエラーになっていた
    if (s.stageProgress > LAST_STAGE) {
        s.stageProgress = 0;
        s.needCharaSelect = true;
    }
    if (s.stageProgress < 0) s.stageProgress = 0;
    s.loopCount = ClampI(s.loopCount, 0, 99);
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

        if (size == (long)sizeof(SaveSlot)) {
            fread(&g_slots[i], sizeof(SaveSlot), 1, fp);
            g_slots[i].saveVersion = SAVE_VERSION;
        }
        else if (size == (long)sizeof(LegacySaveSlot)) {
            LegacySaveSlot old;
            fread(&old, sizeof(old), 1, fp);
            SaveSlot& s = g_slots[i];
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
        fclose(fp);
        SanitizeSlot(g_slots[i]);
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

// 周回難易度（2周目で+1、最大+5）
static int GetHardLevel() {
    if (g_is_net_battle) return 0;
    return ClampI(GetCurrentLap() - 1, 0, 5);
}

static bool IsCharaUnlocked(int c) {
    return CHARA_PROFILES[c].unlockLap <= GetCurrentLap();
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
// エフェクト
// ----------------------------------------------------
static void TriggerScreenShake(int frames) {
    g_screenShakeTimer = frames;
}

static void SpawnHitFX(float x, float y, unsigned int col, int count) {
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
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!g_popups[i].active) {
            g_popups[i].active = true;
            g_popups[i].x = x;
            g_popups[i].y = y;
            strcpy_s(g_popups[i].text, text);
            g_popups[i].color = col;
            g_popups[i].life = 55;
            return;
        }
    }
}

static void SpawnShot(bool fromMe, float x, float y, float vx, float vy, int dmg, unsigned int col, int rad, int type = 0, int sub = 0) {
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!g_shots[i].active) {
            g_shots[i].active = true;
            g_shots[i].isFromMe = fromMe;
            g_shots[i].x = x;
            g_shots[i].y = y;
            g_shots[i].vx = vx;
            g_shots[i].vy = vy;
            g_shots[i].damage = dmg;
            g_shots[i].color = col;
            g_shots[i].life = (type == 1) ? 180 : ((type == 4) ? 240 : 90);
            g_shots[i].radius = rad;
            g_shots[i].projType = type;
            g_shots[i].subType = sub;
            g_shots[i].angle = 0.0f;
            break;
        }
    }
}

static bool SendFighterPacket(unsigned char type, float x, float y, float vx, float vy, int hp, int st, int dir, int d1, int d2 = 0) {
    if (g_peer == INVALID_SOCKET) return false;
    NetPacket p = { MAGIC, type, ++g_sequence, x, y, vx, vy, hp, st, dir, d1, d2 };
    int n = send(g_peer, (const char*)&p, sizeof(p), 0);
    return n == sizeof(p) || (n == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
}

// ファイターが撃つ弾（自分の弾なら通信相手にも送る）
static void FighterShot(Fighter& f, float x, float y, float vx, float vy, int dmg, unsigned int col, int rad, int type = 0, int sub = 0) {
    bool fromMe = (&f == &g_me);
    SpawnShot(fromMe, x, y, vx, vy, dmg, col, rad, type, sub);
    if (fromMe && g_is_net_battle) SendFighterPacket(PKT_SPAWN_SHOT, x, y, vx, vy, dmg, sub, 0, rad, type);
}

static void ReceivePackets() {
    for (;;) {
        char incoming[512];
        int n = recv(g_peer, incoming, sizeof(incoming), 0);
        if (n == 0) { StopNetwork("相手が切断しました"); return; }
        if (n == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) return;
            StopNetwork("受信エラー"); return;
        }
        if (g_rx_used + n > (int)sizeof(g_rx)) g_rx_used = 0;
        memcpy(g_rx + g_rx_used, incoming, n);
        g_rx_used += n;

        while (g_rx_used >= (int)sizeof(NetPacket)) {
            NetPacket p;
            memcpy(&p, g_rx, sizeof(p));
            memmove(g_rx, g_rx + sizeof(p), g_rx_used - sizeof(p));
            g_rx_used -= sizeof(p);

            if (p.magic != MAGIC) continue;

            if (p.type == PKT_STAGE_SYNC) {
                g_selectedNetStage = ClampI(p.data1, 0, 4);
                g_mode = MODE_NET_CHARA_SELECT;
            }
            else if (p.type == PKT_CHARA_SYNC) {
                g_enemyNetChara = ClampI(p.data1, 0, CHARA_COUNT - 1);
                g_enemyNetReady = true;
            }
            else if (p.type == PKT_STATE || p.type == PKT_INPUT) {
                g_other.x = p.x;
                g_other.y = p.y;
                g_other.vx = p.vx;
                g_other.vy = p.vy;
                g_other.hp = p.hp;
                g_other.state = (ActionState)p.state;
                g_other.dir = p.dir;
            }
            else if (p.type == PKT_SPAWN_SHOT) {
                SpawnShot(false, p.x, p.y, p.vx, p.vy, p.hp, CHARA_PROFILES[g_other.charaIndex].specialColor, p.data1, p.data2, p.state);
            }
        }
    }
}

static void HandleTextInput(char* buf, int maxLen, bool allowDot) {
    if (s_keyCooldown > 0) { s_keyCooldown--; return; }
    int len = (int)strlen(buf);
    char pressed = 0;
    if (CheckHitKey(KEY_INPUT_0) || CheckHitKey(KEY_INPUT_NUMPAD0)) pressed = '0';
    else if (CheckHitKey(KEY_INPUT_1) || CheckHitKey(KEY_INPUT_NUMPAD1)) pressed = '1';
    else if (CheckHitKey(KEY_INPUT_2) || CheckHitKey(KEY_INPUT_NUMPAD2)) pressed = '2';
    else if (CheckHitKey(KEY_INPUT_3) || CheckHitKey(KEY_INPUT_NUMPAD3)) pressed = '3';
    else if (CheckHitKey(KEY_INPUT_4) || CheckHitKey(KEY_INPUT_NUMPAD4)) pressed = '4';
    else if (CheckHitKey(KEY_INPUT_5) || CheckHitKey(KEY_INPUT_NUMPAD5)) pressed = '5';
    else if (CheckHitKey(KEY_INPUT_6) || CheckHitKey(KEY_INPUT_NUMPAD6)) pressed = '6';
    else if (CheckHitKey(KEY_INPUT_7) || CheckHitKey(KEY_INPUT_NUMPAD7)) pressed = '7';
    else if (CheckHitKey(KEY_INPUT_8) || CheckHitKey(KEY_INPUT_NUMPAD8)) pressed = '8';
    else if (CheckHitKey(KEY_INPUT_9) || CheckHitKey(KEY_INPUT_NUMPAD9)) pressed = '9';
    else if (allowDot && (CheckHitKey(KEY_INPUT_PERIOD) || CheckHitKey(KEY_INPUT_DECIMAL))) pressed = '.';

    if (pressed != 0 && len < maxLen - 1) {
        buf[len] = pressed; buf[len + 1] = '\0';
        s_keyCooldown = 10;
    }
    if (CheckHitKey(KEY_INPUT_BACK)) {
        if (len > 0) { buf[len - 1] = '\0'; s_keyCooldown = 10; }
    }
}

static void StartHost() {
    g_is_host = true;
    g_is_net_battle = true;
    g_net_room_code = 1000 + rand() % 9000;
    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == INVALID_SOCKET) { StopNetwork("ソケット初期化失敗"); return; }
    sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(PORT);
    if (bind(g_listen, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR || listen(g_listen, 1) == SOCKET_ERROR) {
        StopNetwork("ポート27015開放失敗"); return;
    }
    SetNonBlocking(g_listen);
    g_mode = MODE_HOST_WAIT;
}

static void UpdateHostWait() {
    SOCKET s = accept(g_listen, NULL, NULL);
    if (s != INVALID_SOCKET) {
        g_peer = s; SetNonBlocking(g_peer); CloseSocket(g_listen);
        g_myNetReady = false; g_enemyNetReady = false;
        g_mode = MODE_NET_STAGE_SELECT;
        SendFighterPacket(PKT_HELLO, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

static void StartClient(const char* ip) {
    g_is_host = false;
    g_is_net_battle = true;
    g_peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_peer == INVALID_SOCKET) { StopNetwork("ソケット作成失敗"); return; }
    sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) { StopNetwork("IPアドレス書式不正"); return; }
    if (connect(g_peer, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) { StopNetwork("接続失敗"); return; }
    SetNonBlocking(g_peer);
    g_myNetReady = false; g_enemyNetReady = false;
    g_mode = MODE_NET_STAGE_SELECT;
    SendFighterPacket(PKT_HELLO, 0, 0, 0, 0, 0, 0, 0, 0);
}

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
    g_gimmick.itemActive = false;
    g_gimmick.itemX = 0;
    g_gimmick.itemY = 0;
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
    f.stamina = 100.0f;
    f.maxStamina = 100.0f;
    f.moveDir = dir;
}

// 天輪太子はプレイヤーが使うと「封印解放版」：体力が大きく下がる
static int PlayableMaxHp(int chara) {
    return (chara == 6) ? 780 : CHARA_PROFILES[chara].maxHp;
}

static void SetupBattleRound(bool isNet = false) {
    int curStage = isNet ? g_selectedNetStage : g_slots[g_currentSlot].stageProgress;
    curStage = ClampI(curStage, 0, LAST_STAGE);
    InitStageGimmick(curStage);

    for (int i = 0; i < MAX_SHOTS; i++) g_shots[i].active = false;
    for (int i = 0; i < MAX_FX; i++) g_fx[i].active = false;
    for (int i = 0; i < MAX_SPARKS; i++) g_sparks[i].active = false;
    for (int i = 0; i < MAX_TRAPS; i++) g_traps[i].active = false;
    for (int i = 0; i < MAX_POPUPS; i++) g_popups[i].active = false;

    int myChara = isNet ? g_myNetChara : g_slots[g_currentSlot].chosenChara;
    int enemyChara = isNet ? g_enemyNetChara : CPU_ROSTER[curStage].charaIndex;
    myChara = ClampI(myChara, 0, CHARA_COUNT - 1);
    enemyChara = ClampI(enemyChara, 0, CHARA_COUNT - 1);

    ResetFighter(g_me, 160.0f, 1, myChara, PlayableMaxHp(myChara));

    int enemyHp = isNet ? PlayableMaxHp(enemyChara)
        : CHARA_PROFILES[enemyChara].maxHp + curStage * 80 + GetHardLevel() * 100;
    ResetFighter(g_other, 480.0f, -1, enemyChara, enemyHp);

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
    g_cpuTilt = 0;
    g_cpuTiltTimer = 0;
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

    // 1人4個まで。超えたら一番古い罠を消す
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
        if (g_is_net_battle && &victim != &g_me) continue;
        if (!victim.isGrounded || victim.invulFrames > 0) continue;
        if (IsHitState(victim.state) || victim.state == ACT_SLIP || victim.state == ACT_STUN || victim.state == ACT_COUNTER_STRIKE) continue;
        if (fabsf(victim.x - t.x) > 22.0f) continue;

        victim.animTimer = 0;
        victim.hasHit = false;
        victim.hitStop = 6;
        victim.stanceLevel = 0;
        if (t.type == TRAP_BANANA) {
            victim.state = ACT_SLIP;
            victim.stateTimer = 55;
            victim.vx = (victim.vx != 0.0f) ? ((victim.vx > 0) ? 6.0f : -6.0f) : (float)victim.dir * 6.0f;
            victim.hp -= 15;
            SpawnPopup(victim.x, victim.y - 85, "ツルッ!!", YELLOW);
        }
        else if (t.type == TRAP_LEGO) {
            victim.state = ACT_STUN;
            victim.stateTimer = 45;
            victim.vx = 0;
            victim.hp -= 30;
            SpawnPopup(victim.x, victim.y - 85, "イッッテェ!!", RED);
        }
        else {
            victim.state = ACT_SLIP;
            victim.stateTimer = 40;
            victim.vx = (victim.x < t.x) ? -4.0f : 4.0f;
            victim.hp -= 20;
            SpawnPopup(victim.x, victim.y - 85, "ガシャーン!", ORANGE);
        }
        SpawnHitFX(t.x, GROUND_Y, YELLOW, 14);
        TriggerScreenShake(5);
        owner.spGauge += 12; if (owner.spGauge > 100) owner.spGauge = 100;
        if (&victim == &g_other) {
            AddCpuTilt(1);
            SpawnPopup(victim.x + 30, victim.y - 110, "ｲﾗｯ!!", RED);
        }
        t.active = false;
    }
}

// ----------------------------------------------------
// ダメージ・当たり判定
// ----------------------------------------------------
static void ApplyDamage(Fighter& def, Fighter& atk, int dmg, int stun, float kx, float ky, bool isLauncher, bool isSmash, bool unblockable = false) {
    // 周回難易度：CPUの攻撃力アップ
    if (!g_is_net_battle && &atk == &g_other) dmg = dmg * (100 + GetHardLevel() * 18) / 100;
    // 壁バウンド後の追撃は火力を絞る（限定追撃）
    if (def.wallBounceUsed) dmg = dmg * 7 / 10;

    if (def.state == ACT_GUARD) {
        if (!unblockable) {
            int chipDmg = dmg / 5;
            if (chipDmg < 4) chipDmg = 4;
            def.hp -= chipDmg;
            def.hitStop = 4;
            def.vx = kx * 0.4f;
            SpawnHitFX(def.x, def.y - 15, BLUE, 6);
            return;
        }
        SpawnPopup(def.x, def.y - 85, "ガード不能!!", RED);
    }

    if (def.armorFrames > 0) {
        def.hp -= (int)(dmg * 0.7f);
        def.hitStop = 4;
        SpawnHitFX(def.x, def.y - 20, ORANGE, 12);
        SpawnSparks(def.x, def.y - 20, YELLOW, 4, 25.0f);
        return;
    }

    def.hp -= dmg;
    def.hitStop = 6;
    def.stateTimer = stun;
    def.animTimer = 0;
    def.vx = kx;
    def.vy = ky;
    def.stanceLevel = 0;
    def.comboTaken++;
    def.wallBounceReady = !def.wallBounceUsed && fabsf(kx) >= 6.5f;

    atk.comboCount++;
    atk.comboDisplayTimer = 60;

    int spMul = (atk.charaIndex == 0) ? 6 : 4;
    int spBonus = 5 + (atk.comboCount * spMul);
    atk.spGauge += spBonus; if (atk.spGauge > 100) atk.spGauge = 100;
    def.spGauge += 3;       if (def.spGauge > 100) def.spGauge = 100;

    SpawnHitFX(def.x, def.y - 15, RED, 20);
    SpawnHitFX(def.x, def.y - 15, YELLOW, 15);
    SpawnSparks(def.x, def.y - 15, WHITE, 8, 35.0f);

    if (isLauncher || isSmash) {
        TriggerScreenShake(8);
        SpawnSparks(def.x, def.y - 15, YELLOW, 12, 50.0f);
    }

    if (isLauncher) {
        def.state = ACT_AIR_HIT;
        def.isGrounded = false;
    }
    else if (isSmash) {
        def.state = ACT_AIR_HIT;
    }
    else {
        def.state = def.isGrounded ? ACT_HIT : ACT_AIR_HIT;
    }
}

// たけしの当身成功：受けた技が大きいほど返しも大きい
static void TriggerCounter(Fighter& def, Fighter& atk, int incoming) {
    bool sup = (def.stanceLevel >= 2);
    int dmg = (sup ? 300 : 190) + (int)((float)incoming * (sup ? 1.5f : 1.0f));

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

    ApplyDamage(atk, def, dmg, 70, (float)def.dir * 9.0f, -17.0f, true, false, true);
    atk.hitStop = 22;
    def.hitStop = 22;

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
    SpawnPopup(def.x, def.y - 95, "COUNTER!!", CYAN);
}

static void CheckAttackHit(Fighter& atk, Fighter& def) {
    if (atk.hasHit) return;

    FrameData fd = GetFrameData(atk.charaIndex, atk.state);

    if (atk.animTimer < fd.startup || atk.animTimer >= (fd.startup + fd.active)) {
        return;
    }
    if (def.invulFrames > 0) return;

    if (atk.state == ACT_THROW) {
        if (fabsf(atk.x - def.x) < 45.0f && fabsf(atk.y - def.y) < 35.0f) {
            atk.hasHit = true;
            def.hp -= 95;
            def.state = ACT_AIR_HIT;
            def.stateTimer = 40;
            def.vx = (float)atk.dir * 8.5f;
            def.vy = -12.0f;
            def.stanceLevel = 0;
            TriggerScreenShake(8);
            SpawnHitFX(def.x, def.y - 20, YELLOW, 20);
            return;
        }
        return;
    }

    float hitBoxW = 0, hitBoxH = 0, offX = 0, offY = 0;
    int damage = 0;
    int hitStun = 20;
    float knockBackX = 0, knockBackY = 0;
    bool isLauncher = false;
    bool isAirSmash = false;
    bool unblockable = false;
    int c = atk.charaIndex;
    bool customSuper = (c == 1 || c == 4 || c == 6 || c == 7);

    if (atk.state == ACT_SUPER_AIR) {
        hitBoxW = 85; hitBoxH = 75; offX = (float)atk.dir * 45; offY = 20;
        damage = 240;
        knockBackX = (float)atk.dir * 8.0f; knockBackY = 18.0f;
        isAirSmash = true; hitStun = 55;
    }
    else if (atk.state == ACT_SUPER_ENHANCED) {
        if (atk.fromAirLightCombo) {
            hitBoxW = 100; hitBoxH = 110; offX = (float)atk.dir * 45; offY = -25;
            damage = 450;
            knockBackX = (float)atk.dir * 5.0f; knockBackY = -24.0f;
            isLauncher = true; hitStun = 80;
            TriggerScreenShake(20);
            SpawnSparks(atk.x, atk.y - 20, YELLOW, 25, 75.0f);
            SpawnSparks(atk.x, atk.y - 20, RED, 25, 75.0f);
        }
        else {
            hitBoxW = 95; hitBoxH = 90; offX = (float)atk.dir * 45; offY = -35;
            damage = 330; knockBackX = (float)atk.dir * 3.5f; knockBackY = -22.0f;
            isLauncher = true; hitStun = 70;
        }
    }
    else if (atk.state == ACT_SUPER_ATTACK && !customSuper) {
        hitBoxW = 80; hitBoxH = 50; offX = (float)atk.dir * 45; offY = -15;
        damage = 190; knockBackX = (float)atk.dir * 7.0f; knockBackY = -15.0f;
        isLauncher = true; hitStun = 60;
    }
    else if (atk.state == ACT_BOAR_CHARGE) {
        hitBoxW = 64; hitBoxH = 54; offX = (float)atk.dir * 30; offY = -10;
        damage = atk.boarSuper ? 260 : 160;
        knockBackX = (float)atk.dir * 11.0f; knockBackY = -9.0f;
        isLauncher = true; hitStun = 50; unblockable = true;
    }
    else {
        switch (c) {
        case 0:
            if (atk.state == ACT_LIGHT1) {
                hitBoxW = 48; hitBoxH = 28; offX = (float)atk.dir * 35; offY = -12;
                damage = 45; knockBackX = (float)atk.dir * 2.4f; hitStun = 18;
            }
            else if (atk.state == ACT_LIGHT2) {
                hitBoxW = 55; hitBoxH = 32; offX = (float)atk.dir * 40; offY = -10;
                damage = 55; knockBackX = (float)atk.dir * 3.2f; hitStun = 22;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                hitBoxW = 60; hitBoxH = 80; offX = (float)atk.dir * 36; offY = -40;
                damage = 125; knockBackX = (float)atk.dir * 1.5f; knockBackY = -17.5f;
                isLauncher = true; hitStun = 48;
            }
            else if (atk.state == ACT_AIR_KICK) {
                hitBoxW = 58; hitBoxH = 50; offX = (float)atk.dir * 32; offY = 22;
                damage = 135; knockBackX = (float)atk.dir * 4.0f; knockBackY = 16.5f;
                isAirSmash = true; hitStun = 42;
            }
            else if (atk.state == ACT_AIR_LIGHT) {
                hitBoxW = 52; hitBoxH = 38; offX = (float)atk.dir * 28; offY = 0;
                damage = 55; knockBackX = (float)atk.dir * 1.8f; knockBackY = -2.0f; hitStun = 28;
            }
            break;

        case 1: // 罠太郎：近接は激弱
            if (atk.state == ACT_LIGHT1) {
                hitBoxW = 30; hitBoxH = 22; offX = (float)atk.dir * 24; offY = -12;
                damage = 22; knockBackX = (float)atk.dir * 1.5f; hitStun = 14;
            }
            else if (atk.state == ACT_LIGHT2) {
                hitBoxW = 32; hitBoxH = 24; offX = (float)atk.dir * 26; offY = -12;
                damage = 28; knockBackX = (float)atk.dir * 2.0f; hitStun = 16;
            }
            else if (atk.state == ACT_AIR_LIGHT) {
                hitBoxW = 36; hitBoxH = 30; offX = (float)atk.dir * 22; offY = 0;
                damage = 25; knockBackX = (float)atk.dir * 1.5f; knockBackY = -1.0f; hitStun = 16;
            }
            break;

        case 2:
            if (atk.state == ACT_LIGHT1) {
                hitBoxW = 120; hitBoxH = 28; offX = (float)atk.dir * 80; offY = -12;
                damage = 50; knockBackX = (float)atk.dir * 3.5f; hitStun = 20;
                if (fabsf(atk.x - def.x) > 100.0f) damage = (int)(damage * 1.5f);
            }
            else if (atk.state == ACT_MID_ATTACK) {
                hitBoxW = 160; hitBoxH = 32; offX = (float)atk.dir * 105; offY = -8;
                damage = 70; knockBackX = (float)atk.dir * 5.0f; hitStun = 25;
                if (fabsf(atk.x - def.x) > 120.0f) damage = (int)(damage * 1.5f);
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                hitBoxW = 75; hitBoxH = 75; offX = (float)atk.dir * 45; offY = -28;
                damage = 140; knockBackX = (float)atk.dir * 2.2f; knockBackY = -16.5f;
                isLauncher = true; hitStun = 48;
            }
            else if (atk.state == ACT_AIR_KICK || atk.state == ACT_AIR_LIGHT) {
                hitBoxW = 85; hitBoxH = 80; offX = (float)atk.dir * 45; offY = 25;
                damage = 135; knockBackX = (float)atk.dir * 4.0f; knockBackY = 16.0f;
                isAirSmash = true; hitStun = 42;
            }
            break;

        case 4: // メガトン大前：物干し竿
            if (atk.state == ACT_LIGHT1) {
                hitBoxW = 60; hitBoxH = 40; offX = (float)atk.dir * 45; offY = -12;
                damage = 50; knockBackX = (float)atk.dir * 3.0f; hitStun = 20;
            }
            else if (atk.state == ACT_LIGHT2) {
                hitBoxW = 64; hitBoxH = 40; offX = (float)atk.dir * 48; offY = -12;
                damage = 60; knockBackX = (float)atk.dir * 3.5f; hitStun = 22;
            }
            else if (atk.state == ACT_MID_ATTACK) {
                hitBoxW = 300; hitBoxH = 55; offX = (float)atk.dir * 165; offY = -15;
                damage = 165; knockBackX = (float)atk.dir * 10.0f; knockBackY = -5.0f;
                isAirSmash = true; hitStun = 42;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                hitBoxW = 170; hitBoxH = 130; offX = (float)atk.dir * 115; offY = -25;
                damage = 190; knockBackX = (float)atk.dir * 4.0f; knockBackY = -17.0f;
                isLauncher = true; hitStun = 50;
            }
            else if (atk.state == ACT_SUPER_ATTACK) {
                hitBoxW = 1280; hitBoxH = 90; offX = 0; offY = -15;
                damage = 270; knockBackX = (float)atk.dir * 12.0f; knockBackY = -10.0f;
                isLauncher = true; hitStun = 60;
            }
            break;

        case 5:
            if (atk.state == ACT_LIGHT1) {
                hitBoxW = 62; hitBoxH = 35; offX = (float)atk.dir * 44; offY = -12;
                damage = 55; knockBackX = (float)atk.dir * 3.5f; hitStun = 22;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                hitBoxW = 68; hitBoxH = 65; offX = (float)atk.dir * 44; offY = -32;
                damage = 120; knockBackX = (float)atk.dir * 2.0f; knockBackY = -16.5f;
                isLauncher = true; hitStun = 46;
            }
            break;

        case 6:
            if (atk.state == ACT_LIGHT1) {
                hitBoxW = 70; hitBoxH = 45; offX = (float)atk.dir * 45; offY = -12;
                damage = 65; knockBackX = (float)atk.dir * 3.5f; hitStun = 25;
            }
            else if (atk.state == ACT_MID_ATTACK) {
                hitBoxW = 60; hitBoxH = 45; offX = (float)atk.dir * 35; offY = -12;
                damage = 80; knockBackX = (float)atk.dir * 4.0f; hitStun = 22;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                hitBoxW = 80; hitBoxH = 80; offX = (float)atk.dir * 45; offY = -35;
                damage = 150; knockBackX = (float)atk.dir * 2.0f; knockBackY = -18.0f;
                isLauncher = true; hitStun = 55;
            }
            else if (atk.state == ACT_SUPER_ATTACK) {
                hitBoxW = 80; hitBoxH = 60; offX = (float)atk.dir * 40; offY = -15;
                damage = 70; knockBackX = (float)atk.dir * 3.0f; knockBackY = -2.0f; hitStun = 30;
            }
            break;

        case 7: // 吉田：蹴り技はどれもリーチと火力が高い
            if (atk.state == ACT_LIGHT1) {
                hitBoxW = 70; hitBoxH = 28; offX = (float)atk.dir * 50; offY = 0;
                damage = 55; knockBackX = (float)atk.dir * 2.6f; hitStun = 20;
            }
            else if (atk.state == ACT_LIGHT2) {
                hitBoxW = 80; hitBoxH = 36; offX = (float)atk.dir * 55; offY = -20;
                damage = 70; knockBackX = (float)atk.dir * 4.0f; hitStun = 24;
            }
            else if (atk.state == ACT_MID_ATTACK) {
                hitBoxW = 90; hitBoxH = 40; offX = (float)atk.dir * 60; offY = -12;
                damage = 100; knockBackX = (float)atk.dir * 9.0f; knockBackY = -4.0f;
                isAirSmash = true; hitStun = 36;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                hitBoxW = 60; hitBoxH = 90; offX = (float)atk.dir * 30; offY = -40;
                damage = 130; knockBackX = (float)atk.dir * 2.0f; knockBackY = -18.0f;
                isLauncher = true; hitStun = 48;
            }
            else if (atk.state == ACT_AIR_LIGHT) {
                hitBoxW = 60; hitBoxH = 50; offX = (float)atk.dir * 32; offY = 20;
                damage = 95; knockBackX = (float)atk.dir * 3.0f; knockBackY = 15.0f;
                isAirSmash = true; hitStun = 36;
            }
            else if (atk.state == ACT_AIR_KICK) {
                hitBoxW = 90; hitBoxH = 50; offX = (float)atk.dir * 30; offY = 10;
                damage = 120; knockBackX = (float)atk.dir * 7.5f; knockBackY = -6.0f;
                isAirSmash = true; hitStun = 40;
            }
            else if (atk.state == ACT_SUPER_ATTACK) {
                hitBoxW = 80; hitBoxH = 60; offX = (float)atk.dir * 45; offY = -10;
                damage = 42; knockBackX = (float)atk.dir * 2.5f; knockBackY = -1.0f; hitStun = 30;
            }
            break;

        case 8: // 無防備たけし：自分の攻撃は雀の涙
            if (atk.state == ACT_LIGHT1) {
                hitBoxW = 24; hitBoxH = 22; offX = (float)atk.dir * 20; offY = -12;
                damage = 26; knockBackX = (float)atk.dir * 1.2f; hitStun = 14;
            }
            else if (atk.state == ACT_LIGHT2) {
                hitBoxW = 26; hitBoxH = 24; offX = (float)atk.dir * 22; offY = -12;
                damage = 30; knockBackX = (float)atk.dir * 1.6f; hitStun = 16;
            }
            else if (atk.state == ACT_AIR_LIGHT) {
                hitBoxW = 28; hitBoxH = 26; offX = (float)atk.dir * 20; offY = 0;
                damage = 28; knockBackX = (float)atk.dir * 1.2f; hitStun = 16;
            }
            break;
        }
    }

    if (damage == 0) return;

    float ax = atk.x + offX;
    float ay = atk.y + offY;

    if (!g_gimmick.fallen && !g_gimmick.isFalling) {
        if (fabsf(ax - g_gimmick.x) < 50.0f && ay > GROUND_Y - 140.0f) {
            g_gimmick.hp -= damage;
            SpawnHitFX(g_gimmick.x, GROUND_Y - 60, YELLOW, 8);
            if (g_gimmick.hp <= 0) {
                g_gimmick.isFalling = true;
                g_gimmick.fallAngle = 0.0f;
                g_gimmick.activeHitBox = atk.dir;
                TriggerScreenShake(12);
                SpawnHitFX(g_gimmick.x, GROUND_Y - 40, ORANGE, 30);
                SpawnSparks(g_gimmick.x, GROUND_Y - 40, YELLOW, 10, 45.0f);

                if (g_slots[g_currentSlot].stageProgress == 2) {
                    g_gimmick.itemActive = true;
                    g_gimmick.itemX = g_gimmick.x;
                    g_gimmick.itemY = GROUND_Y;
                }
            }
        }
    }

    float defHitW = (def.charaIndex == 4) ? 48.0f : 30.0f;
    float defHitH = (def.charaIndex == 4) ? 60.0f : 50.0f;
    float defCenterX = def.x;
    float defCenterY = def.y;

    if (def.charaIndex == 2) {
        if (def.state == ACT_LIGHT1) {
            defHitW = 130.0f;
            defCenterX = def.x + (float)def.dir * 65.0f;
        }
        else if (def.state == ACT_MID_ATTACK) {
            defHitW = 160.0f;
            defCenterX = def.x + (float)def.dir * 80.0f;
        }
    }

    if (fabsf(ax - defCenterX) < (hitBoxW + defHitW) / 2.0f && fabsf(ay - defCenterY) < (hitBoxH + defHitH) / 2.0f) {
        atk.hasHit = true;

        // 当身（たけし）：構えの最中に殴られたら即反撃
        if (def.state == ACT_COUNTER_STANCE && def.animTimer >= 2 && (def.stanceLevel >= 2 || def.animTimer <= 32)) {
            TriggerCounter(def, atk, damage);
            return;
        }

        if (def.state == ACT_PARRY && def.parryWindow > 0) {
            bool master = (def.charaIndex == 8);
            atk.state = ACT_STUN;
            atk.stateTimer = master ? 75 : 42;
            atk.animTimer = 0;
            atk.hitStop = master ? 14 : 8;
            atk.vx = (float)-atk.dir * 4.5f;
            def.isCounter = true;
            def.spGauge += master ? 40 : 20; if (def.spGauge > 100) def.spGauge = 100;
            g_parryNoticeTimer = 35;
            TriggerScreenShake(master ? 12 : 6);
            SpawnHitFX(def.x, def.y - 15, CYAN, 25);
            SpawnSparks(def.x, def.y - 15, CYAN, 12, 40.0f);
            if (master) {
                g_flashTimer = 14;
                g_superCutinTimer = 40;
                g_superIsUltimate = false;
                g_cutinIsCounter = true;
                g_superCutinName = "JUST PARRY・極 ――『ニヤリ』";
                SpawnSparks(def.x, def.y - 15, YELLOW, 16, 55.0f);
                SpawnPopup(def.x, def.y - 90, "ﾆﾔﾘ…", CYAN);
            }
            return;
        }

        if (atk.isCounter) {
            damage = (int)((float)damage * ((atk.charaIndex == 8) ? 2.5f : 1.5f));
            hitStun += 10;
            atk.isCounter = false;
            SpawnHitFX(def.x, def.y, YELLOW, 20);
            SpawnSparks(def.x, def.y, YELLOW, 8, 30.0f);
        }

        atk.hitStop = 6;

        // トドメ（KO）フラグのチェック
        if (def.hp <= damage && atk.fromAirLightCombo) {
            g_isKokusenKoFinish = true;
        }

        ApplyDamage(def, atk, damage, hitStun, knockBackX, knockBackY, isLauncher, isAirSmash, unblockable);
        if (atk.state == ACT_BOAR_CHARGE) atk.vx *= 0.3f;
    }
}

static void UpdateStageGimmick(int stageIdx) {
    g_gimmick.stageTimer++;

    if (stageIdx == 1) {
        if (g_gimmick.stageTimer % 240 > 180) {
            g_gimmick.hazardActive = true;
            Fighter* fList[2] = { &g_me, &g_other };
            for (int i = 0; i < 2; i++) {
                if (fList[i]->isGrounded && fList[i]->state != ACT_GUARD) {
                    fList[i]->hp -= 1;
                    if (rand() % 12 == 0) SpawnHitFX(fList[i]->x, GROUND_Y, CYAN, 4);
                }
            }
        }
        else {
            g_gimmick.hazardActive = false;
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
            if (fabsf(targets[i]->x - tipX) < 45.0f && fabsf(targets[i]->y - tipY) < 40.0f) {
                targets[i]->hp -= 120;
                targets[i]->state = ACT_STUN;
                targets[i]->stateTimer = 65;
                targets[i]->animTimer = 0;
                targets[i]->hitStop = 10;
                targets[i]->vx = (float)g_gimmick.activeHitBox * 4.0f;
                TriggerScreenShake(14);
                SpawnHitFX(targets[i]->x, targets[i]->y - 20, YELLOW, 30);
                SpawnSparks(targets[i]->x, targets[i]->y - 20, ORANGE, 10, 45.0f);
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

// ----------------------------------------------------
// 共通の物理・状態更新（プレイヤー / CPU 共通）
// ----------------------------------------------------

// 壁バウンド：強い吹っ飛びで画面端に激突すると跳ね返り、1回だけ追撃できる
static void HitWall(Fighter& f, int pushDir) {
    bool hitState = IsHitState(f.state);
    if (hitState && f.wallBounceReady && !f.wallBounceUsed && fabsf(f.vx) > 2.5f) {
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
        SpawnPopup(f.x + (float)pushDir * 60.0f, f.y - 100, "WALL BOUNCE!! 追撃チャンス", ORANGE);
    }
    else if (f.state == ACT_AIR_HIT) {
        f.vx = (float)pushDir * 4.0f;
    }
}

static bool UpdateFighterCommon(Fighter& f, Fighter& opp, bool isPlayer) {
    if (f.hitStop > 0) { f.hitStop--; return false; }

    if (f.armorFrames > 0) f.armorFrames--;
    if (f.parryWindow > 0) f.parryWindow--;
    if (f.invulFrames > 0) f.invulFrames--;
    if (f.burstCooldown > 0) f.burstCooldown--;
    if (f.diceShowTimer > 0) f.diceShowTimer--;

    if (f.state == ACT_IDLE || f.state == ACT_GUARD || f.state == ACT_DASH) {
        f.stamina += 0.75f;
        if (f.stamina > f.maxStamina) f.stamina = f.maxStamina;
    }

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
            f.fromAirLightCombo = false;
            f.stanceLevel = 0;
            f.boarSuper = false;
        }
    }

    if (isPlayer) {
        int curStage = g_is_net_battle ? g_selectedNetStage : g_slots[g_currentSlot].stageProgress;
        if (curStage == 3 && !f.isGrounded) {
            f.vx -= 0.15f;
        }
    }

    f.vy += GRAVITY;
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
            f.vy = 0;
            f.isGrounded = true;
            f.airDashUsed = false;
            f.doubleJumpUsed = false;
            if (f.state == ACT_AIR_HIT || f.state == ACT_AIR_KICK || f.state == ACT_SUPER_AIR) {
                f.state = ACT_IDLE;
                f.animTimer = 0;
                f.fromAirLightCombo = false;
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
        f.comboTaken = 0;
    }
    return true;
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
}

static void StartDodge(Fighter& f, int moveDir) {
    f.state = ACT_DODGE;
    f.moveDir = moveDir;
    f.stateTimer = 22;
    f.animTimer = 0;
    f.hasHit = false;
    f.invulFrames = 15;
    f.vx = (float)moveDir * 7.5f;
    SpawnHitFX(f.x, f.y + 10, WHITE, 6);
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
    SpawnPopup(f.x, f.y - 90, "BURST!! コンボ抜け", CYAN);
    return true;
}

// 通常技・必殺技の発動（btn 0:J 1:K 2:L）。プレイヤー・CPU共通
static bool StartAttack(Fighter& f, Fighter& opp, int btn) {
    int c = f.charaIndex;

    // いのししマン：どのボタンでも突進（サイコロ判定）
    if (c == 3) {
        if (!f.isGrounded) return false;
        f.state = ACT_BOAR_ROLL;
        f.stateTimer = 999;
        f.animTimer = 0;
        f.hasHit = false;
        f.boarSuper = false;
        f.vx = 0;
        return true;
    }

    if (btn == 1 && !f.isGrounded) return false;
    if (btn == 2 && !f.isGrounded && c == 8) return false;

    f.hasHit = false;
    f.animTimer = 0;
    f.whiffChecked = false;

    if (btn == 0) {
        if (f.isGrounded) {
            f.state = ACT_LIGHT1;
            f.stateTimer = (c == 8) ? 14 : 18;
            if (c == 2) f.vx = 0;
            else if (c == 8 || c == 1) f.vx = (float)f.dir * 1.0f;
            else f.vx = (float)f.dir * 3.0f;
        }
        else {
            f.state = ACT_AIR_LIGHT;
            f.stateTimer = 17;
        }
        return true;
    }

    if (btn == 1) {
        f.state = ACT_MID_ATTACK;
        f.stateTimer = 28;
        switch (c) {
        case 0:
            f.vx = (float)-f.dir * 2.0f;
            FighterShot(f, f.x + f.dir * 32, f.y - 16, f.dir * 10.5f, 0, 65, CYAN, 12, 2);
            break;
        case 1: // 足元に罠を落として逃げる
            FighterShot(f, f.x - f.dir * 10, f.y - 20, -f.dir * 1.5f, -3.0f, 0, ORANGE, 10, 4, f.trapCycle);
            f.trapCycle = (f.trapCycle + 1) % 3;
            f.vx = (float)-f.dir * 6.0f;
            f.stateTimer = 20;
            break;
        case 2:
            f.vx = (float)-f.dir * 1.5f;
            break;
        case 4:
            f.stateTimer = 30;
            f.vx = 0;
            break;
        case 6: // 瞬動
            f.stateTimer = 22;
            f.vx = (float)f.dir * 15.0f;
            f.invulFrames = 12;
            SpawnHitFX(f.x, f.y - 15, WHITE, 12);
            break;
        case 7:
            f.stateTimer = 30;
            f.vx = (float)f.dir * 7.0f;
            break;
        case 8: // 当身構え
            f.state = ACT_COUNTER_STANCE;
            f.stateTimer = 40;
            f.stanceLevel = 1;
            f.vx = 0;
            break;
        }
        return true;
    }

    // btn == 2
    if (f.isGrounded) {
        f.state = ACT_SKILL_ATTACK;
        f.stateTimer = 34;
        switch (c) {
        case 0:
            f.vx = (float)f.dir * 3.5f;
            f.vy = -16.5f;
            f.isGrounded = false;
            TriggerScreenShake(6);
            break;
        case 1: // 罠を放り投げる（当たれば小ダメージ、外れても着地点に設置）
            FighterShot(f, f.x + f.dir * 20, f.y - 30, f.dir * 6.5f, -7.5f, 40, ORANGE, 10, 4, f.trapCycle);
            f.trapCycle = (f.trapCycle + 1) % 3;
            f.stateTimer = 26;
            f.vx = 0;
            break;
        case 2:
            f.vx = (float)f.dir * 8.5f;
            SpawnHitFX(f.x, f.y - 15, CYAN, 20);
            break;
        case 4:
            f.vx = 0;
            break;
        case 7: // サマーソルト
            f.stateTimer = 30;
            f.vx = (float)f.dir * 2.5f;
            f.vy = -13.0f;
            f.isGrounded = false;
            break;
        case 8: // 挑発
            f.state = ACT_TAUNT;
            f.stateTimer = 45;
            f.vx = 0;
            break;
        }
    }
    else {
        f.state = ACT_AIR_KICK;
        if (c == 1) {
            f.stateTimer = 18;
            FighterShot(f, f.x, f.y - 10, f.dir * 1.0f, 2.0f, 30, ORANGE, 10, 4, f.trapCycle);
            f.trapCycle = (f.trapCycle + 1) % 3;
        }
        else if (c == 7) { // 旋風脚
            f.stateTimer = 24;
            f.vx = (float)f.dir * 5.0f;
            f.vy = -2.0f;
        }
        else {
            f.stateTimer = 27;
            f.vx = (float)f.dir * 6.0f;
            f.vy = 16.0f;
            SpawnHitFX(f.x, f.y, YELLOW, 10);
        }
    }
    return true;
}

// 地上超必殺技（J+K）
static bool StartSuper(Fighter& f, Fighter& opp) {
    f.spGauge = 0;
    f.hasHit = false;
    f.animTimer = 0;
    f.whiffChecked = false;
    f.fromAirLightCombo = false;
    g_superCutinTimer = 45;
    g_superIsUltimate = false;
    g_cutinIsCounter = false;
    TriggerScreenShake(14);

    switch (f.charaIndex) {
    case 0:
        if ((float)f.hp / (float)f.maxHp < 0.25f) {
            f.state = ACT_SUPER_ENHANCED;
            f.stateTimer = 55;
            f.vx = (float)f.dir * 7.5f;
            f.vy = -16.0f;
            f.isGrounded = false;
            g_superCutinName = "=== DRAGON SUPLEX LIFT !!! ===";
            SpawnHitFX(f.x, f.y, RED, 40);
        }
        else {
            f.state = ACT_SUPER_ATTACK;
            f.stateTimer = 45;
            f.vx = (float)f.dir * 12.0f;
            g_superCutinName = "=== ROLLING SOBAT RUSH ! ===";
            SpawnHitFX(f.x, f.y, ORANGE, 30);
        }
        break;
    case 1:
        f.state = ACT_SUPER_ATTACK;
        f.stateTimer = 40;
        g_superCutinName = "=== 大特価!! 罠バラまきセール ! ===";
        for (int i = 0; i < 5; i++) {
            FighterShot(f, f.x + f.dir * 15, f.y - 30, f.dir * (2.5f + i * 2.0f), -8.0f - i * 0.5f, 30, ORANGE, 10, 4, i % 3);
        }
        break;
    case 2:
        f.state = ACT_SUPER_ATTACK;
        f.stateTimer = 50;
        g_superCutinName = "=== BRAHMAN KUNDALINI BURST ! ===";
        for (int an = 0; an < 8; an++) {
            float ang = (float)an * 3.14159f / 4.0f;
            FighterShot(f, f.x, f.y - 20, cosf(ang) * 9.0f, sinf(ang) * 9.0f, 40, CYAN, 14, 2);
        }
        break;
    case 3:
        f.state = ACT_BOAR_ROLL;
        f.stateTimer = 999;
        f.boarSuper = true;
        f.vx = 0;
        g_superCutinName = "=== 確変!! 猪突猛進・大当たり確定 ! ===";
        break;
    case 4:
        f.state = ACT_SUPER_ATTACK;
        f.stateTimer = 55;
        f.armorFrames = 25;
        f.vx = 0;
        g_superCutinName = "=== 電柱フルスイング・天地轟断 ! ===";
        TriggerScreenShake(18);
        break;
    case 6:
        f.state = ACT_SUPER_ATTACK;
        f.stateTimer = 45;
        f.x = opp.x - (float)f.dir * 50.0f;
        f.vx = (float)f.dir * 6.0f;
        g_superCutinName = "=== 天輪・神速百烈拳 ! ===";
        SpawnHitFX(f.x, f.y - 20, YELLOW, 30);
        break;
    case 7:
        f.state = ACT_SUPER_ATTACK;
        f.stateTimer = 55;
        f.vx = (float)f.dir * 4.0f;
        g_superCutinName = "=== 吉田式・百烈脚 ! ===";
        break;
    case 8:
        f.state = ACT_COUNTER_STANCE;
        f.stateTimer = 90;
        f.stanceLevel = 2;
        f.vx = 0;
        g_superCutinName = "=== 奥義・無防備の極み (全反撃構え) ===";
        break;
    default:
        f.state = ACT_SUPER_ATTACK;
        f.stateTimer = 45;
        g_superCutinName = "=== SUPER ATTACK ! ===";
        break;
    }
    return true;
}

// 状態ごとの毎フレーム処理（空振り・サイコロ・ダッシュ等）
static void UpdateStateSpecials(Fighter& f, Fighter& opp) {
    int c = f.charaIndex;

    // 大前：空振りすると約1秒の大スキ
    if (c == 4 && !f.whiffChecked && (f.state == ACT_MID_ATTACK || f.state == ACT_SKILL_ATTACK || f.state == ACT_SUPER_ATTACK)) {
        FrameData fd = GetFrameData(c, f.state);
        if (f.animTimer >= fd.startup + fd.active) {
            f.whiffChecked = true;
            if (!f.hasHit) {
                f.stateTimer += (f.state == ACT_SUPER_ATTACK) ? 40 : 55;
                SpawnPopup(f.x, f.y - 95, "スカッ…（大スキ!!）", GRAY);
            }
        }
    }

    // 多段ヒットの超必殺
    if ((c == 6 || c == 7) && f.state == ACT_SUPER_ATTACK && f.animTimer % 6 == 0) f.hasHit = false;
    if (c == 7 && f.state == ACT_SUPER_ATTACK) f.vx = (float)f.dir * 2.5f;

    switch (f.state) {
    case ACT_BOAR_ROLL: {
        f.vx = 0;
        if (f.animTimer % 3 == 0) f.diceFace = 1 + rand() % 6;
        int rollLen = f.boarSuper ? 16 : 30;
        if (f.animTimer >= rollLen) {
            bool success = f.boarSuper || (rand() % 2 == 0);
            f.diceShowTimer = 45;
            f.animTimer = 0;
            f.hasHit = false;
            if (success) {
                f.diceFace = 6;
                f.state = ACT_BOAR_CHARGE;
                f.stateTimer = f.boarSuper ? 40 : 30;
                f.vx = (float)f.dir * (f.boarSuper ? 19.0f : 17.0f);
                TriggerScreenShake(8);
                SpawnPopup(f.x, f.y - 95, "大当たり!! ガード不能突進!!", YELLOW);
            }
            else {
                f.diceFace = 1;
                f.state = ACT_SLIP;
                f.stateTimer = 75;
                f.vx = (float)f.dir * 5.0f;
                SpawnPopup(f.x, f.y - 95, "ズコーッ!! 大転倒…", GRAY);
            }
        }
        break;
    }
    case ACT_BOAR_CHARGE:
        if (!f.hasHit) f.vx = (float)f.dir * (f.boarSuper ? 19.0f : 17.0f);
        if (f.animTimer % 3 == 0) SpawnHitFX(f.x - (float)f.dir * 20.0f, f.y + 15, GRAY, 2);
        break;
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
        if (f.animTimer == 2) SpawnPopup(f.x, f.y - 95, "ほらほら、打ってきなよ？", CYAN);
        if (f.animTimer == 40) {
            f.spGauge += 25; if (f.spGauge > 100) f.spGauge = 100;
            SpawnPopup(f.x, f.y - 75, "SP UP!", YELLOW);
            if (&f == &g_me) {
                AddCpuTilt(2);
                if (!g_is_net_battle) SpawnPopup(opp.x, opp.y - 110, "ｲﾗｲﾗ…!!", RED);
            }
        }
        break;
    default:
        break;
    }
}

// ----------------------------------------------------
// プレイヤー入力
// ----------------------------------------------------
struct KeyTrack {
    bool held;
    bool pressed;
    bool doubleTap;
    int lastTap;
};
static const int DOUBLE_TAP_FRAMES = 14;
static KeyTrack g_kA = { false, false, false, -1000 };
static KeyTrack g_kD = { false, false, false, -1000 };
static KeyTrack g_kW = { false, false, false, -1000 };
static KeyTrack g_kS = { false, false, false, -1000 };
static KeyTrack g_kSpace = { false, false, false, -1000 };
static KeyTrack g_kDodge = { false, false, false, -1000 };

static void TrackKey(KeyTrack& k, bool now) {
    k.pressed = now && !k.held;
    k.doubleTap = false;
    if (k.pressed) {
        if (g_frameCount - k.lastTap <= DOUBLE_TAP_FRAMES) {
            k.doubleTap = true;
            k.lastTap = -1000;
        }
        else {
            k.lastTap = g_frameCount;
        }
    }
    k.held = now;
}

static void UpdatePlayer() {
    TrackKey(g_kA, CheckHitKey(KEY_INPUT_A) != 0);
    TrackKey(g_kD, CheckHitKey(KEY_INPUT_D) != 0);
    TrackKey(g_kW, CheckHitKey(KEY_INPUT_W) != 0);
    TrackKey(g_kS, CheckHitKey(KEY_INPUT_S) != 0);
    TrackKey(g_kSpace, CheckHitKey(KEY_INPUT_SPACE) != 0);
    TrackKey(g_kDodge, CheckHitKey(KEY_INPUT_I) != 0 || CheckHitKey(KEY_INPUT_LSHIFT) != 0);

    if (!UpdateFighterCommon(g_me, g_other, true)) return;
    if (g_readyFightTimer < 80) return;

    UpdateStateSpecials(g_me, g_other);
    int c = g_me.charaIndex;

    // ==========================================
    // コンボ抜け（被弾中に SPACE）
    // ==========================================
    if (IsHitState(g_me.state) && g_kSpace.pressed) {
        if (!TryBurst(g_me, g_other)) {
            SpawnPopup(g_me.x, g_me.y - 90, (g_me.burstCooldown > 0) ? "バースト再使用まで待機" : "バーストにはSP50必要", GRAY);
        }
    }

    // ==========================================
    // 空N（空中J）＞空SP（黒閃＆パチンコ風覚醒キャンセル）
    // ==========================================
    if (c == 0 && g_me.state == ACT_AIR_LIGHT && !g_me.isGrounded) {
        if (CheckHitKey(KEY_INPUT_J) && CheckHitKey(KEY_INPUT_K) && g_me.spGauge >= 100) {
            g_me.spGauge = 0;
            g_me.hasHit = false;
            g_me.animTimer = 0;
            g_me.fromAirLightCombo = true;
            g_me.state = ACT_SUPER_ENHANCED;
            g_me.stateTimer = 55;
            g_me.vx = (float)g_me.dir * 11.5f;
            g_me.vy = 12.0f;
            g_superCutinTimer = 60;
            g_superIsUltimate = true;
            g_cutinIsCounter = false;
            g_kokusenStrobe = 45; // 45フレーム空間反転
            g_superCutinName = "【大当り】真・極点昇天破!! (確変突入)";
            TriggerScreenShake(22);
            // 空間を裂く黒閃の稲妻スパーク
            SpawnSparks(g_me.x, g_me.y, RED, 30, 80.0f);
            SpawnSparks(g_me.x, g_me.y, YELLOW, 30, 80.0f);
            return;
        }
    }

    // 空中SP技
    if (c == 0 && !g_me.isGrounded && g_me.state == ACT_IDLE) {
        if (CheckHitKey(KEY_INPUT_J) && CheckHitKey(KEY_INPUT_K) && g_me.spGauge >= 100) {
            g_me.spGauge = 0;
            g_me.hasHit = false;
            g_me.animTimer = 0;
            g_me.fromAirLightCombo = false;
            g_me.state = ACT_SUPER_AIR;
            g_me.stateTimer = 45;
            g_me.vx = (float)g_me.dir * 9.5f;
            g_me.vy = 14.0f;
            g_superCutinTimer = 45;
            g_superIsUltimate = false;
            g_cutinIsCounter = false;
            g_superCutinName = "=== 天翔・金剛螺旋脚 (AIR SUPER) ===";
            TriggerScreenShake(12);
            SpawnSparks(g_me.x, g_me.y, ORANGE, 15, 45.0f);
            return;
        }
    }

    // ==========================================
    // 空中アクション：W再入力で2段ジャンプ / SS急降下 / AA・DD空中ダッシュ
    // ==========================================
    if (!g_me.isGrounded && (g_me.state == ACT_IDLE || g_me.state == ACT_AIR_LIGHT)) {
        if (g_me.state == ACT_IDLE && g_kW.pressed && !g_me.doubleJumpUsed && c != 4) {
            g_me.vy = CHARA_PROFILES[c].jumpPower * 0.85f;
            g_me.doubleJumpUsed = true;
            SpawnHitFX(g_me.x, g_me.y + 10, WHITE, 8);
        }
        if (g_kS.doubleTap) {
            g_me.vy = 17.0f;
            SpawnHitFX(g_me.x, g_me.y - 30, WHITE, 6);
        }
        if (g_me.state == ACT_IDLE && !g_me.airDashUsed && (g_kA.doubleTap || g_kD.doubleTap)) {
            int md = g_kA.doubleTap ? -1 : 1;
            g_me.vx = (float)md * CHARA_PROFILES[c].dashSpeed * 0.9f;
            if (g_me.vy > -2.0f) g_me.vy = -2.0f;
            g_me.airDashUsed = true;
            SpawnHitFX(g_me.x - (float)md * 15.0f, g_me.y - 10, WHITE, 6);
        }
    }

    bool isBackKey = (g_me.dir == 1) ? CheckHitKey(KEY_INPUT_A) : CheckHitKey(KEY_INPUT_D);
    if (g_me.state == ACT_IDLE && g_me.isGrounded && isBackKey) {
        g_me.state = ACT_GUARD;
    }
    else if (g_me.state == ACT_GUARD && (!isBackKey || !g_me.isGrounded)) {
        g_me.state = ACT_IDLE;
    }

    // 地上ダッシュ（AA / DD）
    if (g_me.isGrounded && (g_me.state == ACT_IDLE || g_me.state == ACT_GUARD) && (g_kA.doubleTap || g_kD.doubleTap) && g_me.stamina >= 6.0f) {
        g_me.stamina -= 6.0f;
        StartDash(g_me, g_kA.doubleTap ? -1 : 1);
    }

    // 回避（I / 左Shift）：押している方向へ、何も押していなければ後ろへ無敵ロール
    if (g_kDodge.pressed && g_me.isGrounded && (g_me.state == ACT_IDLE || g_me.state == ACT_GUARD || g_me.state == ACT_DASH) && g_me.stamina >= 18.0f) {
        int md = CheckHitKey(KEY_INPUT_A) ? -1 : (CheckHitKey(KEY_INPUT_D) ? 1 : -g_me.dir);
        g_me.stamina -= 18.0f;
        StartDodge(g_me, md);
    }

    if (g_me.state == ACT_IDLE || g_me.state == ACT_GUARD || g_me.state == ACT_DASH) {
        bool dashing = (g_me.state == ACT_DASH);
        float speed = CHARA_PROFILES[c].walkSpeed;

        if (!dashing) {
            if (CheckHitKey(KEY_INPUT_A)) g_me.vx = -speed;
            if (CheckHitKey(KEY_INPUT_D)) g_me.vx = speed;
        }

        if (CheckHitKey(KEY_INPUT_W) && g_me.isGrounded && c != 4) {
            g_me.vy = CHARA_PROFILES[c].jumpPower;
            g_me.isGrounded = false;
            if (dashing) { g_me.state = ACT_IDLE; g_me.stateTimer = 0; g_me.animTimer = 0; }
        }

        int jCost = (c == 3) ? 20 : 8;
        int kCost = (c == 3) ? 20 : 22;
        int lCost = (c == 3) ? 20 : 32;

        bool isForwardKey = (g_me.dir == 1) ? CheckHitKey(KEY_INPUT_D) : CheckHitKey(KEY_INPUT_A);
        if (isForwardKey && CheckHitKey(KEY_INPUT_J) && g_me.isGrounded && g_me.stamina >= 15.0f) {
            // 投げ
            g_me.stamina -= 15.0f;
            g_me.state = ACT_THROW;
            g_me.stateTimer = 22;
            g_me.animTimer = 0;
            g_me.hasHit = false;
            return;
        }
        else if (CheckHitKey(KEY_INPUT_SPACE) && g_me.isGrounded && g_me.stamina >= 10.0f) {
            // パリィ（たけしは受付が約3倍）
            g_me.stamina -= 10.0f;
            g_me.state = ACT_PARRY;
            g_me.stateTimer = (c == 8) ? 24 : 18;
            g_me.animTimer = 0;
            g_me.parryWindow = (c == 8) ? 14 : 5;
            g_me.vx = 0;
        }
        else if (CheckHitKey(KEY_INPUT_J) && CheckHitKey(KEY_INPUT_K) && g_me.spGauge >= 100 && g_me.isGrounded) {
            StartSuper(g_me, g_other);
            return;
        }
        else if (CheckHitKey(KEY_INPUT_J) && g_me.stamina >= jCost) {
            if (StartAttack(g_me, g_other, 0)) g_me.stamina -= jCost;
        }
        else if (CheckHitKey(KEY_INPUT_K) && g_me.stamina >= kCost) {
            if (StartAttack(g_me, g_other, 1)) g_me.stamina -= kCost;
        }
        else if (CheckHitKey(KEY_INPUT_L) && g_me.stamina >= lCost) {
            if (StartAttack(g_me, g_other, 2)) g_me.stamina -= lCost;
        }
    }
    else if (g_me.state == ACT_LIGHT1 && g_me.hasHit && CheckHitKey(KEY_INPUT_J) && g_me.stamina >= 8.0f) {
        g_me.stamina -= 8.0f;
        g_me.state = ACT_LIGHT2;
        g_me.stateTimer = 20;
        g_me.animTimer = 0;
        g_me.hasHit = false;
        g_me.vx = (float)g_me.dir * ((c == 8 || c == 1) ? 1.5f : 3.5f);
    }

    CheckAttackHit(g_me, g_other);
}

// ----------------------------------------------------
// CPU思考
// ----------------------------------------------------
static int CpuFreq(int base) {
    int v = base - GetHardLevel() * 4 - g_cpuTilt * 3;
    return (v < 4) ? 4 : v;
}

static void CpuStartState(ActionState st, int timer) {
    g_other.state = st;
    g_other.stateTimer = timer;
    g_other.animTimer = 0;
    g_other.hasHit = false;
    g_other.whiffChecked = false;
}

static void CpuThink(int stage, const CpuBossData& boss) {
    int hard = GetHardLevel();
    float dist = fabsf(g_me.x - g_other.x);
    float speed = CHARA_PROFILES[g_other.charaIndex].walkSpeed;

    // 相手の空振り・転倒・挑発には全力で差し込む
    bool playerOpen = (g_me.whiffChecked && !g_me.hasHit &&
        (g_me.state == ACT_MID_ATTACK || g_me.state == ACT_SKILL_ATTACK || g_me.state == ACT_SUPER_ATTACK))
        || g_me.state == ACT_SLIP || g_me.state == ACT_TAUNT;
    if (playerOpen) {
        g_other.vx = (float)g_other.dir * speed * 2.2f;
        if (dist <= 90.0f && g_other.isGrounded) CpuStartState(ACT_SKILL_ATTACK, 34);
        return;
    }

    // 周回難易度：大技を見てから回避
    bool bigThreat = (g_me.state == ACT_SKILL_ATTACK || g_me.state == ACT_MID_ATTACK ||
        g_me.state == ACT_SUPER_ATTACK || g_me.state == ACT_BOAR_CHARGE) && g_me.animTimer <= 2;
    if (hard >= 1 && bigThreat && dist < 320.0f && g_other.isGrounded && (rand() % 10) < hard + 1 - g_cpuTilt / 2) {
        int dodgeDir = (hard >= 3 && rand() % 2 == 0) ? g_other.dir : -g_other.dir;
        StartDodge(g_other, dodgeDir);
        return;
    }

    if (dist < 85.0f && (g_me.state == ACT_LIGHT1 || g_me.state == ACT_SKILL_ATTACK)) {
        int parryRate = ClampI(boss.aiParryRate + hard - g_cpuTilt, 0, 9);
        int guardRate = ClampI(7 + hard / 2 - g_cpuTilt, 1, 9);
        if ((rand() % 10) < parryRate) {
            g_other.state = ACT_PARRY;
            g_other.stateTimer = 18;
            g_other.animTimer = 0;
            g_other.parryWindow = 5;
            return;
        }
        else if (rand() % 10 < guardRate) {
            g_other.state = ACT_GUARD;
            g_other.stateTimer = 20;
            return;
        }
    }

    if (stage == 1) {
        g_other.vx = (float)-g_other.dir * speed;
        if (dist > 150.0f && rand() % CpuFreq(25) == 0) {
            CpuStartState(ACT_MID_ATTACK, 28);
            SpawnShot(false, g_other.x + g_other.dir * 30, g_other.y - 18, g_other.dir * 12.0f, 0, 45, CYAN, 10, 0);
            return;
        }
        if (dist > 100.0f && rand() % CpuFreq(80) == 0) {
            StartAttack(g_other, g_me, 2);
            return;
        }
    }
    else if (stage == 0) {
        g_other.vx = (float)g_other.dir * speed;
        if (dist < 75.0f) {
            CpuStartState(ACT_LIGHT1, 18);
            return;
        }
    }
    else if (stage == 2) {
        g_other.vx = (float)g_other.dir * speed;
        if (dist < 90.0f && rand() % CpuFreq(30) == 0) {
            CpuStartState(ACT_SKILL_ATTACK, 34);
            g_other.armorFrames = 20;
            g_other.vx = (float)g_other.dir * 9.5f;
            return;
        }
    }
    else if (stage == 3) {
        // 物干し竿の間合い管理
        g_other.vx = (float)g_other.dir * speed;
        if (dist < 300.0f && dist > 110.0f && rand() % CpuFreq(40) == 0) {
            StartAttack(g_other, g_me, 1);
            return;
        }
        if (dist < 180.0f && rand() % CpuFreq(70) == 0) {
            StartAttack(g_other, g_me, 2);
            return;
        }
    }
    else if (stage == 4 || stage == 5) {
        if ((float)g_other.hp / (float)g_other.maxHp < 0.30f && rand() % CpuFreq(60) == 0) {
            CpuStartState(ACT_SUPER_ATTACK, 50);
            TriggerScreenShake(15);
            for (int an = 0; an < 8; an++) {
                float ang = (float)an * 3.14159f / 4.0f;
                SpawnShot(false, g_other.x, g_other.y - 20, cosf(ang) * 9.5f, sinf(ang) * 9.5f, 40, YELLOW, 14, 2);
            }
            return;
        }
        g_other.vx = (float)g_other.dir * speed;
    }

    float closeRange = (g_other.charaIndex == 4) ? 110.0f : 85.0f;
    if (dist <= closeRange) {
        if (g_other.isGrounded) {
            // イライラしているほど大技を振りがち
            int r = rand() % 100;
            int lightTh = 45 - g_cpuTilt * 6;
            int skillTh = 75 + g_cpuTilt * 4;
            if (r < lightTh) CpuStartState(ACT_LIGHT1, 18);
            else if (r < skillTh) CpuStartState(ACT_SKILL_ATTACK, 34);
        }
    }
    else {
        g_other.vx = (float)g_other.dir * speed;
    }
}

static void UpdateCpu() {
    if (!UpdateFighterCommon(g_other, g_me, false)) return;

    int stage = ClampI(g_slots[g_currentSlot].stageProgress, 0, LAST_STAGE);
    const CpuBossData& boss = CPU_ROSTER[stage];
    int hard = GetHardLevel();

    if (g_cpuTilt > 0 && --g_cpuTiltTimer <= 0) {
        g_cpuTilt--;
        g_cpuTiltTimer = 300;
    }

    if (g_readyFightTimer < 80) return;

    UpdateStateSpecials(g_other, g_me);

    // CPUのコンボ抜け（周回が進むほど早く抜ける）
    if (IsHitState(g_other.state) && g_other.comboTaken >= ((5 - hard < 2) ? 2 : 5 - hard) &&
        g_other.spGauge >= 50 && rand() % 100 < 6 + hard * 6) {
        TryBurst(g_other, g_me);
    }

    if (g_other.state == ACT_IDLE) {
        CpuThink(stage, boss);
    }

    CheckAttackHit(g_other, g_me);
}

static void UpdateShotsAndFX() {
    if (g_superCutinTimer > 0) g_superCutinTimer--;
    if (g_kokusenStrobe > 0) g_kokusenStrobe--;
    if (g_flashTimer > 0) g_flashTimer--;
    g_rainbowCycle = (g_rainbowCycle + 8) % 360;

    if (g_screenShakeTimer > 0) {
        g_screenShakeTimer--;
        g_shakeOffsetX = (rand() % 9) - 4;
        g_shakeOffsetY = (rand() % 9) - 4;
    }
    else {
        g_shakeOffsetX = 0;
        g_shakeOffsetY = 0;
    }

    for (int i = 0; i < MAX_FX; i++) {
        if (!g_fx[i].active) continue;
        g_fx[i].x += g_fx[i].vx;
        g_fx[i].y += g_fx[i].vy;
        g_fx[i].life--;
        if (g_fx[i].life <= 0) g_fx[i].active = false;
    }

    for (int i = 0; i < MAX_SPARKS; i++) {
        if (!g_sparks[i].active) continue;
        g_sparks[i].life--;
        g_sparks[i].length *= 0.88f;
        if (g_sparks[i].life <= 0) g_sparks[i].active = false;
    }

    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!g_popups[i].active) continue;
        g_popups[i].y -= 0.6f;
        if (--g_popups[i].life <= 0) g_popups[i].active = false;
    }

    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!g_shots[i].active) continue;
        g_shots[i].x += g_shots[i].vx;
        g_shots[i].y += g_shots[i].vy;
        g_shots[i].life--;
        g_shots[i].angle += 0.35f;

        // 罠は放物線を描いて落ち、着地点に設置される
        if (g_shots[i].projType == 4) {
            g_shots[i].vy += 0.45f;
            if (g_shots[i].y >= GROUND_Y - 4.0f) {
                PlaceTrap(g_shots[i].isFromMe, g_shots[i].subType, g_shots[i].x);
                g_shots[i].active = false;
                continue;
            }
        }

        Fighter& def = g_shots[i].isFromMe ? g_other : g_me;
        Fighter& atk = g_shots[i].isFromMe ? g_me : g_other;

        float defHitW = 30.0f;
        float defCenterX = def.x;
        if (def.charaIndex == 2) {
            if (def.state == ACT_LIGHT1) {
                defHitW = 130.0f;
                defCenterX = def.x + (float)def.dir * 65.0f;
            }
            else if (def.state == ACT_MID_ATTACK) {
                defHitW = 160.0f;
                defCenterX = def.x + (float)def.dir * 80.0f;
            }
        }

        if (g_shots[i].damage > 0 && def.invulFrames == 0 &&
            fabsf(g_shots[i].x - defCenterX) < (defHitW / 2.0f + 15.0f) && fabsf(g_shots[i].y - (def.y - 15)) < 35.0f) {
            bool reflect = (def.state == ACT_PARRY && def.parryWindow > 0) ||
                (def.state == ACT_COUNTER_STANCE && def.stanceLevel >= 2);
            if (reflect) {
                g_shots[i].vx *= -1.3f;
                g_shots[i].isFromMe = !g_shots[i].isFromMe;
                def.spGauge += 20; if (def.spGauge > 100) def.spGauge = 100;
                SpawnHitFX(def.x, def.y - 15, CYAN, 20);
                SpawnSparks(def.x, def.y - 15, CYAN, 10, 35.0f);
                if (def.state == ACT_COUNTER_STANCE) SpawnPopup(def.x, def.y - 90, "見切り!", CYAN);
                continue;
            }
            ApplyDamage(def, atk, g_shots[i].damage, 22, (g_shots[i].vx > 0 ? 1.0f : -1.0f) * 5.0f, 0, false, false);
            g_shots[i].active = false;
            continue;
        }

        if (g_shots[i].life <= 0 || g_shots[i].x < 10 || g_shots[i].x > 630) {
            g_shots[i].active = false;
        }
    }
}

// ----------------------------------------------------
// 描画
// ----------------------------------------------------
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
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;
    for (int i = 0; i < MAX_TRAPS; i++) {
        const Trap& t = g_traps[i];
        if (!t.active) continue;
        if ((t.armDelay > 0 || t.life < 120) && (g_frameCount / 4) % 2 == 0) continue;
        DrawTrapIcon(t.type, (int)t.x + ox, (int)GROUND_Y + 24 + oy);
    }
}

static void DrawGimmick(int stageIdx) {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;

    DrawTraps();

    if (stageIdx == 1 && g_gimmick.hazardActive) {
        DrawBox(0, (int)GROUND_Y + 22 + oy, 640, (int)GROUND_Y + 26 + oy, CYAN, TRUE);
        for (int e = 0; e < 10; e++) {
            int rx = rand() % 640;
            DrawLine(rx, (int)GROUND_Y + 20, rx + (rand() % 16 - 8), (int)GROUND_Y + 12, YELLOW);
        }
    }

    if (g_gimmick.itemActive) {
        int ix = (int)g_gimmick.itemX + ox;
        int iy = (int)g_gimmick.itemY - 12 + oy;
        DrawTriangle(ix, iy - 10, ix - 12, iy + 10, ix + 12, iy + 10, WHITE, TRUE);
        DrawBox(ix - 5, iy + 2, ix + 5, iy + 10, GetColor(20, 20, 20), TRUE);
        DrawString(ix - 15, iy - 25, "HEAL", GREEN);
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
        DrawLine((int)g_gimmick.x + ox, (int)GROUND_Y + 25 + oy, (int)tipX + ox, (int)tipY + oy, YELLOW);
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

// 大前の物干し竿（ang: 0で正面水平、マイナスで上向き）
static void DrawPole(int sx, int sy, int dir, float ang, int len, bool glow) {
    int tx = sx + (int)((float)dir * cosf(ang) * (float)len);
    int ty = sy + (int)(sinf(ang) * (float)len);
    DrawLine(sx, sy, tx, ty, GetColor(120, 125, 140), 9);
    DrawLine(sx, sy, tx, ty, glow ? YELLOW : GetColor(215, 220, 230), 3);
    DrawCircle(tx, ty, 7, GetColor(190, 195, 205), TRUE);
}

static void DrawMegatonPole(const Fighter& f, int bx, int by) {
    int sx = bx, sy = by - 12;
    FrameData fd = GetFrameData(4, f.state);
    float t = (fd.startup > 0) ? (float)f.animTimer / (float)fd.startup : 1.0f;
    if (t > 1.0f) t = 1.0f;
    bool active = (f.animTimer >= fd.startup && f.animTimer < fd.startup + fd.active);

    if (f.state == ACT_MID_ATTACK) {
        if (f.animTimer < fd.startup) DrawPole(sx, sy, f.dir, -1.9f + 1.75f * t, 300, false);
        else if (active) {
            DrawPole(sx, sy, f.dir, 0.0f, 300, true);
            for (int k = 1; k <= 3; k++) DrawLine(sx, sy - 12 * k, sx + f.dir * 300, sy - 4 * k, ORANGE);
        }
        else DrawPole(sx, sy + 10, f.dir, 0.25f, 280, false);
    }
    else if (f.state == ACT_SKILL_ATTACK) {
        if (f.animTimer < fd.startup) DrawPole(sx, sy, f.dir, -1.57f - 0.35f * t, 190, false);
        else if (active) DrawPole(sx, sy, f.dir, 0.55f, 190, true);
        else {
            DrawPole(sx, sy, f.dir, 0.7f, 180, false);
            if (f.animTimer % 4 == 0) SpawnHitFX(f.x + f.dir * 140, GROUND_Y + 15, GRAY, 1);
        }
    }
    else if (f.state == ACT_SUPER_ATTACK) {
        DrawPole(sx, sy, f.dir, (float)f.animTimer * 0.5f, 320, active);
        if (active) DrawCircle(sx, sy, 320, ORANGE, FALSE, 3);
    }
    else if (f.state == ACT_LIGHT1 || f.state == ACT_LIGHT2) {
        DrawPole(sx, sy, f.dir, -0.05f, 95, active);
    }
    else {
        DrawPole(bx - f.dir * 8, by + 12, f.dir, -1.25f, 130, false);
    }
}

// 吉田の蹴り（描いたら true）
static bool DrawYoshidaKick(const Fighter& f, int bx, int by, unsigned int col) {
    int hipY = by + 8;
    switch (f.state) {
    case ACT_LIGHT1:
        DrawLine(bx, hipY, bx + f.dir * 68, by + 16, col, 6);
        DrawCircle(bx + f.dir * 68, by + 16, 6, YELLOW, TRUE);
        return true;
    case ACT_LIGHT2:
        DrawLine(bx, hipY, bx + f.dir * 72, by - 22, col, 6);
        DrawCircle(bx + f.dir * 72, by - 22, 7, YELLOW, TRUE);
        return true;
    case ACT_MID_ATTACK:
        DrawLine(bx, hipY - 10, bx + f.dir * 85, by - 10, col, 8);
        DrawCircle(bx + f.dir * 85, by - 10, 8, YELLOW, TRUE);
        for (int k = 0; k < 3; k++) DrawLine(bx - f.dir * (10 + k * 10), by - 20 + k * 10, bx - f.dir * (40 + k * 10), by - 20 + k * 10, WHITE);
        return true;
    case ACT_SKILL_ATTACK:
        DrawLine(bx, hipY, bx + f.dir * 25, by - 65, col, 7);
        DrawCircle(bx, by - 20, 45, YELLOW, FALSE, 2);
        return true;
    case ACT_AIR_LIGHT:
        DrawLine(bx, hipY, bx + f.dir * 30, by + 48, col, 7);
        DrawCircle(bx + f.dir * 30, by + 48, 8, ORANGE, TRUE);
        return true;
    case ACT_AIR_KICK:
        DrawCircle(bx, by, 45, GREEN, FALSE, 3);
        DrawLine(bx, hipY, bx + f.dir * 70, by + 4, col, 6);
        return true;
    case ACT_SUPER_ATTACK:
        for (int k = 0; k < 4; k++) {
            int ky = by - 30 + (rand() % 60);
            DrawLine(bx, hipY, bx + f.dir * (55 + rand() % 30), ky, (k % 2) ? YELLOW : WHITE, 4);
        }
        return true;
    default:
        return false;
    }
}

static void DrawFighterBody(const Fighter& f, bool isYou) {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;
    int c = f.charaIndex;

    unsigned int col = CHARA_PROFILES[c].baseColor;
    if (f.state == ACT_PARRY) col = CYAN;
    if (f.state == ACT_GUARD) col = BLUE;
    if (f.state == ACT_STUN) col = (rand() % 2 == 0) ? YELLOW : GRAY;
    if (f.state == ACT_HIT || f.state == ACT_AIR_HIT) col = WHITE;
    if (f.state == ACT_SUPER_ENHANCED || f.state == ACT_SUPER_AIR) col = (rand() % 2 == 0) ? RED : YELLOW;
    if (f.state == ACT_BOAR_CHARGE) col = (rand() % 2 == 0) ? ORANGE : YELLOW;
    if (f.state == ACT_COUNTER_STRIKE) col = WHITE;

    int bx = (int)f.x + ox;
    int by = (int)f.y - 25 + oy;

    int bw = (c == 4) ? 24 : 14;
    int bh = (c == 4) ? 28 : 22;

    int markerBaseY = by - bh - 24;
    if (isYou) {
        DrawString(bx - 10, markerBaseY - 14, "YOU", YELLOW);
        DrawTriangle(bx - 6, markerBaseY - 2, bx + 6, markerBaseY - 2, bx, markerBaseY + 6, YELLOW, TRUE);
        DrawTriangle(bx - 6, markerBaseY - 2, bx + 6, markerBaseY - 2, bx, markerBaseY + 6, WHITE, FALSE);
    }
    else {
        const char* targetLabel = g_is_net_battle ? "2P" : "COM";
        unsigned int markerCol = g_is_net_battle ? BLUE : RED;
        DrawString(bx - (g_is_net_battle ? 8 : 12), markerBaseY - 14, targetLabel, markerCol);
        DrawTriangle(bx - 6, markerBaseY - 2, bx + 6, markerBaseY - 2, bx, markerBaseY + 6, markerCol, TRUE);
        DrawTriangle(bx - 6, markerBaseY - 2, bx + 6, markerBaseY - 2, bx, markerBaseY + 6, WHITE, FALSE);
    }

    // いのししのサイコロ
    if (f.state == ACT_BOAR_ROLL || f.diceShowTimer > 0) {
        int dx = bx, dy = by - bh - 72;
        DrawBox(dx - 12, dy - 12, dx + 12, dy + 12, WHITE, TRUE);
        DrawBox(dx - 12, dy - 12, dx + 12, dy + 12, (f.diceFace == 1) ? RED : BLACK, FALSE);
        DrawFormatString(dx - 4, dy - 8, (f.diceFace == 1) ? RED : BLACK, "%d", f.diceFace);
    }

    // 無敵中は半透明
    if (f.invulFrames > 0 && (f.state == ACT_DODGE || (g_frameCount / 2) % 2 == 0)) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 110);
    }

    // 転倒中は寝そべり
    if (f.state == ACT_SLIP) {
        int ly = by + 16;
        DrawBox(bx - 26, ly - 8, bx + 20, ly + 8, col, TRUE);
        DrawBox(bx - 26, ly - 8, bx + 20, ly + 8, WHITE, FALSE);
        DrawCircle(bx - f.dir * 32, ly, 10, col, TRUE);
        DrawString(bx - 16, by - 26, "@_@", YELLOW);
        return;
    }

    // ダッシュの残像
    if (f.state == ACT_DASH) {
        for (int k = 1; k <= 2; k++) {
            int ax = bx - f.moveDir * 14 * k;
            DrawBox(ax - bw, by - bh, ax + bw, by + bh, GRAY, FALSE);
        }
    }

    DrawBox(bx - bw, by - bh, bx + bw, by + bh, col, TRUE);
    DrawBox(bx - bw, by - bh, bx + bw, by + bh, WHITE, FALSE);
    DrawCircle(bx, by - bh - 10, (c == 4) ? 14 : 11, col, TRUE);
    DrawCircle(bx, by - bh - 10, (c == 4) ? 14 : 11, WHITE, FALSE);

    int hx = bx, hy = by - bh - 10;

    if (c == 5) {
        int chairX = bx + f.dir * 18;
        int chairY = by - 5;
        DrawLine(chairX - 8, chairY - 18, chairX + 8, chairY + 18, GetColor(200, 205, 220));
        DrawLine(chairX + 8, chairY - 18, chairX - 8, chairY + 18, GetColor(200, 205, 220));
        DrawBox(chairX - 10, chairY - 16, chairX + 10, chairY - 6, BLUE, TRUE);
    }

    if (c == 1) {
        // 工事用コーンの帽子
        DrawTriangle(hx, hy - 30, hx - 10, hy - 8, hx + 10, hy - 8, ORANGE, TRUE);
        DrawLine(hx - 5, hy - 18, hx + 5, hy - 18, WHITE, 2);
    }

    if (c == 3) {
        // 猪の鼻と牙と耳
        DrawCircle(hx + f.dir * 10, hy + 2, 5, GetColor(230, 150, 160), TRUE);
        DrawLine(hx + f.dir * 6, hy + 5, hx + f.dir * 16, hy - 3, WHITE, 2);
        DrawLine(hx + f.dir * 2, hy + 6, hx + f.dir * 12, hy - 2, WHITE, 2);
        DrawTriangle(hx - 9, hy - 6, hx - 4, hy - 16, hx - 1, hy - 8, col, TRUE);
        DrawTriangle(hx + 9, hy - 6, hx + 4, hy - 16, hx + 1, hy - 8, col, TRUE);
    }

    if (c == 4) {
        DrawBox(bx - bw, by + 5, bx + bw, by + 18, GetColor(50, 40, 20), TRUE);
        DrawMegatonPole(f, bx, by);
    }

    if (c == 7) {
        // ハチマキ
        DrawLine(hx - 11, hy - 4, hx + 11, hy - 4, RED, 3);
        DrawLine(hx - f.dir * 11, hy - 4, hx - f.dir * 22, hy + 2, RED, 2);
    }

    if (c == 8 && f.state == ACT_IDLE && (g_frameCount / 40) % 3 == 0) {
        DrawString(hx + 14, hy - 22, "ﾆﾔﾆﾔ", CYAN);
    }

    if (c == 2) {
        if (f.state == ACT_LIGHT1) {
            int startX = bx + f.dir * bw;
            int startY = by - 10;
            int endX = bx + f.dir * 135;
            int endY = by - 10;
            for (int seg = 0; seg <= 6; seg++) {
                float t = (float)seg / 6.0f;
                int segX = (int)(startX + (endX - startX) * t);
                int segY = startY + ((seg % 2 == 1) ? -4 : 4);
                DrawCircle(segX, segY, 6, CYAN, TRUE);
                DrawCircle(segX, segY, 6, WHITE, FALSE);
            }
            DrawCircle(endX, endY, 12, YELLOW, TRUE);
            DrawCircle(endX, endY, 12, WHITE, FALSE);
            return;
        }
        else if (f.state == ACT_MID_ATTACK) {
            int startX = bx + f.dir * bw;
            int startY = by + 10;
            int endX = bx + f.dir * 170;
            int endY = by + 2;
            for (int seg = 0; seg <= 8; seg++) {
                float t = (float)seg / 8.0f;
                int segX = (int)(startX + (endX - startX) * t);
                int segY = startY + ((seg % 2 == 1) ? -5 : 5);
                DrawCircle(segX, segY, 7, CYAN, TRUE);
                DrawCircle(segX, segY, 7, WHITE, FALSE);
            }
            DrawBox(endX - 8, endY - 10, endX + 16, endY + 10, YELLOW, TRUE);
            DrawBox(endX - 8, endY - 10, endX + 16, endY + 10, WHITE, FALSE);
            return;
        }
        else if (f.state == ACT_AIR_KICK || f.state == ACT_AIR_LIGHT) {
            int startX = bx + f.dir * bw;
            int startY = by + 10;
            int endX = bx + f.dir * 70;
            int endY = by + 75;
            for (int seg = 0; seg <= 5; seg++) {
                float t = (float)seg / 5.0f;
                int segX = (int)(startX + (endX - startX) * t);
                int segY = (int)(startY + (endY - startY) * t);
                DrawCircle(segX, segY, 6, CYAN, TRUE);
            }
            DrawCircle(endX, endY, 12, YELLOW, TRUE);
            DrawCircle(endX, endY, 12, WHITE, FALSE);
            return;
        }
    }

    // キャラ固有の攻撃グラフィック
    bool customDrawn = false;
    if (c == 4) {
        customDrawn = (f.state == ACT_LIGHT1 || f.state == ACT_LIGHT2 || f.state == ACT_MID_ATTACK ||
            f.state == ACT_SKILL_ATTACK || f.state == ACT_SUPER_ATTACK);
    }
    else if (c == 7) {
        customDrawn = DrawYoshidaKick(f, bx, by, col);
    }
    else if (c == 1 && (f.state == ACT_LIGHT1 || f.state == ACT_LIGHT2 || f.state == ACT_AIR_LIGHT)) {
        DrawLine(bx + f.dir * bw, by - 8, bx + f.dir * (bw + 12), by - 8, YELLOW, 3);
        customDrawn = true;
    }
    else if (c == 1 && (f.state == ACT_MID_ATTACK || f.state == ACT_SKILL_ATTACK || f.state == ACT_AIR_KICK || f.state == ACT_SUPER_ATTACK)) {
        DrawLine(bx, by - 10, bx + f.dir * 20, by - 30, YELLOW, 3);
        customDrawn = true;
    }
    else if (c == 3 && f.state == ACT_BOAR_CHARGE) {
        for (int k = 0; k < 5; k++) {
            int ly = by - 20 + k * 10;
            DrawLine(bx - f.dir * 20, ly, bx - f.dir * (50 + rand() % 30), ly, WHITE);
        }
        DrawString(bx - 30, by - bh - 50, "ﾌﾞﾋｨｨｨ!!", ORANGE);
        customDrawn = true;
    }
    else if (c == 8) {
        if (f.state == ACT_COUNTER_STANCE) {
            int r = 30 + (int)(sinf((float)f.animTimer * 0.4f) * 5.0f);
            unsigned int ac = (f.stanceLevel >= 2) ? YELLOW : CYAN;
            DrawCircle(bx, by, r, ac, FALSE, 3);
            DrawCircle(bx, by, r + 8, ac, FALSE, 1);
            DrawString(bx - 16, by - bh - 50, (f.stanceLevel >= 2) ? "極・当身" : "当身", ac);
            customDrawn = true;
        }
        else if (f.state == ACT_COUNTER_STRIKE) {
            for (int k = 0; k < 3; k++) DrawCircle(bx + f.dir * 30, by - 10, 30 + k * 18 + f.animTimer, (k % 2) ? CYAN : WHITE, FALSE, 3);
            DrawLine(bx, by - 10, bx + f.dir * 60, by - 10, WHITE, 8);
            customDrawn = true;
        }
        else if (f.state == ACT_TAUNT) {
            int bounce = (f.animTimer / 6) % 2 ? -4 : 0;
            DrawString(bx - 40, by - bh - 50 + bounce, "ほらほら？", CYAN);
            DrawLine(bx + f.dir * bw, by - 10, bx + f.dir * (bw + 12), by - 22 + bounce, col, 3);
            customDrawn = true;
        }
        else if (f.state == ACT_LIGHT1 || f.state == ACT_LIGHT2 || f.state == ACT_AIR_LIGHT) {
            DrawLine(bx + f.dir * bw, by - 6, bx + f.dir * (bw + 14), by - 6, YELLOW, 3);
            customDrawn = true;
        }
    }

    if (!customDrawn) {
        if (f.state == ACT_LIGHT1 || f.state == ACT_AIR_LIGHT) {
            DrawBox(bx + f.dir * bw, by - 14, bx + f.dir * (bw + 28), by + 4, YELLOW, TRUE);
            DrawLine(bx + f.dir * bw, by - 20, bx + f.dir * (bw + 32), by + 10, WHITE);
        }
        else if (f.state == ACT_SKILL_ATTACK) {
            DrawBox(bx + f.dir * 10, by - 48, bx + f.dir * 42, by + 15, RED, TRUE);
            DrawCircle(bx + f.dir * 28, by - 48, 18, YELLOW, TRUE);
            for (int arc = -2; arc <= 2; arc++) {
                DrawLine(bx + f.dir * 15, by + 10 + arc, bx + f.dir * 45, by - 60 + arc, ORANGE);
            }
        }
        else if (f.state == ACT_AIR_KICK || f.state == ACT_SUPER_AIR) {
            DrawBox(bx + f.dir * 10, by + 6, bx + f.dir * 45, by + 40, RED, TRUE);
            for (int arc = -2; arc <= 2; arc++) {
                DrawLine(bx + f.dir * 10, by - 20 + arc, bx + f.dir * 50, by + 45 + arc, YELLOW);
            }
            if (f.state == ACT_SUPER_AIR) {
                DrawCircle(bx + f.dir * 25, by + 20, 30, ORANGE, FALSE);
                DrawCircle(bx + f.dir * 25, by + 20, 20, YELLOW, TRUE);
            }
        }
        else if (f.state == ACT_SUPER_ENHANCED) {
            DrawCircle(bx, by - 25, 55, RED, TRUE);
            DrawCircle(bx, by - 25, 40, ORANGE, TRUE);
            DrawCircle(bx, by - 25, 25, YELLOW, TRUE);
        }
    }

    if (f.state == ACT_PARRY) {
        DrawCircle(bx + f.dir * 18, by, 24, CYAN, FALSE);
        DrawCircle(bx + f.dir * 18, by, 18, WHITE, FALSE);
        if (c == 8) DrawCircle(bx + f.dir * 18, by, 32, YELLOW, FALSE, 2);
    }
    else if (f.state == ACT_GUARD) {
        DrawBox(bx + f.dir * 12, by - 20, bx + f.dir * 18, by + 20, BLUE, TRUE);
    }

    if (f.state == ACT_STUN) {
        DrawString(bx - 20, by - 65, "*PIYO*", YELLOW);
    }
}

static void DrawFighterObj(const Fighter& f, bool isYou) {
    DrawFighterBody(f, isYou);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

static void DrawHud(bool isNet = false) {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;

    int stage = isNet ? g_selectedNetStage : g_slots[g_currentSlot].stageProgress;
    stage = ClampI(stage, 0, LAST_STAGE);

    // 1P: 大型HPゲージ
    DrawBox(30, 20, 280, 44, GetColor(50, 15, 15), TRUE);
    float myHpR = (float)g_me.hp / (float)g_me.maxHp;
    if (myHpR < 0) myHpR = 0;
    unsigned int hpCol1 = (myHpR < 0.25f) ? RED : ((myHpR < 0.5f) ? YELLOW : GREEN);
    DrawBox(30, 20, 30 + (int)(250.0f * myHpR), 44, hpCol1, TRUE);
    DrawBox(30, 20, 280, 44, WHITE, FALSE);
    if (isNet) {
        DrawFormatString(32, 5, WHITE, "1P: YOU [%s]", CHARA_PROFILES[g_me.charaIndex].charaId);
    }
    else {
        DrawFormatString(32, 5, WHITE, "DATA %d: %s", g_currentSlot + 1, CHARA_PROFILES[g_me.charaIndex].charaId);
    }

    // 1P: 細めのスタミナゲージ
    DrawBox(30, 48, 280, 56, GetColor(20, 35, 20), TRUE);
    float myStR = g_me.stamina / g_me.maxStamina;
    if (myStR < 0) myStR = 0;
    DrawBox(30, 48, 30 + (int)(250.0f * myStR), 56, CYAN, TRUE);
    DrawBox(30, 48, 280, 56, WHITE, FALSE);
    DrawString(285, 46, "STM", CYAN);

    // 2P: 大型HPゲージ
    DrawBox(360, 20, 610, 44, GetColor(50, 15, 15), TRUE);
    float otherHpR = (float)g_other.hp / (float)g_other.maxHp;
    if (otherHpR < 0) otherHpR = 0;
    unsigned int hpCol2 = (otherHpR < 0.25f) ? RED : ((otherHpR < 0.5f) ? YELLOW : GREEN);
    DrawBox(610 - (int)(250.0f * otherHpR), 20, 610, 44, hpCol2, TRUE);
    DrawBox(360, 20, 610, 44, WHITE, FALSE);
    if (isNet) {
        DrawFormatString(450, 5, WHITE, "2P: REMOTE [%s]", CHARA_PROFILES[g_other.charaIndex].charaId);
    }
    else {
        DrawFormatString(430, 5, WHITE, "%s - %s", CPU_ROSTER[stage].name, CPU_ROSTER[stage].alias);
    }

    // 2P: 細めのスタミナゲージ
    DrawBox(360, 48, 610, 56, GetColor(20, 35, 20), TRUE);
    float othStR = g_other.stamina / g_other.maxStamina;
    if (othStR < 0) othStR = 0;
    DrawBox(610 - (int)(250.0f * othStR), 48, 610, 56, CYAN, TRUE);
    DrawBox(360, 48, 610, 56, WHITE, FALSE);
    DrawString(330, 46, "STM", CYAN);

    // タイマー
    DrawBox(292, 16, 348, 52, GetColor(20, 20, 30), TRUE);
    DrawBox(292, 16, 348, 52, (g_roundTime <= 10) ? RED : YELLOW, FALSE);
    DrawFormatString(304, 24, (g_roundTime <= 10) ? RED : YELLOW, "%02d", g_roundTime);

    // 周回・難易度
    if (!isNet) {
        int hard = GetHardLevel();
        DrawFormatString(288, 60, hard > 0 ? ORANGE : GRAY, "%d周目", GetCurrentLap());
        if (hard > 0) DrawFormatString(286, 76, RED, "HARD+%d", hard);
        if (g_cpuTilt > 0) {
            char bars[16] = "";
            for (int i = 0; i < g_cpuTilt && i < 6; i++) strcat_s(bars, "■");
            DrawFormatString(400, 62, RED, "COMイライラ %s", bars);
        }
    }

    // SP GAUGE
    DrawBox(30, 420, 230, 436, GetColor(15, 20, 35), TRUE);
    int spW1 = (int)(200.0f * ((float)g_me.spGauge / 100.0f));
    unsigned int spCol1 = (g_me.spGauge >= 100) ? ((rand() % 2 == 0) ? YELLOW : ORANGE) : CYAN;
    DrawBox(30, 420, 30 + spW1, 436, spCol1, TRUE);
    DrawBox(30, 420, 230, 436, WHITE, FALSE);
    DrawLine(130, 420, 130, 436, GRAY);
    if (g_me.spGauge >= 100) {
        DrawString(235, 421, "SP READY! [J+K]", YELLOW);
    }
    else {
        DrawFormatString(235, 421, GRAY, "SP %d%%", g_me.spGauge);
    }
    if (g_me.spGauge >= 50 && g_me.burstCooldown == 0) {
        DrawString(30, 402, "BURST OK", CYAN);
    }

    // コンボ通知
    if (g_me.comboCount > 1 && g_me.comboDisplayTimer > 0) {
        DrawBox(35, 95, 250, 138, GetColor(0, 0, 0), TRUE);
        DrawBox(35, 95, 250, 138, YELLOW, FALSE);
        DrawFormatString(45, 100, YELLOW, "%d HITS COMBO !!", g_me.comboCount);
        if (g_me.fromAirLightCombo) {
            DrawString(45, 118, "ULTIMATE CANCEL!!", RED);
        }
        else if (g_other.wallBounceUsed) {
            DrawString(45, 118, "WALL COMBO!", ORANGE);
        }
        else {
            DrawString(45, 118, "SP GAUGE BOOST!", CYAN);
        }
    }

    if (g_parryNoticeTimer > 0) {
        g_parryNoticeTimer--;
        DrawString(260, 90, "JUST PARRY!!", CYAN);
    }

    // ==========================================
    // パチンコ風・超激熱カットイン帯演出
    // ==========================================
    if (g_superCutinTimer > 0) {
        if (g_cutinIsCounter) {
            DrawBox(0, 185, 640, 275, GetColor(0, 15, 25), TRUE);
            DrawBox(0, 185, 640, 275, CYAN, FALSE);
            DrawBox(3, 188, 637, 272, WHITE, FALSE);
            DrawString(230, 200, "!!! C O U N T E R !!!", (rand() % 2 == 0) ? CYAN : WHITE);
            DrawString(150, 235, g_superCutinName, YELLOW);
        }
        else if (g_superIsUltimate) {
            // パチンコ大当たり・レインボー枠
            DrawBox(0, 180, 640, 280, GetColor(20, 0, 0), TRUE);
            for (int b = 0; b < 6; b++) {
                int rcol = GetColor((rand() % 155) + 100, (rand() % 155) + 100, (rand() % 155) + 100);
                DrawBox(0 + b, 180 + b, 640 - b, 280 - b, rcol, FALSE);
            }
            DrawString(180, 195, "★★★ 確 変 突 入 !! 超 激 熱 ★★★", YELLOW);
            DrawString(130, 225, g_superCutinName, (rand() % 2 == 0 ? WHITE : YELLOW));
            DrawString(210, 252, ">> PUSH [J+K] SUCCESS!! <<", CYAN);
        }
        else {
            DrawBox(0, 190, 640, 270, GetColor(0, 0, 0), TRUE);
            DrawBox(0, 190, 640, 270, RED, FALSE);
            DrawString(170, 205, "=== SUPER COMBO FINISH ACTIVATION ===", YELLOW);
            DrawString(190, 230, g_superCutinName, (g_me.hp < g_me.maxHp / 4 ? RED : YELLOW));
        }
    }

    if (g_readyFightTimer < 50) {
        DrawBox(220, 195, 420, 245, GetColor(0, 0, 0), TRUE);
        DrawBox(220, 195, 420, 245, YELLOW, FALSE);
        DrawString(285, 212, "READY ?", YELLOW);
    }
    else if (g_readyFightTimer < 80) {
        DrawBox(200, 190, 440, 250, GetColor(0, 0, 0), TRUE);
        DrawBox(200, 190, 440, 250, RED, FALSE);
        DrawString(280, 210, "F I G H T !!", RED);
    }

    DrawString(25, 444, "[A/D]移動 [AA/DD]ダッシュ [W]跳 [空中W]2段 [空中SS]急降下 [I/Shift]回避", WHITE);
    DrawString(25, 462, "[J/K/L]攻撃 [前+J]投げ [J+K]超必 [SPACE]パリィ(被弾中:バースト)", WHITE);

    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!g_shots[i].active) continue;
        int sx = (int)g_shots[i].x + ox;
        int sy = (int)g_shots[i].y + oy;

        if (g_shots[i].projType == 1) {
            DrawCircle(sx, sy, g_shots[i].radius, YELLOW, TRUE);
            DrawCircle(sx, sy, g_shots[i].radius - 3, ORANGE, TRUE);
            DrawCircle(sx, sy, g_shots[i].radius, WHITE, FALSE);
        }
        else if (g_shots[i].projType == 2) {
            DrawCircle(sx, sy, g_shots[i].radius + 3, CYAN, TRUE);
            DrawCircle(sx, sy, g_shots[i].radius - 2, WHITE, TRUE);
        }
        else if (g_shots[i].projType == 4) {
            DrawTrapIcon(g_shots[i].subType % 3, sx, sy + 8);
        }
        else {
            DrawCircle(sx, sy, g_shots[i].radius, g_shots[i].color, TRUE);
            DrawCircle(sx, sy, g_shots[i].radius, WHITE, FALSE);
        }
    }

    for (int i = 0; i < MAX_FX; i++) {
        if (g_fx[i].active) DrawCircle((int)g_fx[i].x + ox, (int)g_fx[i].y + oy, 3, g_fx[i].color, TRUE);
    }

    for (int i = 0; i < MAX_SPARKS; i++) {
        if (g_sparks[i].active) {
            float sx1 = g_sparks[i].x + ox - cosf(g_sparks[i].angle) * g_sparks[i].length;
            float sy1 = g_sparks[i].y + oy - sinf(g_sparks[i].angle) * g_sparks[i].length;
            float sx2 = g_sparks[i].x + ox + cosf(g_sparks[i].angle) * g_sparks[i].length;
            float sy2 = g_sparks[i].y + oy + sinf(g_sparks[i].angle) * g_sparks[i].length;
            DrawLine((int)sx1, (int)sy1, (int)sx2, (int)sy2, g_sparks[i].color);
        }
    }

    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!g_popups[i].active) continue;
        int w = GetDrawStringWidth(g_popups[i].text, (int)strlen(g_popups[i].text));
        int px = (int)g_popups[i].x + ox - w / 2;
        if (px < 4) px = 4;
        if (px + w > 636) px = 636 - w;
        int py = (int)g_popups[i].y + oy;
        DrawString(px + 1, py + 1, g_popups[i].text, BLACK);
        DrawString(px, py, g_popups[i].text, g_popups[i].color);
    }

    if (g_flashTimer > 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, g_flashTimer * 8);
        DrawBox(0, 0, 640, 480, WHITE, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

static void DrawPadlock(int x, int y, int size) {
    DrawCircle(x, y - size / 2, size / 2, YELLOW, FALSE, 3);
    DrawBox(x - size * 3 / 4, y - size / 3, x + size * 3 / 4, y + size * 2 / 3, YELLOW, TRUE);
    DrawCircle(x, y + 2, size / 6 + 1, BLACK, TRUE);
}

static void DrawPokeCard(int charaIdx, float offsetX, float scale, bool isSelected, int rank, bool noCon, bool locked, bool isNew) {
    int centerX = 320 + (int)offsetX;
    int centerY = 210;

    int cardW = (int)(180.0f * scale);
    int cardH = (int)(240.0f * scale);
    int x1 = centerX - cardW / 2;
    int y1 = centerY - cardH / 2;
    int x2 = centerX + cardW / 2;
    int y2 = centerY + cardH / 2;

    int illX1 = x1 + (int)(12.0f * scale);
    int illY1 = y1 + (int)(12.0f * scale);
    int illX2 = x2 - (int)(12.0f * scale);
    int illY2 = y1 + (int)(135.0f * scale);
    int artX = (illX1 + illX2) / 2;
    int artY = (illY1 + illY2) / 2;
    int rad = (int)(28.0f * scale);

    // 未解放キャラ：シルエットと鍵でチラ見せ
    if (locked) {
        DrawBox(x1, y1, x2, y2, GetColor(10, 10, 16), TRUE);
        DrawBox(x1, y1, x2, y2, isSelected ? YELLOW : GetColor(80, 70, 40), FALSE);
        DrawBox(illX1, illY1, illX2, illY2, GetColor(20, 20, 28), TRUE);
        DrawCircle(artX, artY - (int)(18 * scale), (int)(16 * scale), GetColor(40, 40, 50), TRUE);
        DrawBox(artX - (int)(22 * scale), artY, artX + (int)(22 * scale), illY2 - 4, GetColor(40, 40, 50), TRUE);
        DrawPadlock(artX, artY, (int)(26.0f * scale));
        if (scale > 0.85f) {
            DrawString(x1 + 8, y1 + (int)(145.0f * scale), "？？？？？", GRAY);
            DrawString(x1 + 8, y1 + (int)(170.0f * scale), "LOCKED", RED);
            DrawFormatString(x1 + 8, y1 + (int)(195.0f * scale), YELLOW, "%d周目で解放", CHARA_PROFILES[charaIdx].unlockLap);
        }
        return;
    }

    DrawBox(x1, y1, x2, y2, GetColor(18, 22, 36), TRUE);

    bool isCleared = rank > 0;
    unsigned int borderCol = isCleared ? YELLOW : (isSelected ? WHITE : GetColor(70, 75, 95));
    if (rank >= 2) borderCol = GetColor(128 + (g_frameCount * 5) % 128, 200, 255 - (g_frameCount * 3) % 128);
    DrawBox(x1, y1, x2, y2, borderCol, FALSE);
    if (isSelected || isCleared) {
        DrawBox(x1 - 2, y1 - 2, x2 + 2, y2 + 2, borderCol, FALSE);
        DrawLine(x1 + 10, y1 + 10, x1 + 60, y1 + 10, WHITE);
        DrawLine(x1 + 10, y1 + 10, x1 + 10, y1 + 60, WHITE);
    }

    DrawBox(illX1, illY1, illX2, illY2, GetColor(28, 34, 52), TRUE);
    DrawCircle(artX, artY, rad, CHARA_PROFILES[charaIdx].baseColor, TRUE);
    DrawCircle(artX, artY, rad, WHITE, FALSE);

    if (charaIdx == 0) {
        DrawCircle(artX, artY, (int)(14.0f * scale), ORANGE, TRUE);
        DrawCircle(artX, artY, (int)(8.0f * scale), YELLOW, TRUE);
    }
    else if (charaIdx == 1) {
        DrawTriangle(artX, artY - (int)(26 * scale), artX - (int)(14 * scale), artY + (int)(8 * scale), artX + (int)(14 * scale), artY + (int)(8 * scale), ORANGE, TRUE);
        DrawLine(artX - (int)(7 * scale), artY - (int)(8 * scale), artX + (int)(7 * scale), artY - (int)(8 * scale), WHITE, 3);
    }
    else if (charaIdx == 2) {
        DrawCircle(artX, artY, rad + 4, YELLOW, FALSE);
    }
    else if (charaIdx == 3) {
        DrawCircle(artX + 10, artY + 4, (int)(8 * scale), GetColor(230, 150, 160), TRUE);
        DrawLine(artX + 4, artY + 10, artX + 20, artY - 4, WHITE, 3);
        DrawLine(artX - 4, artY + 10, artX - 20, artY - 4, WHITE, 3);
    }
    else if (charaIdx == 4) {
        DrawLine(artX - (int)(40 * scale), artY + (int)(25 * scale), artX + (int)(45 * scale), artY - (int)(35 * scale), GetColor(200, 205, 215), 5);
        DrawBox(artX - 18, artY + 4, artX + 18, artY + 14, GetColor(50, 40, 20), TRUE);
    }
    else if (charaIdx == 6) {
        DrawCircle(artX, artY, rad + 4, YELLOW, FALSE, 2);
        DrawCircle(artX, artY, rad + 9, ORANGE, FALSE, 1);
    }
    else if (charaIdx == 7) {
        DrawLine(artX - 6, artY + 8, artX + (int)(38 * scale), artY - (int)(14 * scale), WHITE, 5);
        DrawLine(artX - 14, artY - 12, artX + 14, artY - 12, RED, 3);
    }
    else if (charaIdx == 8) {
        DrawString(artX - 16, artY - 8, "ﾆﾔﾘ", WHITE);
        DrawCircle(artX, artY, rad + 4, CYAN, FALSE, 2);
    }

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
        DrawFormatString(x1 + 8, y1 + (int)(145.0f * scale), WHITE, "%s", CHARA_PROFILES[charaIdx].charaId);
        DrawFormatString(x1 + 8, y1 + (int)(170.0f * scale), YELLOW, "HP: %d", PlayableMaxHp(charaIdx));
        DrawFormatString(x1 + 8, y1 + (int)(195.0f * scale), CYAN, "SPD: %.1f", CHARA_PROFILES[charaIdx].walkSpeed);
        if (isCleared) {
            DrawFormatString(x1 + 8, y1 + (int)(216.0f * scale), ORANGE, "『%s』", CHARA_TITLES[charaIdx][rank - 1]);
        }
    }
}

// ----------------------------------------------------
// 周回処理
// ----------------------------------------------------

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
    g_mode = MODE_CHARA_SELECT;
}

// ----------------------------------------------------
// メイン関数
// ----------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    ChangeWindowMode(TRUE);
    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;

    srand((unsigned int)time(NULL));
    InitMasterData();
    LoadAllSlots();

    // 通信対戦で選べるキャラ（最初から使えるキャラのみ）
    int netCharaList[CHARA_COUNT];
    int netCharaCount = 0;
    for (int i = 0; i < ROSTER_COUNT; i++) {
        if (CHARA_PROFILES[g_rosterOrder[i]].unlockLap <= 1) netCharaList[netCharaCount++] = g_rosterOrder[i];
    }

    while (ProcessMessage() == 0) {
        ClearDrawScreen();
        g_frameCount++;
        if (s_keyCooldown > 0) s_keyCooldown--;

        int curStage = ClampI(g_slots[g_currentSlot].stageProgress, 0, LAST_STAGE);

        switch (g_mode) {
        case MODE_TITLE: {
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(140, 70, "=== STREET DUEL: ARCADE CLASH ===", YELLOW);
            DrawString(190, 110, "世界格闘チャンピオンシップ", WHITE);

            DrawString(220, 210, (g_titleMenu == 0) ? "> MAIN STORY (CPU勝ち抜き)" : "  MAIN STORY (CPU勝ち抜き)", (g_titleMenu == 0) ? YELLOW : WHITE);
            DrawString(220, 250, (g_titleMenu == 1) ? "> LAN VERSUS (ローカル通信対戦)" : "  LAN VERSUS (ローカル通信対戦)", (g_titleMenu == 1) ? YELLOW : WHITE);
            DrawString(220, 290, (g_titleMenu == 2) ? "> EXIT" : "  EXIT", (g_titleMenu == 2) ? YELLOW : WHITE);

            DrawString(180, 410, "[W/S]: 選択   [ENTER]: 決定", GRAY);

            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP)) { g_titleMenu = (g_titleMenu + 2) % 3; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) { g_titleMenu = (g_titleMenu + 1) % 3; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 15;
                    if (g_titleMenu == 0) {
                        g_is_net_battle = false;
                        g_mode = MODE_SLOT_SELECT;
                    }
                    else if (g_titleMenu == 1) {
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
                unsigned int borderCol = (i == g_slotCursor) ? YELLOW : GRAY;
                DrawBox(50, sy, 590, sy + 95, GetColor(25, 30, 45), TRUE);
                DrawBox(50, sy, 590, sy + 95, borderCol, FALSE);

                const SaveSlot& s = g_slots[i];
                if (s.inUse) {
                    if (s.needCharaSelect) {
                        DrawFormatString(70, sy + 12, YELLOW, "[ DATA %d ] - %d周目  キャラ選択から開始", i + 1, s.loopCount + 1);
                    }
                    else {
                        DrawFormatString(70, sy + 12, YELLOW, "[ DATA %d ] - %d周目  %s",
                            i + 1, s.loopCount + 1, STAGES[ClampI(s.stageProgress, 0, LAST_STAGE)].name);
                    }
                    DrawFormatString(70, sy + 38, CYAN, "ファイター: %s    称号: %d個",
                        CHARA_PROFILES[s.chosenChara].charaId, CountTitles(s));

                    float winRate = 0.0f;
                    if (s.totalFights > 0) {
                        winRate = ((float)s.wins / (float)s.totalFights) * 100.0f;
                    }
                    DrawFormatString(70, sy + 64, WHITE, "戦績: %d戦 %d勝 %d敗 %d分  (勝率 %.1f%%)",
                        s.totalFights, s.wins, s.losses, s.draws, winRate);
                }
                else {
                    DrawFormatString(70, sy + 25, GRAY, "[ DATA %d ] - NEW GAME (データなし)", i + 1);
                    DrawFormatString(70, sy + 55, GRAY, "選択すると新規キャラクター選択からスタートします");
                }
            }

            DrawString(70, 425, "[W/S]: データ選択   [ENTER]: 決定/再開   [DEL]: データ消去", WHITE);
            DrawString(70, 450, "[ESC]: タイトル画面へ戻る", GRAY);

            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP)) { g_slotCursor = (g_slotCursor + 2) % 3; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) { g_slotCursor = (g_slotCursor + 1) % 3; s_keyCooldown = 12; }

                if (CheckHitKey(KEY_INPUT_DELETE)) {
                    DeleteSlot(g_slotCursor);
                    s_keyCooldown = 15;
                }

                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    g_currentSlot = g_slotCursor;
                    s_keyCooldown = 15;
                    if (g_slots[g_currentSlot].inUse && !g_slots[g_currentSlot].needCharaSelect) {
                        g_mode = MODE_VS_SCREEN;
                        g_phaseTimer = 0;
                    }
                    else {
                        EnterCharaSelect(false);
                    }
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                    g_mode = MODE_TITLE;
                    s_keyCooldown = 15;
                }
            }
            break;
        }

        case MODE_CHARA_SELECT: {
            const SaveSlot& slot = g_slots[g_currentSlot];
            int lap = GetCurrentLap();
            int hard = GetHardLevel();

            DrawBox(0, 0, 640, 480, GetColor(8, 10, 18), TRUE);
            DrawFormatString(210, 20, YELLOW, "[ DATA %d ] SELECT CARD", g_currentSlot + 1);
            if (hard > 0) DrawFormatString(450, 20, ORANGE, "%d周目 (難易度+%d)", lap, hard);
            else DrawFormatString(450, 20, GRAY, "%d周目", lap);
            DrawString(180, 45, "コレクションからファイターを選択", GRAY);

            float targetScroll = (float)g_charaCursor * 210.0f;
            g_cardScrollX += (targetScroll - g_cardScrollX) * 0.18f;

            for (int i = 0; i < ROSTER_COUNT; i++) {
                int c = g_rosterOrder[i];
                float distFromCenter = ((float)i * 210.0f) - g_cardScrollX;
                float absDist = fabsf(distFromCenter);
                if (absDist > 700.0f) continue;

                float scale = 1.0f - (absDist / 550.0f);
                if (scale < 0.65f) scale = 0.65f;

                bool locked = !IsCharaUnlocked(c);
                bool isNew = !locked && CHARA_PROFILES[c].unlockLap == lap && lap > 1;
                DrawPokeCard(c, distFromCenter, scale, (i == g_charaCursor && absDist < 25.0f),
                    slot.clearRank[c], slot.noContinueClear[c], locked, isNew);
            }

            int cur = g_rosterOrder[g_charaCursor];
            bool curLocked = !IsCharaUnlocked(cur);

            DrawBox(30, 340, 610, 440, GetColor(20, 24, 38), TRUE);
            DrawBox(30, 340, 610, 440, YELLOW, FALSE);

            DrawFormatString(45, 348, YELLOW, "%s", CHARA_PROFILES[cur].archetype);
            if (curLocked) {
                DrawString(45, 370, "NAME    : ？？？？？", GRAY);
                DrawFormatString(45, 392, RED, "LOCKED  : %d周目から使用可能（クリアして周回すると解放）", CHARA_PROFILES[cur].unlockLap);
                DrawString(45, 414, "HINT    : 操作は難しいが、使いこなせば最強クラス…？", GRAY);
            }
            else {
                DrawFormatString(45, 370, WHITE, "NAME    : %s", CHARA_PROFILES[cur].charaId);
                DrawFormatString(45, 392, CYAN, "SKILL   : %s", CHARA_PROFILES[cur].specialDesc);
                if (slot.clearRank[cur] > 0) {
                    DrawFormatString(45, 414, ORANGE, "称号    : 『%s』 クリア%d回%s",
                        CHARA_TITLES[cur][slot.clearRank[cur] - 1], slot.clearCount[cur],
                        slot.noContinueClear[cur] ? " / ノーコン達成" : "");
                }
                else {
                    DrawString(45, 414, "称号    : まだなし（クリアで獲得）", GRAY);
                }
            }

            DrawString(160, 455, "[A/D]: カードをめくる   [ENTER]: 決定して参戦   [ESC]: 戻る", WHITE);

            if (g_unlockBannerTimer > 0) {
                g_unlockBannerTimer--;
                DrawBox(60, 64, 580, 92, GetColor(40, 0, 0), TRUE);
                DrawBox(60, 64, 580, 92, (g_frameCount / 6) % 2 ? YELLOW : RED, FALSE);
                DrawFormatString(80, 70, YELLOW, "NEW CHALLENGER!!  『%s』が参戦可能になった！",
                    CHARA_PROFILES[g_unlockBannerChara].charaId);
            }
            if (g_lockedMsgTimer > 0) {
                g_lockedMsgTimer--;
                DrawBox(150, 190, 490, 230, BLACK, TRUE);
                DrawBox(150, 190, 490, 230, RED, FALSE);
                DrawFormatString(170, 202, RED, "まだ使えない！ %d周目で解放されます", CHARA_PROFILES[cur].unlockLap);
            }

            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_A) || CheckHitKey(KEY_INPUT_LEFT)) {
                    if (g_charaCursor > 0) g_charaCursor--;
                    s_keyCooldown = 12;
                }
                if (CheckHitKey(KEY_INPUT_D) || CheckHitKey(KEY_INPUT_RIGHT)) {
                    if (g_charaCursor < ROSTER_COUNT - 1) g_charaCursor++;
                    s_keyCooldown = 12;
                }
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 20;
                    if (curLocked) {
                        g_lockedMsgTimer = 70;
                    }
                    else {
                        SaveSlot& s = g_slots[g_currentSlot];
                        s.inUse = true;
                        s.chosenChara = cur;
                        s.stageProgress = 0;
                        s.totalFights = 0;
                        s.wins = 0;
                        s.losses = 0;
                        s.draws = 0;
                        s.hasContinued = false;
                        s.hadTimeOver = false;
                        s.needCharaSelect = false;
                        SaveCurrentSlot();
                        g_mode = MODE_VS_SCREEN;
                        g_phaseTimer = 0;
                    }
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                    g_mode = MODE_SLOT_SELECT;
                    s_keyCooldown = 15;
                }
            }
            break;
        }

        case MODE_VS_SCREEN: {
            g_phaseTimer++;
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            const SaveSlot& s = g_slots[g_currentSlot];

            DrawString(240, 60, "BATTLE MATCH-UP", YELLOW);
            DrawFormatString(180, 100, CYAN, "%s", STAGES[curStage].name);
            if (GetHardLevel() > 0) {
                DrawFormatString(200, 125, ORANGE, "%d周目：敵の攻撃力・AI強化 (難易度+%d)", GetCurrentLap(), GetHardLevel());
            }

            DrawBox(50, 150, 270, 360, GetColor(25, 30, 50), TRUE);
            DrawBox(50, 150, 270, 360, CYAN, FALSE);
            DrawBox(120, 180, 200, 260, CHARA_PROFILES[s.chosenChara].baseColor, TRUE);
            DrawFormatString(70, 280, WHITE, "DATA %d", g_currentSlot + 1);
            DrawFormatString(70, 310, YELLOW, "%s", CHARA_PROFILES[s.chosenChara].charaId);
            if (s.clearRank[s.chosenChara] > 0) {
                DrawFormatString(70, 334, ORANGE, "『%s』", CHARA_TITLES[s.chosenChara][s.clearRank[s.chosenChara] - 1]);
            }

            DrawString(300, 240, "VS", RED);

            DrawBox(370, 150, 590, 360, GetColor(50, 25, 30), TRUE);
            DrawBox(370, 150, 590, 360, RED, FALSE);
            DrawBox(440, 180, 520, 260, CHARA_PROFILES[CPU_ROSTER[curStage].charaIndex].baseColor, TRUE);
            DrawFormatString(390, 280, WHITE, "%s", CPU_ROSTER[curStage].name);
            DrawFormatString(390, 310, YELLOW, "%s", CPU_ROSTER[curStage].alias);

            if (g_phaseTimer > 120 || (CheckHitKey(KEY_INPUT_RETURN) && s_keyCooldown == 0 && g_phaseTimer > 20)) {
                SetupBattleRound(false);
                g_mode = MODE_BATTLE_PLAY;
            }
            break;
        }

        case MODE_BATTLE_PLAY: {
            if (g_readyFightTimer < 80) {
                g_readyFightTimer++;
                if (g_readyFightTimer == 50) TriggerScreenShake(8);
            }
            else {
                g_battleTimerFrames++;
                if (g_battleTimerFrames >= 60) {
                    g_battleTimerFrames = 0;
                    if (g_roundTime > 0) g_roundTime--;
                }
            }

            UpdatePlayer();
            UpdateCpu();
            UpdateTraps();
            UpdateStageGimmick(curStage);
            UpdateShotsAndFX();

            int oy = g_shakeOffsetY;

            // ==========================================
            // 黒閃・空間反転ストロボ演出（画面全体が漆黒と真紅に反転）
            // ==========================================
            if (g_kokusenStrobe > 0) {
                if (g_kokusenStrobe % 4 < 2) {
                    DrawBox(0, 0, 640, 480, GetColor(0, 0, 0), TRUE); // 漆黒
                }
                else {
                    DrawBox(0, 0, 640, 480, GetColor(180, 10, 10), TRUE); // 真紅
                }
                DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, GetColor(10, 10, 10), TRUE);
                DrawLine(0, (int)GROUND_Y + 25 + oy, 640, (int)GROUND_Y + 25 + oy, RED);
            }
            else {
                DrawBox(0, 0, 640, 480, STAGES[curStage].skyColor, TRUE);
                DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, STAGES[curStage].groundColor, TRUE);
                DrawLine(0, (int)GROUND_Y + 25 + oy, 640, (int)GROUND_Y + 25 + oy, WHITE);
            }

            DrawGimmick(curStage);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            DrawHud(false);

            if (g_me.hp <= 0 || g_other.hp <= 0) {
                g_roundWon = (g_me.hp > 0);
                g_phaseTimer = 0;
                TriggerScreenShake(g_isKokusenKoFinish ? 25 : 16);
                g_slots[g_currentSlot].totalFights++;
                if (g_roundWon) g_slots[g_currentSlot].wins++;
                else g_slots[g_currentSlot].losses++;
                SaveCurrentSlot();
                g_mode = MODE_KO_DISPLAY;
            }
            else if (g_roundTime <= 0) {
                g_slots[g_currentSlot].hadTimeOver = true;
                float myR = (float)g_me.hp / (float)g_me.maxHp;
                float othR = (float)g_other.hp / (float)g_other.maxHp;
                g_slots[g_currentSlot].totalFights++;
                if (fabsf(myR - othR) < 0.001f) {
                    g_isDraw = true; g_roundWon = false; g_slots[g_currentSlot].draws++;
                }
                else if (myR > othR) {
                    g_isDraw = false; g_roundWon = true; g_slots[g_currentSlot].wins++;
                }
                else {
                    g_isDraw = false; g_roundWon = false; g_slots[g_currentSlot].losses++;
                }
                SaveCurrentSlot();
                g_phaseTimer = 0;
                TriggerScreenShake(12);
                g_mode = MODE_TIME_OVER;
            }
            break;
        }

        case MODE_TIME_OVER: {
            g_phaseTimer++;
            UpdateShotsAndFX();
            int oy = g_shakeOffsetY;

            DrawBox(0, 0, 640, 480, STAGES[curStage].skyColor, TRUE);
            DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, STAGES[curStage].groundColor, TRUE);
            DrawGimmick(curStage);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            DrawHud(false);

            DrawBox(160, 180, 480, 260, GetColor(0, 0, 0), TRUE);
            DrawBox(160, 180, 480, 260, (g_roundWon ? YELLOW : RED), FALSE);
            if (g_isDraw) DrawString(260, 205, "DRAW  GAME", GRAY);
            else DrawString(245, 205, "TIME  OVER !!", (g_roundWon ? YELLOW : RED));

            if (g_phaseTimer > 110) {
                g_phaseTimer = 0;
                if (g_roundWon) {
                    g_slots[g_currentSlot].stageProgress++;
                    SaveCurrentSlot();
                    g_mode = MODE_WIN_QUOTE;
                }
                else {
                    g_mode = MODE_GAME_OVER;
                }
            }
            break;
        }

                           // ==========================================
                           // KO画面（最強技KOフィニッシュなら超豪華演出）
                           // ==========================================
        case MODE_KO_DISPLAY: {
            g_phaseTimer++;
            UpdateShotsAndFX();
            int oy = g_shakeOffsetY;

            if (g_isKokusenKoFinish) {
                // 画面をレインボー＆ストロボで彩る
                int bgCol = (g_phaseTimer % 6 < 3) ? GetColor(15, 0, 0) : GetColor(0, 0, 0);
                DrawBox(0, 0, 640, 480, bgCol, TRUE);
                DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, GetColor(30, 10, 10), TRUE);
            }
            else {
                DrawBox(0, 0, 640, 480, STAGES[curStage].skyColor, TRUE);
                DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, STAGES[curStage].groundColor, TRUE);
            }

            DrawGimmick(curStage);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            DrawHud(false);

            if (g_isKokusenKoFinish) {
                // パチンコ風・極限神撃K.O.特大フィニッシュ枠
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
                DrawBox(180, 180, 460, 260, RED, FALSE);
                DrawString(280, 205, "K. O.", YELLOW);
            }

            if (g_phaseTimer > (g_isKokusenKoFinish ? 160 : 110)) {
                g_phaseTimer = 0;
                if (g_roundWon) {
                    g_slots[g_currentSlot].stageProgress++;
                    SaveCurrentSlot();
                    g_mode = MODE_WIN_QUOTE;
                }
                else {
                    g_mode = MODE_GAME_OVER;
                }
            }
            break;
        }

        case MODE_WIN_QUOTE: {
            g_phaseTimer++;
            SaveSlot& s = g_slots[g_currentSlot];
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            int prevStage = ClampI(s.stageProgress - 1, 0, LAST_STAGE);
            bool secretNext = (s.stageProgress == LAST_STAGE && !s.hasContinued && !s.hadTimeOver);

            DrawString(220, 80, "=== VICTORY! ===", YELLOW);
            DrawFormatString(120, 160, WHITE, "敗者 %s: 「%s」", CPU_ROSTER[prevStage].name, CPU_ROSTER[prevStage].winQuote);

            float winRate = 0.0f;
            if (s.totalFights > 0) {
                winRate = ((float)s.wins / (float)s.totalFights) * 100.0f;
            }
            DrawFormatString(120, 220, CYAN, "通算戦績: %d戦 %d勝 %d敗 %d分 (勝率: %.1f%%)",
                s.totalFights, s.wins, s.losses, s.draws, winRate);
            if (secretNext) DrawString(120, 255, "……ただならぬ気配！ 何者かが乱入してくる！", RED);
            else if (s.stageProgress >= LAST_STAGE) DrawString(120, 255, "全ての強敵を打ち倒した！", GREEN);
            else DrawString(120, 255, "次の戦場へ向かう……", GREEN);

            DrawString(180, 360, "[Enter]: 次へ進む", WHITE);

            if (CheckHitKey(KEY_INPUT_RETURN) && g_phaseTimer > 40 && s_keyCooldown == 0) {
                s_keyCooldown = 20;
                g_phaseTimer = 0;

                if (s.stageProgress == LAST_STAGE) {
                    if (secretNext) {
                        g_mode = MODE_VS_SCREEN;
                    }
                    else {
                        OnRunCleared(false);
                        g_mode = MODE_ENDING;
                    }
                }
                else if (s.stageProgress > LAST_STAGE) {
                    OnRunCleared(true);
                    g_mode = MODE_ENDING;
                }
                else {
                    g_mode = MODE_VS_SCREEN;
                }
            }
            break;
        }

        case MODE_GAME_OVER: {
            DrawBox(0, 0, 640, 480, GetColor(10, 0, 0), TRUE);
            DrawString(240, 140, "=== DEFEAT... ===", RED);
            DrawString(220, 200, "CONTINUE CHALLENGE?", WHITE);

            DrawString(200, 290, "[Enter]: このステージから再挑戦", GREEN);
            DrawString(200, 330, "[Esc]: スロット選択画面へ戻る", GRAY);

            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 20;
                    g_slots[g_currentSlot].hasContinued = true;
                    SaveCurrentSlot();
                    g_mode = MODE_VS_SCREEN;
                    g_phaseTimer = 0;
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                    s_keyCooldown = 20;
                    g_mode = MODE_SLOT_SELECT;
                }
            }
            break;
        }

        case MODE_ENDING: {
            g_phaseTimer++;
            const SaveSlot& s = g_slots[g_currentSlot];
            int c = s.chosenChara;
            DrawBox(0, 0, 640, 480, GetColor(20, 25, 40), TRUE);

            if (g_endingSecret) {
                unsigned int cc = ((g_phaseTimer / 6) % 2) ? YELLOW : ORANGE;
                DrawExtendString(236, 36, 3.0, 3.0, "CLEAR!!", cc);
                DrawString(150, 105, "【伝説達成】裏ボス・真 天輪太子を撃破した！", RED);
            }
            else {
                DrawExtendString(184, 44, 2.0, 2.0, "FINAL STAGE CLEAR", YELLOW);
                DrawString(90, 105, "※ノーコンティニュー＆タイムオーバー無しで… 何かが現れる？", GRAY);
            }

            DrawFormatString(150, 145, WHITE, "使用キャラ: %s", CHARA_PROFILES[c].charaId);
            DrawFormatString(150, 172, YELLOW, "称号『%s』を獲得！", CHARA_TITLES[c][g_endingRank - 1]);

            float winRate = 0.0f;
            if (s.totalFights > 0) {
                winRate = ((float)s.wins / (float)s.totalFights) * 100.0f;
            }
            DrawFormatString(150, 205, CYAN, "最終戦績: %d戦 %d勝 %d敗 %d分 (勝率: %.1f%%)",
                s.totalFights, s.wins, s.losses, s.draws, winRate);
            DrawFormatString(150, 240, ORANGE, "次は %d周目：敵の攻撃力・AIが強化されます (難易度+%d)", GetCurrentLap(), GetHardLevel());

            if (g_endingUnlockChara >= 0 && (g_phaseTimer / 10) % 3 != 0) {
                DrawFormatString(110, 280, CYAN, "NEW CHALLENGER!! 次の周回から『%s』が使用可能に！",
                    CHARA_PROFILES[g_endingUnlockChara].charaId);
            }

            DrawString(170, 370, "[Enter]: キャラクター選択へ (周回プレイ)", WHITE);
            DrawString(170, 395, "[Esc]: タイトルへ戻る", GRAY);

            if (s_keyCooldown == 0 && g_phaseTimer > 60) {
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 20;
                    EnterCharaSelect(true);
                }
                else if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                    s_keyCooldown = 20;
                    g_mode = MODE_TITLE;
                }
            }
            break;
        }

                        // ==========================================
                        // LAN VERSUS
                        // ==========================================
        case MODE_NET_MENU: {
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(200, 80, "=== LAN VERSUS MENU ===", YELLOW);
            DrawString(210, 180, (g_netMenu == 0) ? "> HOST 作成 (部屋を開く)" : "  HOST 作成 (部屋を開く)", (g_netMenu == 0) ? YELLOW : WHITE);
            DrawString(210, 220, (g_netMenu == 1) ? "> CLIENT 参加 (IP接続)" : "  CLIENT 参加 (IP接続)", (g_netMenu == 1) ? YELLOW : WHITE);

            DrawString(190, 380, "[W/S]: 選択   [ENTER]: 決定   [ESC]: 戻る", GRAY);

            if (s_keyCooldown == 0) {
                if (CheckHitKey(KEY_INPUT_W) || CheckHitKey(KEY_INPUT_UP)) { g_netMenu = 0; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_S) || CheckHitKey(KEY_INPUT_DOWN)) { g_netMenu = 1; s_keyCooldown = 12; }
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 15;
                    if (g_netMenu == 0) StartHost();
                    else g_mode = MODE_CLIENT_INPUT;
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                    g_mode = MODE_TITLE;
                    s_keyCooldown = 15;
                }
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

            if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                CloseSocket(g_listen);
                g_mode = MODE_NET_MENU;
                s_keyCooldown = 15;
            }
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

            if (CheckHitKey(KEY_INPUT_RETURN)) {
                StartClient(g_client_ip_input);
                s_keyCooldown = 15;
            }
            if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                g_mode = MODE_NET_MENU;
                s_keyCooldown = 15;
            }
            break;
        }

        case MODE_NET_STAGE_SELECT: {
            ReceivePackets();
            DrawBox(0, 0, 640, 480, GetColor(10, 8, 20), TRUE);
            DrawString(240, 20, "STAGE SELECT", PURPLE);

            DrawBox(40, 50, 600, 240, STAGES[g_selectedNetStage].skyColor, TRUE);
            DrawBox(40, 50, 600, 240, WHITE, FALSE);
            DrawFormatString(60, 80, YELLOW, "%s", STAGES[g_selectedNetStage].name);

            for (int i = 0; i < 5; i++) {
                int bx = 45 + i * 110;
                int by = 270;
                DrawBox(bx, by, bx + 100, by + 80, STAGES[i].skyColor, TRUE);
                DrawBox(bx, by, bx + 100, by + 80, (i == g_selectedNetStage) ? YELLOW : GRAY, FALSE);
                DrawFormatString(bx + 8, by + 30, WHITE, "STAGE %d", i + 1);
            }

            if (g_is_host) {
                DrawString(160, 400, "A / D: ステージ切り替え   Enter: 決定", WHITE);
                if (s_keyCooldown == 0) {
                    if (CheckHitKey(KEY_INPUT_A)) { g_selectedNetStage = (g_selectedNetStage + 4) % 5; s_keyCooldown = 12; }
                    if (CheckHitKey(KEY_INPUT_D)) { g_selectedNetStage = (g_selectedNetStage + 1) % 5; s_keyCooldown = 12; }
                    if (CheckHitKey(KEY_INPUT_RETURN)) {
                        SendFighterPacket(PKT_STAGE_SYNC, 0, 0, 0, 0, 0, 0, 0, g_selectedNetStage);
                        g_mode = MODE_NET_CHARA_SELECT;
                        s_keyCooldown = 15;
                    }
                }
            }
            else {
                DrawString(200, 400, "ホストがステージを選んでいます...", YELLOW);
            }
            break;
        }

        case MODE_NET_CHARA_SELECT: {
            ReceivePackets();
            DrawBox(0, 0, 640, 480, GetColor(14, 16, 26), TRUE);
            DrawString(225, 18, "CHARACTER SELECT", YELLOW);

            g_netCharaCursor = ClampI(g_netCharaCursor, 0, netCharaCount - 1);
            g_myNetChara = netCharaList[g_netCharaCursor];

            DrawBox(30, 45, 290, 275, GetColor(25, 30, 45), TRUE);
            DrawBox(30, 45, 290, 275, g_myNetReady ? GREEN : RED, FALSE);
            DrawString(45, 55, "1P: YOU", g_myNetReady ? GREEN : RED);
            DrawBox(140, 100, 180, 160, CHARA_PROFILES[g_myNetChara].baseColor, TRUE);
            DrawFormatString(45, 180, WHITE, "NAME: %s", CHARA_PROFILES[g_myNetChara].charaId);

            DrawBox(350, 45, 610, 275, GetColor(35, 20, 25), TRUE);
            DrawBox(350, 45, 610, 275, g_enemyNetReady ? GREEN : BLUE, FALSE);
            DrawString(365, 55, "2P: REMOTE", g_enemyNetReady ? GREEN : BLUE);
            DrawBox(460, 100, 500, 160, CHARA_PROFILES[g_enemyNetChara].baseColor, TRUE);
            DrawFormatString(365, 180, WHITE, "NAME: %s", CHARA_PROFILES[g_enemyNetChara].charaId);

            for (int i = 0; i < netCharaCount; i++) {
                int cx = 22 + i * 100;
                int cy = 295;
                int c = netCharaList[i];
                DrawBox(cx, cy, cx + 94, cy + 90, GetColor(20, 25, 35), TRUE);
                DrawBox(cx, cy, cx + 94, cy + 90, (i == g_netCharaCursor) ? YELLOW : GRAY, FALSE);
                DrawBox(cx + 32, cy + 20, cx + 62, cy + 50, CHARA_PROFILES[c].baseColor, TRUE);
                DrawFormatString(cx + 4, cy + 60, WHITE, "%s", CHARA_PROFILES[c].charaId);
            }

            if (!g_myNetReady) {
                DrawString(160, 415, "A / D: キャラ選択   Enter: 決定", WHITE);
                if (s_keyCooldown == 0) {
                    if (CheckHitKey(KEY_INPUT_A)) { g_netCharaCursor = (g_netCharaCursor + netCharaCount - 1) % netCharaCount; s_keyCooldown = 12; }
                    if (CheckHitKey(KEY_INPUT_D)) { g_netCharaCursor = (g_netCharaCursor + 1) % netCharaCount; s_keyCooldown = 12; }
                    if (CheckHitKey(KEY_INPUT_RETURN)) {
                        g_myNetChara = netCharaList[g_netCharaCursor];
                        g_myNetReady = true;
                        SendFighterPacket(PKT_CHARA_SYNC, 0, 0, 0, 0, 0, 0, 0, g_myNetChara);
                        s_keyCooldown = 15;
                    }
                }
            }
            else {
                DrawString(200, 415, "相手の決定を待機中...", YELLOW);
            }

            if (g_myNetReady && g_enemyNetReady) {
                SetupBattleRound(true);
                WaitTimer(300);
                g_mode = MODE_NET_BATTLE_PLAY;
            }
            break;
        }

        case MODE_NET_BATTLE_PLAY: {
            if (g_readyFightTimer < 80) {
                g_readyFightTimer++;
                if (g_readyFightTimer == 50) TriggerScreenShake(8);
            }
            else {
                g_battleTimerFrames++;
                if (g_battleTimerFrames >= 60) {
                    g_battleTimerFrames = 0;
                    if (g_roundTime > 0) g_roundTime--;
                }
            }

            UpdatePlayer();
            UpdateTraps();
            UpdateShotsAndFX();

            if (g_is_host) {
                SendFighterPacket(PKT_STATE, g_me.x, g_me.y, g_me.vx, g_me.vy, g_me.hp, (int)g_me.state, g_me.dir, 0);
            }
            else {
                SendFighterPacket(PKT_INPUT, g_me.x, g_me.y, g_me.vx, g_me.vy, g_me.hp, (int)g_me.state, g_me.dir, 0);
            }
            ReceivePackets();

            int oy = g_shakeOffsetY;

            if (g_kokusenStrobe > 0) {
                if (g_kokusenStrobe % 4 < 2) DrawBox(0, 0, 640, 480, GetColor(0, 0, 0), TRUE);
                else DrawBox(0, 0, 640, 480, GetColor(180, 10, 10), TRUE);
                DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, GetColor(10, 10, 10), TRUE);
            }
            else {
                DrawBox(0, 0, 640, 480, STAGES[g_selectedNetStage].skyColor, TRUE);
                DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, STAGES[g_selectedNetStage].groundColor, TRUE);
                DrawLine(0, (int)GROUND_Y + 25 + oy, 640, (int)GROUND_Y + 25 + oy, WHITE);
            }

            DrawGimmick(g_selectedNetStage);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            DrawHud(true);

            if (g_me.hp <= 0 || g_other.hp <= 0) {
                g_roundWon = (g_me.hp > 0);
                g_mode = MODE_NET_RESULT;
            }
            if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                CloseSocket(g_peer);
                g_mode = MODE_TITLE;
                s_keyCooldown = 15;
            }
            break;
        }

        case MODE_NET_RESULT: {
            DrawBox(0, 0, 640, 480, GetColor(10, 10, 15), TRUE);
            if (g_roundWon) {
                DrawString(240, 160, "=== YOU WIN! ===", YELLOW);
            }
            else {
                DrawString(240, 160, "=== YOU LOSE... ===", RED);
            }
            DrawString(190, 310, "[Enter] 再戦 (ステージ選択へ)", GREEN);
            DrawString(190, 350, "[ESC] メニューへ戻る", GRAY);

            if (CheckHitKey(KEY_INPUT_RETURN)) {
                g_myNetReady = false;
                g_enemyNetReady = false;
                g_mode = MODE_NET_STAGE_SELECT;
                s_keyCooldown = 15;
            }
            if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                CloseSocket(g_peer);
                g_mode = MODE_TITLE;
                WaitTimer(200);
            }
            break;
        }

        case MODE_NET_ERROR: {
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(100, 180, "[通信エラー / 切断]", RED);
            DrawString(100, 220, g_net_status, WHITE);
            DrawString(100, 280, "[ESC] タイトルへ戻る", GRAY);
            if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                g_mode = MODE_TITLE;
                s_keyCooldown = 15;
            }
            break;
        }
        }

        ScreenFlip();
    }

EXIT_GAME:
    CloseSocket(g_peer);
    CloseSocket(g_listen);
    WSACleanup();
    DxLib_End();
    return 0;
}
