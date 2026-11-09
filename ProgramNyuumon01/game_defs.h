// ====================================================
// 定数・列挙・構造体・グローバル変数・テキストデータ
//   main.cpp から一度だけ読み込まれる（単一の翻訳単位）
// ====================================================
#pragma once

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
static const unsigned int SAVE_VERSION = 20261109;
static const int FLOOR_DRAW_OFF = 25;   // f.y（論理の足元）から画面の床の線までの距離

enum GameMode {
    MODE_TITLE,
    MODE_TUTORIAL,
    MODE_SLOT_SELECT,
    MODE_CHARA_SELECT,
    MODE_STORY_INTRO,
    MODE_VS_SCREEN,
    MODE_BATTLE_PLAY,
    MODE_KO_DISPLAY,
    MODE_TIME_OVER,
    MODE_WIN_QUOTE,
    MODE_GAME_OVER,
    MODE_ENDING,
    MODE_TRAINING_SELECT,
    MODE_TRAINING,
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
    ACT_ATTACK,          // 技を出している（中身は Fighter::move）
    ACT_THROW,           // 投げの出かかり・空振り
    ACT_THROW_HOLD,      // つかんでいる
    ACT_HELD,            // つかまれている
    ACT_PARRY,
    ACT_HIT,
    ACT_AIR_HIT,
    ACT_DOWN,            // ダウン（起き上がるまで無敵）
    ACT_STUN,            // ピヨり
    ACT_DASH,
    ACT_DODGE,           // 回避ロール
    ACT_SLIP,            // 転倒（無防備）
    ACT_BOAR_ROLL,       // いのししマン：サイコロ回転中
    ACT_COUNTER_STANCE,  // たけし：当身構え
    ACT_COUNTER_STRIKE,  // たけし：当身反撃
    ACT_TAUNT,           // たけし：挑発
    ACT_STATE_COUNT
};

// 入力ビット（キーボード・通信で共通）
enum InputBit {
    IN_LEFT = 1, IN_RIGHT = 2, IN_UP = 4, IN_DOWN = 8,
    IN_J = 16, IN_K = 32, IN_L = 64, IN_SPACE = 128, IN_THROW = 256, IN_SUPER = 512
};

// 技の枠（入力 → 技）
enum MoveSlot {
    MS_5J, MS_2J, MS_JJ,        // 弱：立ち / しゃがみ / 空中
    MS_5K, MS_2K, MS_JK,        // 強
    MS_5L, MS_6L, MS_2L, MS_JL, // 必殺：L / 前+L / S+L / 空中L
    MS_SUPER, MS_JSUPER,        // 超必殺：I / 空中I
    MS_COUNT
};
static const char* SLOT_INPUT[MS_COUNT] = {
    "J", "S+J", "空中J", "K", "S+K", "空中K", "L", "前+L", "S+L", "空中L", "I", "空中I"
};

enum MoveFlag {
    MF_LOW = 1,          // 下段（しゃがみガードでのみ防げる）
    MF_OVERHEAD = 2,     // 中段（立ちガードでのみ防げる）
    MF_LAUNCH = 4,       // 打ち上げ（ジャンプ・空中技でキャンセル可）
    MF_SMASH = 8,        // 吹っ飛ばし（地上の相手も浮く）
    MF_UNBLOCK = 16,     // ガード不能
    MF_KNOCKDOWN = 32,   // ダウンを奪う
    MF_NOHITBOX = 64,    // 本体に攻撃判定なし（飛び道具・構えなど）
    MF_MULTI = 128,      // 多段ヒット
    MF_ARMOR = 256,      // 出始めにアーマー
    MF_INVUL = 512       // 出始め無敵
};

enum Behavior {
    BH_NONE,
    BH_SHOT,             // 飛び道具（キャラごとに中身を決める）
    BH_TRAP_DROP,
    BH_TRAP_THROW,
    BH_TRAP_AIR,
    BH_TRAP_SCATTER,
    BH_SMOKE,            // 煙玉バックステップ
    BH_BOAR_DICE,
    BH_BOAR_SUPER,
    BH_WATERMELON,
    BH_COUNTER,          // 当身（上中段）
    BH_COUNTER_LOW,      // 当身（下段）
    BH_COUNTER_SUPER,    // 全部返す当身
    BH_TAUNT,
    BH_TELEPORT_BEHIND,
    BH_TELEPORT_FRONT,
    BH_FLOAT,
    BH_DIVE,
    BH_SHOCKWAVE,
    BH_8WAY,
    BH_DRAGON,           // 大乗殿：低体力で強化される超必殺
    BH_KOKUSEN           // 大乗殿：空中J命中からの黒閃
};

enum Pose {
    PO_IDLE, PO_CROUCH, PO_JUMP, PO_LIGHT, PO_HEAVY, PO_LOW, PO_AIR, PO_SPECIAL, PO_UPPER,
    PO_GUARD, PO_CGUARD, PO_HIT, PO_DOWN, PO_STUN, PO_THROW, PO_HELD, PO_DASH, PO_ROLL,
    PO_SUPER, PO_WIN, PO_COUNT
};
static const char* POSE_FILES[PO_COUNT] = {
    "", "crouch", "jump", "light", "heavy", "low", "air", "special", "upper",
    "guard", "cguard", "hit", "down", "stun", "throw", "held", "dash", "roll", "super", "win"
};

// チュートリアルへ「プレイヤーが何をしたか」を知らせるイベント
enum TutEvent {
    TE_HIT,          // param = 当てた技のタグ
    TE_GUARD,        // param = 1 なら下段をしゃがみガード
    TE_PARRY,
    TE_DASH,         // param = 移動方向
    TE_DODGE,
    TE_JUMP,
    TE_DOUBLE_JUMP,
    TE_FAST_FALL,
    TE_BURST,
    TE_COMBO         // param = コンボ数
};
static void TutorialEvent(int ev, int param);

// コンボ記録・課題で使うタグ（技は 200 + MoveSlot）
enum ComboTag {
    TAG_SHOT = 100,
    TAG_TRAP,
    TAG_WALL,
    TAG_COUNTER,
    TAG_COUNTER_SUPER,
    TAG_PARRY,
    TAG_DICE6,
    TAG_THROW,
    TAG_MOVE = 200
};
#define TAGM(slot) (TAG_MOVE + (slot))

struct MoveDef {
    bool valid;
    const char* name;
    short startup, active, total;
    short damage, stun;
    float kx, ky;
    short w, h, offX, offY;
    int flags;
    float vx;        // 出した瞬間の前進速度
    float rise;      // 発生フレームで跳び上がる速度（昇天脚など）
    short behavior;
    short rank;      // 1:弱 2:強 3:必殺 4:超必殺（キャンセルの順番）
    short pose;
    short whiff;     // 空振り時の追加硬直
    short multi;     // 多段の間隔
    short armor;     // アーマーのフレーム数
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
    int tier;          // 0:初心者向け 1:中級者向け 2:玄人向け
};

static const int CHARA_COUNT = 9;
static CharacterProfile CHARA_PROFILES[CHARA_COUNT];
static MoveDef g_moves[CHARA_COUNT][MS_COUNT];

static const char* TIER_NAMES[3] = { "初心者向け ★", "中級者向け ★★", "玄人向け ★★★" };

// キャラ選択画面の並び順（5:望月ママはCPU専用）
static const int g_rosterOrder[] = { 0, 7, 1, 2, 3, 4, 8, 6 };
static const int ROSTER_COUNT = sizeof(g_rosterOrder) / sizeof(g_rosterOrder[0]);

// 画像ファイル名のもと
static const char* CHARA_IMAGE_BASE[CHARA_COUNT] = {
    "rikidou", "wanataro", "ananda", "inoshishi", "omae", "mama", "boss", "yoshida", "takeshi"
};

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

static const char* STORY_PROLOGUE[CHARA_COUNT][3] = {
    { "町内会の空手師範・大乗殿力也。",
      "「世界格闘チャンピオンシップ」の招待状が回覧板に挟まっていた。",
      "町内の平和と、公民館の雨漏り修理のため――いざ参る！" },
    { "罠太郎は、人をイラつかせることに人生を懸けた男。",
      "大会のルールブックに『罠禁止』とは書いていなかった。",
      "「勝つのはオレじゃない。相手が勝手に負けるのさ」" },
    { "ヨガの修行で手足が伸びるようになったアナンダ。",
      "伸びすぎた手足を『縮める方法』を知る者が大会にいるという。",
      "「伸びるのは得意。縮むのは…修行が足りない」" },
    { "夏の夜、スイカ畑を荒らす覆面男・いのししマン。",
      "優勝賞品が『一生分のスイカ』だと聞き、勢いで参戦した。",
      "「止まり方？ 知らん！ 全部サイコロが決める!!」" },
    { "元大横綱・メガトン大前。引退後は物干し竿の訪問販売員。",
      "「この竿の頑丈さを世界に見せつければ、売上倍増でごわす」",
      "空振りの隙など気にしない。当たれば勝ちでごわす。" },
    { "商店街の自警団長・望月ママ。",
      "アーケードの修理費を稼ぐため、パイプ椅子を手に立ち上がった。",
      "「タイムセールまでに終わらせるわよ！」" },
    { "数百年の眠りから覚めた武神・天輪太子。",
      "封印の影響で力の大半を失い、体は脆くなっている。",
      "「それでも――最強を名乗る者がいるなら、確かめずにはいられない」" },
    { "蹴り技一筋40年、町道場の吉田。",
      "「手は使わん。足だけで十分だ」が口癖。",
      "息子に『父さんの蹴りが世界一』と証明するため、大会に挑む。" },
    { "『ノーガード戦法』を研究する男・無防備たけし。",
      "自分からは殴らない。相手が痺れを切らすのを待つだけ。",
      "「さあ、打ってきなよ。……全部見えてるからさ」" }
};

static const char* STORY_ENDING[CHARA_COUNT][3] = {
    { "大会を制した力也は、賞金で公民館の屋根を直した。",
      "子どもたちの間で空手が大流行し、道場は満員に。",
      "「押忍！ 次は町内運動会で会おう！」" },
    { "優勝した罠太郎は、表彰式の会場にも罠を仕掛けていた。",
      "司会者が転び、来賓が転び、最後に自分も転んだ。",
      "「…イラッとしたぜ。これがオレの罠の味か」" },
    { "優勝したアナンダは、ついに『縮む』秘伝を授かった。",
      "しかし縮みすぎて、今度は5cmになってしまった。",
      "「…修行は、まだ続く」" },
    { "優勝賞品の『一生分のスイカ』が届いた。",
      "量が多すぎて、いのししマンは村中に配って回った。",
      "村人たちは彼を『スイカの神』と呼ぶようになった。" },
    { "決勝で振り回した物干し竿は『折れない竿』として大ヒット。",
      "大前の訪問販売は予約3年待ちになった。",
      "「竿は振ってナンボでごわす」" },
    { "望月ママは優勝賞金で商店街のアーケードを修理した。",
      "シャッター街は活気を取り戻し、特売日は大行列。",
      "「さあ、タイムセールに間に合ったわよ！」" },
    { "天輪太子は、封印された体のまま全ての強者を下した。",
      "「力を失っても、技は残る。それを確かめたかった」",
      "彼は再び眠りにつく。次の時代の強者を楽しみにしながら。" },
    { "吉田の蹴りは世界一と認められた。",
      "表彰台で息子が叫ぶ。「父さん、最強！」",
      "吉田は照れ隠しに、トロフィーを蹴り上げてキャッチした。" },
    { "たけしは一度も自分から攻めずに優勝した。",
      "観客は困惑し、対戦相手は全員イライラしていた。",
      "「だから言ったでしょ。全部見えてるって」" }
};

static const char* STORY_ENDING_SECRET[CHARA_COUNT] = {
    "黒閃の噂は海を越え、世界中から挑戦者が公民館を訪れるようになった。",
    "伝説の始祖すら苛立たせた男として、『罠の帝王』の名が刻まれた。",
    "天輪太子は笑った。「伸び縮みは心のままに」――アナンダは解脱した。",
    "サイコロは最後まで6を出し続けた。天運は猪突猛進する者に味方する。",
    "天輪太子もひと竿購入した。武神のお墨付きで、売上はさらに倍に。",
    "伝説の始祖にすら「おかん」と呼ばれ、商店街の守り神となった。",
    "かつての自分自身すら超えたその拳は、『武神そのもの』と讃えられた。",
    "伝説の始祖の神速すら蹴り落とした右脚は『伝説の右脚』と呼ばれる。",
    "武神の拳すら見切ったたけしに、天輪太子が弟子入りを申し出た。"
};

// キャラごとの特徴（コマンドリストに表示）
static const char* CMD_TRAITS[CHARA_COUNT][2] = {
    { "SPが溜まりやすい。空中J命中直後の 空中I で黒閃（最大火力）",
      "体力25%以下の I はドラゴンスープレックスに変化" },
    { "罠は1人4個まで。バナナ=スリップ / レゴ=激痛 / コーン=転倒",
      "罠を踏むとSP回収＆COMのイライラ度UP（ガードが雑になる）" },
    { "J・K系は先端ヒットで威力1.5倍。ただし伸ばした手足にもやられ判定",
      "空中L で浮遊（ゆっくり降りる）。前+L で相手の背後へワープ" },
    { "J は3連打まで繋がる。J→J→前+L→W→空中J→空中K が基本コンボ",
      "L はサイコロ突進。回転中にもう一度ボタンで目押し（高い目が出やすい）" },
    { "跳躍不可・歩きは遅い。ダッシュ(AA/DD)で間合いを詰める",
      "K・S+K・L・I は空振りすると大スキ。振るか、見切られるか" },
    { "CPU専用キャラ（トレーニングのダミーで選べる）", "" },
    { "封印解放版: 体力780と脆いが、全技が速く高火力",
      "前+L 瞬動・S+L 天昇脚は出始め無敵。読み合いで光る上級者向け" },
    { "蹴り技はどれもリーチが長い。迷ったら S+K 足払い",
      "前+L 飛び横蹴りは画面端で壁バウンド → 追撃のチャンス" },
    { "パリィの受付が全キャラの約3倍。成功後の一撃は2.5倍",
      "当身は大技ほど大きく返す。投げには無力なので注意" }
};

static const char* CMD_COMMON[] = {
    "A / D 移動　W ジャンプ　S しゃがみ（押している間）",
    "AA / DD ダッシュ（後ろはバックダッシュ・一瞬無敵）",
    "空中で W：2段ジャンプ　空中で S：急降下　空中で AA/DD：空中ダッシュ",
    "J 弱攻撃　K 強攻撃　L 必殺技（前+L / S+L で別の必殺技）",
    "S+J・S+K は下段（しゃがみガードでしか防げない）　空中攻撃は中段",
    "SPACE 長押し：ガード（S+SPACE でしゃがみガード）",
    "SPACE 短押し（当たる瞬間）：パリィ　ガード中に A/D：回避ロール",
    "左Shift 投げ（ガードを崩せる）　つかまれた直後に左Shift で投げ抜け",
    "I 超必殺技（SP100）　被弾中に SPACE：バースト（SP50）",
    "コンボ: 当たった瞬間に 弱→強→必殺→超必殺 の順で次の技に繋がる",
    "　弱は3回まで連打可。打ち上げ技が当たったら W で追いかけて空中コンボ",
    "カウンター: 技の出始めを潰すと COUNTER、硬直に当てると PUNISH COUNTER",
    "ESC ポーズ　TAB コマンドリスト",
    NULL
};

static const char* CMD_TRAINING[] = {
    "1-7（テンキー可）　ダミーの行動を切り替え",
    "  1:停止 2:立ちガード 3:しゃがみガード 4:オートガード(上下自動)",
    "  5:ランダムジャンプ 6:ランダムガード 7:CPU行動(反撃あり)",
    "F1 当たり判定　F2 フレーム表示　F3 入力履歴",
    "F4 SP無限　F5 位置リセット　F7 画面端に配置　F6 次のコンボ課題",
    "判定表示: 青枠=やられ判定 / 赤=攻撃判定 / 黄枠=投げ判定",
    "フレーム: 発生=攻撃が出るまで / 持続=判定が出ている間 / 硬直=戻りの隙",
    "ガード時の不利が相手の最速技より大きいと「確定反撃あり」",
    "ESC ポーズメニュー",
    NULL
};

// ----------------------------------------------------
// コンボ課題（トレーニング）
// ----------------------------------------------------
struct ComboChallenge {
    const char* desc;
    int tags[4];
    int tagCount;
    const char* hint;
};

static const ComboChallenge CHALLENGES[CHARA_COUNT][3] = {
    { { "J → J → K", { TAGM(MS_5J), TAGM(MS_5J), TAGM(MS_5K) }, 3, "当たった瞬間に次のボタン。弱は2回まで重ねられる" },
      { "J → K → L（昇天脚）", { TAGM(MS_5J), TAGM(MS_5K), TAGM(MS_5L) }, 3, "弱→強→必殺の順で繋がる" },
      { "空中J → 空中I で黒閃", { TAGM(MS_JJ), TAGM(MS_JSUPER) }, 2, "F4 でSP無限にすると練習しやすい" } },
    { { "前+L 罠投げを直撃", { TAG_SHOT }, 1, "相手から3キャラ分ほど離れて 前+L" },
      { "罠を踏ませてから J", { TAG_TRAP, TAGM(MS_5J) }, 2, "ダミーを 7:CPU行動 にすると罠に向かって歩いてくる" },
      { "S+J → S+K（下段2連）", { TAGM(MS_2J), TAGM(MS_2K) }, 2, "しゃがんだまま J→K" } },
    { { "J → J → K", { TAGM(MS_5J), TAGM(MS_5J), TAGM(MS_5K) }, 3, "先端を当てると威力1.5倍" },
      { "K → L（ヨガ突進）", { TAGM(MS_5K), TAGM(MS_5L) }, 2, "強が当たった瞬間に L" },
      { "L → 空中J（打ち上げ追撃）", { TAGM(MS_5L), TAGM(MS_JJ) }, 2, "L が当たったら W で追いかけて J" } },
    { { "J → J → J（牙突き3連）", { TAGM(MS_5J), TAGM(MS_5J), TAGM(MS_5J) }, 3, "J をリズムよく3回" },
      { "J → 前+L（ぶちかまし）", { TAGM(MS_5J), TAGM(MS_6L) }, 2, "J が当たった瞬間に 前+L" },
      { "出目6の突進を当てる", { TAG_DICE6 }, 1, "L の回転中に緑ゾーンで目押し" } },
    { { "K 横薙ぎを当てる", { TAGM(MS_5K) }, 1, "画面半分まで届く。外すと大スキ" },
      { "横薙ぎで壁バウンド", { TAGM(MS_5K), TAG_WALL }, 2, "F7 で画面端に配置してから K" },
      { "壁バウンド → L 振り下ろし", { TAGM(MS_5K), TAG_WALL, TAGM(MS_5L) }, 3, "跳ね返ってきたところに L" } },
    { { "J を当てる", { TAGM(MS_5J) }, 1, "" },
      { "J → K", { TAGM(MS_5J), TAGM(MS_5K) }, 2, "" },
      { "L で打ち上げる", { TAGM(MS_5L) }, 1, "" } },
    { { "J → J → K", { TAGM(MS_5J), TAGM(MS_5J), TAGM(MS_5K) }, 3, "" },
      { "K → 前+L（瞬動）", { TAGM(MS_5K), TAGM(MS_6L) }, 2, "強が当たった瞬間に 前+L" },
      { "L → 空中J（打ち上げ追撃）", { TAGM(MS_5L), TAGM(MS_JJ) }, 2, "L が当たったら W で追いかける" } },
    { { "J → K", { TAGM(MS_5J), TAGM(MS_5K) }, 2, "ローキック → 上段回し蹴り" },
      { "J → K → 前+L", { TAGM(MS_5J), TAGM(MS_5K), TAGM(MS_6L) }, 3, "最後の飛び横蹴りは壁バウンドを狙える" },
      { "L サマーソルト → 空中J", { TAGM(MS_5L), TAGM(MS_JJ) }, 2, "一緒に跳び上がってかかと落とし" } },
    { { "パリィ成功 → J", { TAG_PARRY, TAGM(MS_5J) }, 2, "ダミーを 7:CPU行動 に。攻撃に合わせて SPACE を短く" },
      { "当身(L)を成功させる", { TAG_COUNTER }, 1, "ダミー 7:CPU行動 の攻撃を L で待つ" },
      { "極当身(I)を成功させる", { TAG_COUNTER_SUPER }, 1, "F4 でSP無限に" } }
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

// CPUの立ち回りの好み（キャラごと）
struct AiPref {
    float prefDist;   // 保ちたい間合い
    int farSlot;      // 遠距離で振る技（-1 で無し）
    int midSlot;      // 中距離で振る技
    int antiAir;      // 対空（-1 で無し）
    bool keepAway;    // 距離を取りたがる
};
static const AiPref AI_PREFS[CHARA_COUNT] = {
    { 80.0f,  MS_6L, MS_5K, MS_5L, false },
    { 210.0f, MS_6L, MS_5L, -1,    true },
    { 130.0f, MS_5K, MS_2K, MS_2L, false },
    { 120.0f, MS_5K, MS_2K, MS_6L, false },
    { 190.0f, MS_5K, MS_2J, MS_5L, false },
    { 80.0f,  MS_6L, MS_5K, MS_5L, false },
    { 90.0f,  MS_6L, MS_5K, MS_2L, false },
    { 95.0f,  MS_6L, MS_5K, MS_5L, false },
    { 70.0f,  -1,    MS_5K, -1,    false }
};

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
    bool hazardWarning;
    int hazardCountdown;
    float itemX, itemY;
    bool itemActive;
};
static StageGimmick g_gimmick;
static bool g_gimmickEnabled = true;
static int g_currentStageIdx = 0;

// 感電床：7秒周期。3秒の予告点滅 → 1秒通電
static const int HAZARD_PERIOD = 420;
static const int HAZARD_WARN_START = 180;
static const int HAZARD_ACTIVE_START = 360;
static const float HAZARD_SAFE_HALF = 70.0f;

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
    int loopCount;
    bool needCharaSelect;
    unsigned char clearRank[MAX_CHARA_SLOTS];
    int clearCount[MAX_CHARA_SLOTS];
    bool noContinueClear[MAX_CHARA_SLOTS];
    int difficulty;                              // 0:かんたん 1:ふつう 2:むずかしい
};

// 20261103 版（difficulty が無い）
struct PrevSaveSlot {
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
    int loopCount;
    bool needCharaSelect;
    unsigned char clearRank[MAX_CHARA_SLOTS];
    int clearCount[MAX_CHARA_SLOTS];
    bool noContinueClear[MAX_CHARA_SLOTS];
};

// 〜20261102 版
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
static const char* DIFFICULTY_NAMES[3] = { "かんたん", "ふつう", "むずかしい" };

struct Projectile {
    bool active;
    bool isFromMe;
    float x, y;
    float vx, vy;
    float gravity;
    int damage;
    unsigned int color;
    int life;
    int radius;
    int projType;   // 2:気弾 4:罠（着地で設置） 5:スイカ 6:衝撃波 7:小さな気弾
    int subType;
    bool low;       // 下段扱い
    float angle;
};
static const int MAX_SHOTS = 24;
static Projectile g_shots[MAX_SHOTS];

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
    int move;              // ACT_ATTACK 中の技（MoveSlot）
    int variant;           // 技の派生（黒閃・ドラゴンなど）
    int stateTimer;
    int animTimer;
    int parryWindow;
    int comboCount;
    int comboDisplayTimer;
    int hitStop;
    bool hasHit;           // 当たった（ガードされたも含む）
    bool connected;        // ガードされずに当たった
    bool isCounter;        // パリィ成功後の一撃ボーナス
    int armorFrames;
    int chainCount;        // 弱の連打数
    bool launchCancelUsed;
    float riseVy;          // 発生フレームで跳ぶ速度

    int spGauge;
    int invulFrames;
    int moveDir;
    bool airDashUsed;
    bool doubleJumpUsed;
    bool wallBounceReady;
    bool wallBounceUsed;
    int comboTaken;
    int burstCooldown;
    int stanceLevel;       // たけし当身 1:上中段 2:全部 3:下段
    bool boarSuper;
    int diceFace;
    int diceShowTimer;
    int boarTiming;        // 0:未入力 1:目押し成功 2:ズレ
    bool whiffChecked;
    int trapCycle;
    int guardStun;
    bool crouching;
    int floatTimer;        // アナンダの浮遊

    int throwDir;          // 1:前に投げる -1:後ろに投げる
    int techWindow;        // 投げ抜け受付

    int bufBtn;            // 先行入力
    int bufTimer;
    bool bufDown, bufFwd;

    int labelTimer;        // 技名表示
    int labelSlot;         // 表示する技（-1:投げ -2:裏投げ）
};

static Fighter g_me;
static Fighter g_other;

enum BufferButton { BTN_NONE, BTN_J, BTN_K, BTN_L, BTN_SUPER, BTN_THROW };

// 試合後の統計（0 = 自分 / 1 = 相手）
struct MatchStats {
    int moves;
    int hits;
    int taken;
    int guards;
    int parries;
    int maxCombo;
    int damage;
};
static MatchStats g_stats[2];
static int g_moveUse[2][MS_COUNT + 1];

struct MoveInfo {
    bool valid;
    int chara;
    int slot;
    int startup, active, recovery, total;
};
static MoveInfo g_moveInfo[2];

struct AdvantageInfo {
    bool valid;
    bool byDummy;
    bool guarded;
    bool launch;
    bool punishable;
    int adv;
};
static AdvantageInfo g_adv;

struct ControlState {
    unsigned short held;
    unsigned short pressed;
    unsigned short dbl;
    int lastTap[4];
};
static ControlState g_ctrl[2];

// 演出
static int g_superCutinTimer = 0;
static const char* g_superCutinName = "";
static char g_netCutinText[96] = "";
static bool g_superIsUltimate = false;
static bool g_cutinIsCounter = false;
static int g_kokusenStrobe = 0;
static bool g_isKokusenKoFinish = false;
static int g_flashTimer = 0;
static int g_counterLabelTimer[2] = { 0, 0 };
static int g_counterLabelType[2] = { 0, 0 };   // 1:COUNTER 2:PUNISH COUNTER
static int g_koWinner = 0;                     // KO演出中の勝者 1:自分 2:相手

// CPUのイライラ度
static int g_cpuTilt = 0;
static int g_cpuTiltTimer = 0;
static int g_cpuThinkTimer = 0;

// カメラ（2人の距離に合わせてズーム）
static int g_worldScreen = -1;
static float g_camZoom = 1.0f;
static float g_camX = 0.0f;
static float g_camY = 0.0f;

// ポーズメニュー
static bool g_paused = false;
static int g_pauseCursor = 0;

// キャラ画像
static int g_charaGraph[CHARA_COUNT];
static int g_poseGraph[CHARA_COUNT][PO_COUNT];
static int g_faceGraph[CHARA_COUNT];

// ----------------------------------------------------
// 通信対戦
// ----------------------------------------------------
enum PacketType {
    PKT_HELLO = 1,
    PKT_SETUP,
    PKT_CHARA,
    PKT_INPUT,
    PKT_SNAPSHOT
};

enum NetPhase { NP_READY, NP_FIGHT, NP_ROUND_END, NP_MATCH_END };

#pragma pack(push, 1)
struct NetHeader {
    unsigned int magic;
    unsigned char type;
    unsigned short len;
};
struct NetSetup {
    unsigned char stage;
    unsigned char roundsToWin;
    unsigned char timeLimit;
    unsigned char gimmickOn;
};
struct NetCharaMsg {
    unsigned char chara;
};
struct NetInputMsg {
    unsigned int seq;
    unsigned short bits;
};
struct NetFighter {
    float x, y, vx, vy;
    short hp, maxHp;
    unsigned char state, move, variant;
    signed char dir;
    unsigned char chara;
    unsigned char flags;
    short animTimer, stateTimer;
    unsigned char sp, diceFace, diceShowTimer, stanceLevel, comboCount, comboDisplayTimer;
    signed char moveDir, labelSlot;
    unsigned char boarTiming, guardStun, labelTimer;
};
struct NetShot {
    float x, y;
    unsigned int color;
    unsigned char type, sub, radius, fromA;
};
struct NetTrap {
    float x;
    unsigned char type, ownerA;
    short life, armDelay;
};
struct NetFxEvent {
    short x, y;
    unsigned int color;
    unsigned char count, kind, len;
};
struct NetPopupEvent {
    short x, y;
    unsigned int color;
    char text[48];
};
struct NetStats {
    short moves, hits, taken, guards, parries, maxCombo;
    int damage;
};
static const int NET_MAX_FX = 12;
static const int NET_MAX_POPUPS = 6;
struct NetSnapshot {
    unsigned int frame;
    NetFighter a, b;
    unsigned char shotCount;
    NetShot shots[MAX_SHOTS];
    unsigned char trapCount;
    NetTrap traps[MAX_TRAPS];
    float gx, gFallAngle, itemX;
    short ghp, gmaxHp;
    unsigned char gFlags;
    signed char gDir;
    int gStageTimer;
    signed char roundTime;
    unsigned char readyTimer, phase, phaseTimer, roundsA, roundsB, roundNo, winner;
    unsigned char cutinTimer, cutinFlags;
    char cutinText[96];
    unsigned char shake, flash, strobe, parryNotice, kokusen;
    unsigned char counterTimerA, counterTypeA, counterTimerB, counterTypeB;
    NetStats statsA, statsB;
    unsigned char fxCount;
    NetFxEvent fx[NET_MAX_FX];
    unsigned char popupCount;
    NetPopupEvent popups[NET_MAX_POPUPS];
};
#pragma pack(pop)

static SOCKET g_listen = INVALID_SOCKET;
static SOCKET g_peer = INVALID_SOCKET;
static bool g_is_host = false;
static bool g_is_net_battle = false;
static unsigned int g_sequence = 0;
static char g_rx[65536];
static int g_rx_used = 0;
static char g_tx[65536];
static int g_tx_used = 0;

static NetSetup g_netRules = { 0, 2, 99, 1 };
static int g_netRuleRow = 0;
static int g_myNetChara = 0;
static int g_enemyNetChara = 1;
static int g_netCharaCursor = 0;
static bool g_myNetReady = false;
static bool g_enemyNetReady = false;
static char g_net_status[256] = "";
static int g_net_room_code = 0;
static char g_client_ip_input[64] = "127.0.0.1";

static NetPhase g_netPhase = NP_READY;
static int g_netPhaseTimer = 0;
static int g_netRounds[2] = { 0, 0 };
static int g_netRoundNo = 1;
static int g_netWinner = 0;
static bool g_netGotSnapshot = false;
static unsigned short g_remoteQueue[16];
static int g_remoteQueueCount = 0;
static unsigned short g_remoteBits = 0;
static bool g_netRecordFx = false;
static NetFxEvent g_netFxQueue[NET_MAX_FX];
static int g_netFxCount = 0;
static NetPopupEvent g_netPopupQueue[NET_MAX_POPUPS];
static int g_netPopupCount = 0;
static unsigned int g_netFrame = 0;

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

static bool g_endingSecret = false;
static int g_endingRank = 1;
static int g_endingUnlockChara = -1;
static int g_unlockBannerTimer = 0;
static int g_unlockBannerChara = -1;
static int g_lockedMsgTimer = 0;

static char g_keyNow[256];
static char g_keyPrev[256];

static bool g_cmdListOpen = false;
static int g_cmdListChara = 0;
static int g_cmdListPage = 0;

// ----------------------------------------------------
// トレーニング
// ----------------------------------------------------
enum DummyMode {
    DUMMY_STOP,
    DUMMY_STAND_GUARD,
    DUMMY_CROUCH_GUARD,
    DUMMY_AUTO_GUARD,
    DUMMY_RANDOM_JUMP,
    DUMMY_RANDOM_GUARD,
    DUMMY_CPU,
    DUMMY_MODE_COUNT
};
static const char* DUMMY_MODE_NAMES[DUMMY_MODE_COUNT] = {
    "停止", "立ちガード", "しゃがみガード", "オートガード", "ランダムジャンプ", "ランダムガード", "CPU行動"
};

static int g_trainMyChara = 0;
static int g_trainDummyChara = 0;
static int g_trainStage = 0;
static int g_trainRow = 0;
static DummyMode g_dummyMode = DUMMY_STOP;
static bool g_showHitbox = true;
static bool g_showFrames = true;
static bool g_showInput = true;
static bool g_infiniteSp = false;
static int g_challengeIdx = 0;
static bool g_challengeDone[3] = { false, false, false };
static bool g_challengeHitThisCombo = false;
static int g_comboLog[32];
static int g_comboLogCount = 0;
static int g_comboDmg = 0;
static int g_comboHits = 0;
static int g_lastComboDmg = 0;
static int g_lastComboHits = 0;
static int g_maxComboDmg = 0;
static int g_maxComboHits = 0;
static int g_dummyNeutralFrames = 0;
static int g_meNeutralFrames = 0;

struct InputHistEntry {
    unsigned short bits;
    int frames;
};
static const int INPUT_HIST_MAX = 16;
static InputHistEntry g_inputHist[INPUT_HIST_MAX];
static int g_inputHistCount = 0;
