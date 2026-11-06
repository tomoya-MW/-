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
static const int VIS_OFF_Y = 3;     // 当たり判定の論理座標 → 画面上のキャラ絵の中心へのずれ
static const int FLOOR_DRAW_OFF = 25; // f.y（論理の足元）から画面の床の線までの距離

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
    ACT_BOAR_CHARGE,     // いのししマン：突進
    ACT_COUNTER_STANCE,  // たけし：当身構え
    ACT_COUNTER_STRIKE,  // たけし：当身反撃
    ACT_TAUNT,           // たけし：挑発
    ACT_STATE_COUNT
};

// 入力ビット（キーボード・通信で共通）
enum InputBit {
    IN_LEFT = 1, IN_RIGHT = 2, IN_UP = 4, IN_DOWN = 8,
    IN_J = 16, IN_K = 32, IN_L = 64, IN_SPACE = 128, IN_DODGE = 256
};

// チュートリアルへ「プレイヤーが何をしたか」を知らせるイベント
enum TutEvent {
    TE_HIT,          // param = 当てた技の状態 or コンボタグ
    TE_GUARD,
    TE_PARRY,
    TE_DASH,         // param = 移動方向
    TE_DODGE,
    TE_JUMP,
    TE_DOUBLE_JUMP,
    TE_FAST_FALL,
    TE_BURST
};
static void TutorialEvent(int ev, int param);

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

// ----------------------------------------------------
// キャラ別ストーリー
// ----------------------------------------------------
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

// ----------------------------------------------------
// コマンドリスト（TABで表示）
// ----------------------------------------------------
static const int CMD_LINES = 9;
static const char* CMD_LIST[CHARA_COUNT][CMD_LINES] = {
    { "J / J→J　　正拳突き → 追い突き（ヒット時のみ2段目）",
      "K　　　　　気弾（中距離けん制）",
      "L　　　　　昇天脚（相手を打ち上げ、自分も跳ぶ）",
      "空中J　　　空中突き / 空中L　急降下蹴り",
      "J+K　　　　ローリングソバット（体力25%以下でドラゴンスープレックス）",
      "空中J+K　　天翔・金剛螺旋脚",
      "空中J中にJ+K　黒閃・真極点昇天破（最大火力・KOで特殊演出）",
      "特性: コンボでSPが溜まりやすい",
      NULL },
    { "J / J→J　　ぺちっと叩く（近接は激弱）",
      "K　　　　　足元に罠を落として後ろへ逃げる",
      "L　　　　　罠を放り投げる（直撃で小ダメ・外れても着地点に設置）",
      "空中L　　　真下に罠を落とす",
      "J+K　　　　大特価!! 罠バラまきセール（罠を5個ばらまく）",
      "罠: バナナ=スリップ / レゴ=激痛よろけ / コーン=転倒",
      "罠は1人4個まで。踏むたびにSP回収＆COMのイライラ度UP",
      "イライラしたCOMはガードが雑になり、大技を振りがちになる",
      NULL },
    { "J / J→J　　伸びる突き（先端ヒットで1.5倍）",
      "K　　　　　伸びる足払い（先端ヒットで1.5倍）",
      "L　　　　　ヨガ突進（打ち上げ）",
      "空中J / 空中L　伸びる蹴り下ろし",
      "J+K　　　　ブラフマン・クンダリニー（8方向に気弾）",
      "注意: 手足を伸ばしている間は、手足にもやられ判定がある",
      NULL, NULL, NULL },
    { "J / K / L　どれでも突進！ 押すとサイコロが回る",
      "回転中にもう一度ボタン＝目押し（緑ゾーンで成功率UP）",
      "出目1: 小転倒　2: つまずき（どちらも短い隙）",
      "出目3: 様子見突進（ガード可） 4: 全力突進（ガード可）",
      "出目5: ガード不能突進　6: 超高速ガード不能＋強烈な吹っ飛び",
      "J+K　　　　確変!! 出目6確定の突進",
      "突進はどれも吹っ飛ばしが強く、画面端なら壁バウンドする",
      NULL, NULL },
    { "J / J→J　　竿の石突き（近距離）",
      "K　　　　　横薙ぎ（画面半分まで届く・発生遅め）",
      "L　　　　　振り下ろし（打ち上げ）",
      "J+K　　　　電柱フルスイング（画面全体・ジャンプでかわせる）",
      "空振りすると約1秒の大スキ。振るか、見切られるか！",
      "跳躍不可。歩きは遅いがダッシュ(AA/DD)で間合いを詰める",
      NULL, NULL, NULL },
    { "J　　　　　パイプ椅子ではたく",
      "L　　　　　椅子振り上げ（打ち上げ）",
      "※CPU専用キャラ（トレーニングのダミーで選べます）",
      NULL, NULL, NULL, NULL, NULL, NULL },
    { "J / J→J　　神速拳",
      "K　　　　　瞬動（無敵で突っ込む）",
      "L　　　　　天輪掌（打ち上げ）",
      "J+K　　　　天輪・神速百烈拳（相手の目の前に瞬間移動して連打）",
      "封印解放版: 体力780と脆いが、全技が速く高火力",
      NULL, NULL, NULL, NULL },
    { "J / J→J　　ローキック → 上段回し蹴り",
      "K　　　　　飛び横蹴り（吹っ飛ばしが強く壁バウンドを誘発）",
      "L　　　　　サマーソルト（打ち上げ・自分も跳ぶ）",
      "空中J　　　かかと落とし",
      "空中L　　　旋風脚（空中で横に吹っ飛ばす）",
      "J+K　　　　吉田式・百烈脚（多段ヒット）",
      NULL, NULL, NULL },
    { "J / J→J　　ちょい突き（リーチ・火力とも全キャラ最低）",
      "K　　　　　当身構え（構え中に殴られると即反撃。大技ほど返しも大）",
      "L　　　　　挑発（成功でSP+25、COMをイライラさせる）",
      "J+K　　　　奥義・無防備の極み（約1.5秒、飛び道具も含め全部返す）",
      "SPACEタイミング押しのパリィ受付が全キャラの約3倍",
      "パリィ成功後の一撃は2.5倍。相手が大技を振るのを待とう",
      "※投げは当身で返せない",
      NULL, NULL }
};

static const char* CMD_COMMON[] = {
    "A / D　移動　　AA / DD　ダッシュ（後ろ方向はバックダッシュ・一瞬無敵）",
    "W　ジャンプ　　空中でW　2段ジャンプ　　空中でSS　急降下",
    "空中でAA / DD　空中ダッシュ（1回）",
    "SPACE長押し　ガード（削りダメージ・ガード硬直あり）",
    "SPACEをタイミングよく単押し　パリィ（成功で相手がピヨる・SP+20）",
    "  ※パリィの受付が切れても押しっぱなしならそのままガードに移行",
    "I / 左Shift　回避（無敵ロール。A/Dで向きを指定）",
    "前 + J　投げ（ガードを崩せる）　　J + K　超必殺技（SP100）",
    "被弾中にSPACE　バースト＝コンボ抜け（SP50・再使用4秒）",
    "壁バウンド: 画面端で強い吹っ飛ばし技を当てると跳ね返る（追撃1回・火力70%）",
    "TAB　コマンドリストを開く/閉じる",
    NULL
};

static const char* CMD_TRAINING[] = {
    "1-6（テンキー可）　ダミーの行動を切り替え",
    "  1:停止  2:立ちガード  3:しゃがみガード",
    "  4:ランダムジャンプ  5:ランダムガード  6:CPU行動（反撃あり）",
    "F1 当たり判定表示　F2 フレーム表示　F3 入力履歴表示",
    "F4 SP・スタミナ無限　F5 位置リセット　F7 画面端に配置",
    "F6 次のコンボ課題",
    "判定表示: 青枠=やられ判定 / 赤=攻撃判定 / 黄枠=投げ判定",
    "フレーム: 発生=攻撃が出るまで / 持続=判定が出ている間 / 硬直=戻りの隙",
    "ガード時の不利が相手の最速技より大きいと「確定反撃あり」",
    "ESC　トレーニング設定に戻る",
    NULL
};

// ----------------------------------------------------
// コンボ課題（トレーニング）
// ----------------------------------------------------
enum ComboTag {
    TAG_SHOT = 100,
    TAG_TRAP,
    TAG_WALL,
    TAG_COUNTER,
    TAG_COUNTER_SUPER,
    TAG_PARRY,
    TAG_DICE6
};

struct ComboChallenge {
    const char* desc;
    int tags[4];
    int tagCount;
    const char* hint;
};

static const ComboChallenge CHALLENGES[CHARA_COUNT][3] = {
    { { "J → J の2連撃", { ACT_LIGHT1, ACT_LIGHT2 }, 2, "J がヒットした直後にもう一度 J" },
      { "L で打ち上げ → 空中J", { ACT_SKILL_ATTACK, ACT_AIR_LIGHT }, 2, "L で一緒に跳び上がり、空中で J" },
      { "空中J → J+K で黒閃", { ACT_AIR_LIGHT, ACT_SUPER_ENHANCED }, 2, "F4 でSP無限にすると練習しやすい" } },
    { { "罠投げ(L)を直撃させる", { TAG_SHOT }, 1, "相手から3キャラ分ほど離れて L" },
      { "罠を踏ませてから J", { TAG_TRAP, ACT_LIGHT1 }, 2, "ダミーを 6:CPU行動 にすると罠に向かって歩いてくる" },
      { "罠バラまき(J+K)で2ヒット", { TAG_SHOT, TAG_SHOT }, 2, "近めの間合いで J+K" } },
    { { "J → J の2連撃", { ACT_LIGHT1, ACT_LIGHT2 }, 2, "先端を当てると威力1.5倍" },
      { "L で打ち上げ → 空中J", { ACT_SKILL_ATTACK, ACT_AIR_LIGHT }, 2, "L の後すぐジャンプして J" },
      { "J+K の気弾で2ヒット", { TAG_SHOT, TAG_SHOT }, 2, "密着で J+K" } },
    { { "突進を当てる", { ACT_BOAR_CHARGE }, 1, "J/K/L で突進。回転中にもう一度押すと目押し" },
      { "突進で壁バウンド", { ACT_BOAR_CHARGE, TAG_WALL }, 2, "F7 で画面端に配置してから突進" },
      { "出目6の突進を当てる", { TAG_DICE6 }, 1, "目押し成功で高い目が出やすい" } },
    { { "K 横薙ぎを当てる", { ACT_MID_ATTACK }, 1, "画面半分まで届く。外すと1秒の大スキ" },
      { "横薙ぎで壁バウンド", { ACT_MID_ATTACK, TAG_WALL }, 2, "F7 で画面端に配置してから K" },
      { "壁バウンド → L 振り下ろし", { ACT_MID_ATTACK, TAG_WALL, ACT_SKILL_ATTACK }, 3, "跳ね返ってきたところに L" } },
    { { "J を当てる", { ACT_LIGHT1 }, 1, "" },
      { "J → J の2連撃", { ACT_LIGHT1, ACT_LIGHT2 }, 2, "" },
      { "L で打ち上げる", { ACT_SKILL_ATTACK }, 1, "" } },
    { { "J → J の2連撃", { ACT_LIGHT1, ACT_LIGHT2 }, 2, "" },
      { "L で打ち上げ → 空中J", { ACT_SKILL_ATTACK, ACT_AIR_LIGHT }, 2, "L の硬直が解けたらすぐジャンプ" },
      { "神速百烈拳で3ヒット", { ACT_SUPER_ATTACK, ACT_SUPER_ATTACK, ACT_SUPER_ATTACK }, 3, "F4 でSP無限に" } },
    { { "J → J の2連撃", { ACT_LIGHT1, ACT_LIGHT2 }, 2, "ローキック → 回し蹴り" },
      { "K 飛び横蹴りで壁バウンド", { ACT_MID_ATTACK, TAG_WALL }, 2, "F7 で画面端に配置" },
      { "L サマーソルト → 空中J", { ACT_SKILL_ATTACK, ACT_AIR_LIGHT }, 2, "一緒に跳んでかかと落とし" } },
    { { "パリィ成功 → J", { TAG_PARRY, ACT_LIGHT1 }, 2, "ダミーを 6:CPU行動 に。攻撃に合わせて SPACE" },
      { "当身(K)を成功させる", { TAG_COUNTER }, 1, "ダミー 6:CPU行動 の攻撃を K で待つ" },
      { "極当身(J+K)を成功させる", { TAG_COUNTER_SUPER }, 1, "F4 でSP無限に" } }
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
static const CpuBossData TRAINING_BOSS = { "ダミー", "", 0, 5, 3, 0, "" };
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
static const float HAZARD_SAFE_HALF = 70.0f;   // 感電コンテナ周辺の安全地帯（ゴムマット）

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
    int boarTiming;        // 0:未入力 1:目押し成功 2:ズレ
    bool whiffChecked;     // 大前の空振り判定済み
    int trapCycle;
    int guardStun;         // ガード硬直
    bool crouching;        // しゃがみ（トレーニングのダミー用）
    float pendingRiseVy;   // 昇天脚など：発生フレームを迎えてから跳び上がる
};

static Fighter g_me;
static Fighter g_other;

// 立ち状態（ACT_IDLE）のキャラ画像。読み込めなければ -1 で、図形描画に切り替える
static int g_charaGraph[CHARA_COUNT];
static const char* CHARA_IMAGE_FILES[CHARA_COUNT] = {
    "rikidou.bmp", "wanataro.bmp", "ananda.bmp", "inoshishi.bmp", "omae.bmp",
    "mama.bmp", "boss.bmp", "yoshida.bmp", "takeshi.bmp"
};

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
static int g_moveUse[2][ACT_STATE_COUNT];

// フレーム表示用
struct MoveInfo {
    bool valid;
    int chara;
    ActionState state;
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
    int chara;
    ActionState state;
};
static AdvantageInfo g_adv;

// 入力のエッジ・2度押し判定
struct ControlState {
    unsigned short held;
    unsigned short pressed;
    unsigned short dbl;
    int lastTap[4];
};
static ControlState g_ctrl[2];

// 超必殺技・黒閃・パチンコ演出用変数
static int g_superCutinTimer = 0;
static const char* g_superCutinName = "";
static char g_netCutinText[96] = "";
static bool g_superIsUltimate = false;
static bool g_cutinIsCounter = false;    // 当身・極パリィ用のカットイン
static int g_kokusenStrobe = 0;          // 黒閃・空間反転ストロボタイマー
static bool g_isKokusenKoFinish = false;  // 最強技でKOしたかどうかのフラグ
static int g_rainbowCycle = 0;
static int g_flashTimer = 0;             // 当身成功時の白フラッシュ

// CPUのイライラ度（罠や挑発で上がり、ガードが雑になって大技を振りがちになる）
static int g_cpuTilt = 0;
static int g_cpuTiltTimer = 0;

// ----------------------------------------------------
// 通信対戦（ホストが全て計算し、クライアントは入力送信と表示のみ）
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
    unsigned char timeLimit;    // 0 = 無制限
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
    unsigned char state;
    signed char dir;
    unsigned char chara;
    unsigned char flags;
    short animTimer, stateTimer;
    unsigned char sp, stamina, diceFace, diceShowTimer, stanceLevel, comboCount, comboDisplayTimer;
    signed char moveDir;
    unsigned char boarTiming, guardStun;
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
static bool g_netRulesReceived = false;
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
static int g_netRounds[2] = { 0, 0 };   // [0]=ホスト(A) [1]=クライアント(B)
static int g_netRoundNo = 1;
static int g_netWinner = 0;             // 1:A 2:B 3:引き分け
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

// 周回・クリア演出
static bool g_endingSecret = false;
static int g_endingRank = 1;
static int g_endingUnlockChara = -1;
static int g_unlockBannerTimer = 0;
static int g_unlockBannerChara = -1;
static int g_lockedMsgTimer = 0;

// キーボード
static char g_keyNow[256];
static char g_keyPrev[256];

// コマンドリスト
static bool g_cmdListOpen = false;
static int g_cmdListChara = 0;
static int g_cmdListPage = 0;

// ----------------------------------------------------
// トレーニングモード
// ----------------------------------------------------
enum DummyMode {
    DUMMY_STOP,
    DUMMY_STAND_GUARD,
    DUMMY_CROUCH_GUARD,
    DUMMY_RANDOM_JUMP,
    DUMMY_RANDOM_GUARD,
    DUMMY_CPU,
    DUMMY_MODE_COUNT
};
static const char* DUMMY_MODE_NAMES[DUMMY_MODE_COUNT] = {
    "停止", "立ちガード", "しゃがみガード", "ランダムジャンプ", "ランダムガード", "CPU行動"
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

// ----------------------------------------------------
// 共通ユーティリティ
// ----------------------------------------------------
static int ClampI(int v, int lo, int hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }

static bool IsHitState(ActionState s) { return s == ACT_HIT || s == ACT_AIR_HIT; }

static bool IsAttackState(ActionState s) {
    return (s >= ACT_THROW && s <= ACT_SUPER_ENHANCED) || s == ACT_BOAR_CHARGE;
}

static int FighterIndex(const Fighter& f) { return (&f == &g_me) ? 0 : 1; }

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
    if (KeyDown(KEY_INPUT_I) || KeyDown(KEY_INPUT_LSHIFT)) b |= IN_DODGE;
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
// 通信（低レベル）
// ----------------------------------------------------
static void SetNonBlocking(SOCKET s) { u_long on = 1; ioctlsocket(s, FIONBIO, &on); }
static void CloseSocket(SOCKET& s) { if (s != INVALID_SOCKET) { closesocket(s); s = INVALID_SOCKET; } }

static void StopNetwork(const char* message) {
    CloseSocket(g_peer);
    CloseSocket(g_listen);
    g_tx_used = 0;
    g_rx_used = 0;
    g_mode = MODE_NET_ERROR;
    sprintf_s(g_net_status, "%s (Escでメニューへ戻る)", message);
}

static void FlushSend() {
    if (g_peer == INVALID_SOCKET || g_tx_used == 0) return;
    int n = send(g_peer, g_tx, g_tx_used, 0);
    if (n == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) StopNetwork("送信エラー");
        return;
    }
    memmove(g_tx, g_tx + n, g_tx_used - n);
    g_tx_used -= n;
}

static void SendPacket(unsigned char type, const void* data, int len) {
    if (g_peer == INVALID_SOCKET) return;
    NetHeader h = { MAGIC, type, (unsigned short)len };
    if (g_tx_used + (int)sizeof(h) + len > (int)sizeof(g_tx)) {
        StopNetwork("送信が詰まりました（回線が遅すぎます）");
        return;
    }
    memcpy(g_tx + g_tx_used, &h, sizeof(h));
    g_tx_used += sizeof(h);
    if (len > 0) {
        memcpy(g_tx + g_tx_used, data, len);
        g_tx_used += len;
    }
    FlushSend();
}

static FrameData GetFrameData(int charaIdx, ActionState st) {
    if (st == ACT_BOAR_CHARGE) return { 0, 999, 0 };
    if (st == ACT_COUNTER_STANCE) return { 2, 31, 0 };

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

static void InitMasterData() {
    CHARA_PROFILES[0] = { "大乗殿 力也",   "【町内会代表】 (本格空手道)",               "空N>空SPで黒閃覚醒 / SP技はノースタミナ",          5.0f, -14.0f, 1000, RED,    ORANGE, 1, 9.5f };
    CHARA_PROFILES[1] = { "罠太郎",        "【イライラ製造職人】 (罠設置・逃げ回り)",   "K:足元に罠設置 L:罠投げ(着地で設置) / 近接は激弱", 4.6f, -13.0f,  900, YELLOW, ORANGE, 1, 10.0f };
    CHARA_PROFILES[2] = { "アナンダ・シャンカラン", "【ヨガマスター】 (伸縮肢体・軟体浮遊)", "手足の先端ヒットで威力1.5倍 / 隙に被弾判定有",     5.4f, -14.5f,  880, CYAN,   WHITE,  1, 9.0f };
    CHARA_PROFILES[3] = { "いのししマン",  "【畑を荒らすスイカ泥棒】 (運と腕の猪突猛進)", "J/K/L全部が突進! 目押しで高い出目を狙え",        4.6f, -13.2f,  980, GetColor(150, 95, 55), GREEN, 1, 10.0f };
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

// 画像は実行フォルダ（Debug）に置く。プロジェクトフォルダから起動した場合に備えて Debug\ も探す。
// .bmp が無ければ同名の .png も試す（例: 既存の boss.png）
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
        g_charaGraph[c] = TryLoadGraph(CHARA_IMAGE_FILES[c]);
        if (g_charaGraph[c] == -1) {
            char png[64];
            strcpy_s(png, CHARA_IMAGE_FILES[c]);
            char* dot = strrchr(png, '.');
            if (dot) strcpy_s(dot, sizeof(png) - (dot - png), ".png");
            g_charaGraph[c] = TryLoadGraph(png);
        }
    }
}

static void DeleteCharaGraphs() {
    for (int c = 0; c < CHARA_COUNT; c++) {
        if (g_charaGraph[c] != -1) DeleteGraph(g_charaGraph[c]);
        g_charaGraph[c] = -1;
    }
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

// 周回難易度（2周目で+1、最大+5）。ストーリー以外では0
static int GetHardLevel() {
    if (g_is_net_battle || g_mode == MODE_TRAINING || g_mode == MODE_TRAINING_SELECT || g_mode == MODE_TUTORIAL) return 0;
    return ClampI(GetCurrentLap() - 1, 0, 5);
}

static bool IsCharaUnlocked(int c) {
    return CHARA_PROFILES[c].unlockLap <= GetCurrentLap();
}

// どれかのセーブデータで解放済みならトレーニングで使える
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
// エフェクト
// ----------------------------------------------------
static void TriggerScreenShake(int frames) {
    g_screenShakeTimer = frames;
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

static void FighterShot(Fighter& f, float x, float y, float vx, float vy, int dmg, unsigned int col, int rad, int type = 0, int sub = 0) {
    SpawnShot(&f == &g_me, x, y, vx, vy, dmg, col, rad, type, sub);
}

// ----------------------------------------------------
// 技名・統計
// ----------------------------------------------------
static const char* GetMoveName(int c, ActionState st) {
    switch (st) {
    case ACT_LIGHT1:         return "J 通常技";
    case ACT_LIGHT2:         return "J→J 2段目";
    case ACT_AIR_LIGHT:      return "空中J";
    case ACT_AIR_KICK:       return "空中L";
    case ACT_THROW:          return "投げ";
    case ACT_SUPER_ATTACK:   return "超必殺技(J+K)";
    case ACT_SUPER_AIR:      return "空中超必殺技";
    case ACT_SUPER_ENHANCED: return "強化超必殺技";
    case ACT_BOAR_ROLL:
    case ACT_BOAR_CHARGE:    return "猪突猛進";
    case ACT_COUNTER_STANCE: return "当身構え(K)";
    case ACT_COUNTER_STRIKE: return "当身反撃";
    case ACT_TAUNT:          return "挑発(L)";
    case ACT_PARRY:          return "パリィ";
    case ACT_MID_ATTACK:
        switch (c) {
        case 0: return "気弾(K)";
        case 1: return "罠設置(K)";
        case 2: return "伸びる足払い(K)";
        case 4: return "横薙ぎ(K)";
        case 6: return "瞬動(K)";
        case 7: return "飛び横蹴り(K)";
        default: return "中距離技(K)";
        }
    case ACT_SKILL_ATTACK:
        switch (c) {
        case 0: return "昇天脚(L)";
        case 1: return "罠投げ(L)";
        case 2: return "ヨガ突進(L)";
        case 4: return "振り下ろし(L)";
        case 5: return "椅子振り上げ(L)";
        case 6: return "天輪掌(L)";
        case 7: return "サマーソルト(L)";
        default: return "戦技(L)";
        }
    default: return "-";
    }
}

static void ResetMatchStats() {
    memset(g_stats, 0, sizeof(g_stats));
    memset(g_moveUse, 0, sizeof(g_moveUse));
}

static const char* MostUsedMove(int idx, int chara) {
    int best = -1, bestCount = 0;
    for (int s = 0; s < ACT_STATE_COUNT; s++) {
        if (g_moveUse[idx][s] > bestCount) { bestCount = g_moveUse[idx][s]; best = s; }
    }
    return (best < 0) ? "なし" : GetMoveName(chara, (ActionState)best);
}

// ----------------------------------------------------
// トレーニング：コンボ記録と課題判定
// ----------------------------------------------------
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
        SpawnPopup(320, 170, "コンボ課題 達成!!", YELLOW);
    }
}

static void RecordAdvantage(Fighter& atk, Fighter& def, int defStun, bool guarded, bool launch) {
    if (g_mode != MODE_TRAINING) return;
    g_adv.valid = true;
    g_adv.byDummy = (&atk == &g_other);
    g_adv.guarded = guarded;
    g_adv.launch = launch;
    g_adv.adv = defStun + (def.hitStop - atk.hitStop) - atk.stateTimer;
    g_adv.chara = atk.charaIndex;
    g_adv.state = atk.state;
    int fastest = GetFrameData(def.charaIndex, ACT_LIGHT1).startup;
    g_adv.punishable = guarded && g_adv.adv < 0 && -g_adv.adv > fastest;
}

static void RecordMoveInfo(Fighter& f) {
    int i = FighterIndex(f);
    FrameData fd = GetFrameData(f.charaIndex, f.state);
    MoveInfo& m = g_moveInfo[i];
    m.valid = true;
    m.chara = f.charaIndex;
    m.state = f.state;
    m.total = f.stateTimer;
    m.startup = fd.startup;
    m.active = fd.active;
    if (m.startup + m.active > m.total) m.active = m.total - m.startup;
    if (m.active < 0) m.active = 0;
    m.recovery = m.total - m.startup - m.active;
    if (m.recovery < 0) m.recovery = 0;
}

static void RecordMoveStart(Fighter& f) {
    int i = FighterIndex(f);
    ActionState key = (f.state == ACT_BOAR_ROLL) ? ACT_BOAR_CHARGE : f.state;
    g_stats[i].moves++;
    g_moveUse[i][key]++;
    if (f.state != ACT_BOAR_ROLL && f.state != ACT_TAUNT) RecordMoveInfo(f);
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
    f.stamina = 100.0f;
    f.maxStamina = 100.0f;
    f.moveDir = dir;
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
    g_cmdListOpen = false;
    ResetControl(g_ctrl[0]);
    ResetControl(g_ctrl[1]);
}

// ストーリー用
static void SetupBattleRound() {
    int curStage = ClampI(g_slots[g_currentSlot].stageProgress, 0, LAST_STAGE);
    g_currentStageIdx = curStage;
    g_gimmickEnabled = true;
    InitStageGimmick(curStage);
    ClearBattleObjects();

    int myChara = ClampI(g_slots[g_currentSlot].chosenChara, 0, CHARA_COUNT - 1);
    int enemyChara = CPU_ROSTER[curStage].charaIndex;

    ResetFighter(g_me, 160.0f, 1, myChara, PlayableMaxHp(myChara));
    int enemyHp = CHARA_PROFILES[enemyChara].maxHp + curStage * 80 + GetHardLevel() * 100;
    ResetFighter(g_other, 480.0f, -1, enemyChara, enemyHp);

    ResetBattleGlobals();
    ResetMatchStats();
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
        if (!victim.isGrounded || victim.invulFrames > 0) continue;
        if (IsHitState(victim.state) || victim.state == ACT_SLIP || victim.state == ACT_STUN || victim.state == ACT_COUNTER_STRIKE) continue;
        if (fabsf(victim.x - t.x) > 22.0f) continue;

        victim.animTimer = 0;
        victim.hasHit = false;
        victim.hitStop = 6;
        victim.stanceLevel = 0;
        victim.guardStun = 0;
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
            if (!g_is_net_battle && g_mode != MODE_TRAINING) {
                AddCpuTilt(1);
                SpawnPopup(victim.x + 30, victim.y - 110, "ｲﾗｯ!!", RED);
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
    bool launcher, smash, unblockable;
};

// 攻撃側の現在の技の判定（論理座標：atk.x + offX, atk.y + offY が中心）
static bool GetAttackBox(const Fighter& atk, float distToDef, AttackBox& b) {
    memset(&b, 0, sizeof(b));
    b.stun = 20;
    int c = atk.charaIndex;
    float d = (float)atk.dir;
    bool customSuper = (c == 1 || c == 4 || c == 6 || c == 7);

    if (atk.state == ACT_SUPER_AIR) {
        b.w = 85; b.h = 75; b.offX = d * 45; b.offY = 20;
        b.damage = 240; b.kx = d * 8.0f; b.ky = 18.0f; b.smash = true; b.stun = 55;
    }
    else if (atk.state == ACT_SUPER_ENHANCED) {
        if (atk.fromAirLightCombo) {
            b.w = 100; b.h = 110; b.offX = d * 45; b.offY = -25;
            b.damage = 450; b.kx = d * 5.0f; b.ky = -24.0f; b.launcher = true; b.stun = 80;
        }
        else {
            b.w = 95; b.h = 90; b.offX = d * 45; b.offY = -35;
            b.damage = 330; b.kx = d * 3.5f; b.ky = -22.0f; b.launcher = true; b.stun = 70;
        }
    }
    else if (atk.state == ACT_SUPER_ATTACK && !customSuper) {
        b.w = 80; b.h = 50; b.offX = d * 45; b.offY = -15;
        b.damage = 190; b.kx = d * 7.0f; b.ky = -15.0f; b.launcher = true; b.stun = 60;
    }
    else if (atk.state == ACT_BOAR_CHARGE) {
        int face = atk.boarSuper ? 6 : ClampI(atk.diceFace, 3, 6);
        const BoarFace& bf = BOAR_FACES[face];
        b.w = 64; b.h = 54; b.offX = d * 30; b.offY = -10;
        b.damage = atk.boarSuper ? 260 : bf.damage;
        b.kx = d * bf.knockX; b.ky = -9.0f;
        b.launcher = true; b.stun = 50; b.unblockable = bf.unblockable;
    }
    else {
        switch (c) {
        case 0:
            if (atk.state == ACT_LIGHT1) {
                b.w = 48; b.h = 28; b.offX = d * 35; b.offY = -12; b.damage = 45; b.kx = d * 2.4f; b.stun = 18;
            }
            else if (atk.state == ACT_LIGHT2) {
                b.w = 55; b.h = 32; b.offX = d * 40; b.offY = -10; b.damage = 55; b.kx = d * 3.2f; b.stun = 22;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                // 足元から頭上までを縦長にカバー（対地・対空どちらにも当たる）
                b.w = 64; b.h = 110; b.offX = d * 34; b.offY = -25; b.damage = 125; b.kx = d * 1.5f; b.ky = -17.5f; b.launcher = true; b.stun = 48;
            }
            else if (atk.state == ACT_AIR_KICK) {
                b.w = 58; b.h = 50; b.offX = d * 32; b.offY = 22; b.damage = 135; b.kx = d * 4.0f; b.ky = 16.5f; b.smash = true; b.stun = 42;
            }
            else if (atk.state == ACT_AIR_LIGHT) {
                b.w = 52; b.h = 38; b.offX = d * 28; b.offY = 0; b.damage = 55; b.kx = d * 1.8f; b.ky = -2.0f; b.stun = 28;
            }
            break;

        case 1: // 罠太郎：近接は激弱
            if (atk.state == ACT_LIGHT1) {
                b.w = 30; b.h = 22; b.offX = d * 24; b.offY = -12; b.damage = 22; b.kx = d * 1.5f; b.stun = 14;
            }
            else if (atk.state == ACT_LIGHT2) {
                b.w = 32; b.h = 24; b.offX = d * 26; b.offY = -12; b.damage = 28; b.kx = d * 2.0f; b.stun = 16;
            }
            else if (atk.state == ACT_AIR_LIGHT) {
                b.w = 36; b.h = 30; b.offX = d * 22; b.offY = 0; b.damage = 25; b.kx = d * 1.5f; b.ky = -1.0f; b.stun = 16;
            }
            break;

        case 2:
            if (atk.state == ACT_LIGHT1) {
                b.w = 120; b.h = 28; b.offX = d * 80; b.offY = -12; b.damage = 50; b.kx = d * 3.5f; b.stun = 20;
                if (distToDef > 100.0f) b.damage = (int)(b.damage * 1.5f);
            }
            else if (atk.state == ACT_LIGHT2) {
                b.w = 120; b.h = 30; b.offX = d * 80; b.offY = -14; b.damage = 55; b.kx = d * 3.8f; b.stun = 22;
                if (distToDef > 100.0f) b.damage = (int)(b.damage * 1.5f);
            }
            else if (atk.state == ACT_MID_ATTACK) {
                b.w = 160; b.h = 32; b.offX = d * 105; b.offY = -8; b.damage = 70; b.kx = d * 5.0f; b.stun = 25;
                if (distToDef > 120.0f) b.damage = (int)(b.damage * 1.5f);
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                b.w = 75; b.h = 75; b.offX = d * 45; b.offY = -28; b.damage = 140; b.kx = d * 2.2f; b.ky = -16.5f; b.launcher = true; b.stun = 48;
            }
            else if (atk.state == ACT_AIR_KICK || atk.state == ACT_AIR_LIGHT) {
                b.w = 85; b.h = 80; b.offX = d * 45; b.offY = 25; b.damage = 135; b.kx = d * 4.0f; b.ky = 16.0f; b.smash = true; b.stun = 42;
            }
            break;

        case 4: // メガトン大前：物干し竿
            if (atk.state == ACT_LIGHT1) {
                b.w = 60; b.h = 40; b.offX = d * 45; b.offY = -12; b.damage = 50; b.kx = d * 3.0f; b.stun = 20;
            }
            else if (atk.state == ACT_LIGHT2) {
                b.w = 64; b.h = 40; b.offX = d * 48; b.offY = -12; b.damage = 60; b.kx = d * 3.5f; b.stun = 22;
            }
            else if (atk.state == ACT_MID_ATTACK) {
                b.w = 300; b.h = 55; b.offX = d * 165; b.offY = -15; b.damage = 165; b.kx = d * 10.0f; b.ky = -5.0f; b.smash = true; b.stun = 42;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                b.w = 170; b.h = 130; b.offX = d * 115; b.offY = -25; b.damage = 190; b.kx = d * 4.0f; b.ky = -17.0f; b.launcher = true; b.stun = 50;
            }
            else if (atk.state == ACT_SUPER_ATTACK) {
                b.w = 1280; b.h = 90; b.offX = 0; b.offY = -15; b.damage = 270; b.kx = d * 12.0f; b.ky = -10.0f; b.launcher = true; b.stun = 60;
            }
            break;

        case 5:
            if (atk.state == ACT_LIGHT1) {
                b.w = 62; b.h = 35; b.offX = d * 44; b.offY = -12; b.damage = 55; b.kx = d * 3.5f; b.stun = 22;
            }
            else if (atk.state == ACT_LIGHT2) {
                b.w = 62; b.h = 35; b.offX = d * 46; b.offY = -12; b.damage = 60; b.kx = d * 3.8f; b.stun = 22;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                b.w = 68; b.h = 65; b.offX = d * 44; b.offY = -32; b.damage = 120; b.kx = d * 2.0f; b.ky = -16.5f; b.launcher = true; b.stun = 46;
            }
            break;

        case 6:
            if (atk.state == ACT_LIGHT1) {
                b.w = 70; b.h = 45; b.offX = d * 45; b.offY = -12; b.damage = 65; b.kx = d * 3.5f; b.stun = 25;
            }
            else if (atk.state == ACT_LIGHT2) {
                b.w = 72; b.h = 45; b.offX = d * 48; b.offY = -12; b.damage = 70; b.kx = d * 4.0f; b.stun = 26;
            }
            else if (atk.state == ACT_MID_ATTACK) {
                b.w = 60; b.h = 45; b.offX = d * 35; b.offY = -12; b.damage = 80; b.kx = d * 4.0f; b.stun = 22;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                b.w = 80; b.h = 80; b.offX = d * 45; b.offY = -35; b.damage = 150; b.kx = d * 2.0f; b.ky = -18.0f; b.launcher = true; b.stun = 55;
            }
            else if (atk.state == ACT_SUPER_ATTACK) {
                b.w = 80; b.h = 60; b.offX = d * 40; b.offY = -15; b.damage = 70; b.kx = d * 3.0f; b.ky = -2.0f; b.stun = 30;
            }
            break;

        case 7: // 吉田：蹴り技はどれもリーチと火力が高い
            if (atk.state == ACT_LIGHT1) {
                b.w = 70; b.h = 28; b.offX = d * 50; b.offY = 0; b.damage = 55; b.kx = d * 2.6f; b.stun = 20;
            }
            else if (atk.state == ACT_LIGHT2) {
                b.w = 80; b.h = 36; b.offX = d * 55; b.offY = -20; b.damage = 70; b.kx = d * 4.0f; b.stun = 24;
            }
            else if (atk.state == ACT_MID_ATTACK) {
                b.w = 90; b.h = 40; b.offX = d * 60; b.offY = -12; b.damage = 100; b.kx = d * 9.0f; b.ky = -4.0f; b.smash = true; b.stun = 36;
            }
            else if (atk.state == ACT_SKILL_ATTACK) {
                b.w = 60; b.h = 110; b.offX = d * 30; b.offY = -25; b.damage = 130; b.kx = d * 2.0f; b.ky = -18.0f; b.launcher = true; b.stun = 48;
            }
            else if (atk.state == ACT_AIR_LIGHT) {
                b.w = 60; b.h = 50; b.offX = d * 32; b.offY = 20; b.damage = 95; b.kx = d * 3.0f; b.ky = 15.0f; b.smash = true; b.stun = 36;
            }
            else if (atk.state == ACT_AIR_KICK) {
                b.w = 90; b.h = 50; b.offX = d * 30; b.offY = 10; b.damage = 120; b.kx = d * 7.5f; b.ky = -6.0f; b.smash = true; b.stun = 40;
            }
            else if (atk.state == ACT_SUPER_ATTACK) {
                b.w = 80; b.h = 60; b.offX = d * 45; b.offY = -10; b.damage = 42; b.kx = d * 2.5f; b.ky = -1.0f; b.stun = 30;
            }
            break;

        case 8: // 無防備たけし：自分の攻撃は雀の涙
            if (atk.state == ACT_LIGHT1) {
                b.w = 24; b.h = 22; b.offX = d * 20; b.offY = -12; b.damage = 26; b.kx = d * 1.2f; b.stun = 14;
            }
            else if (atk.state == ACT_LIGHT2) {
                b.w = 26; b.h = 24; b.offX = d * 22; b.offY = -12; b.damage = 30; b.kx = d * 1.6f; b.stun = 16;
            }
            else if (atk.state == ACT_AIR_LIGHT) {
                b.w = 28; b.h = 26; b.offX = d * 20; b.offY = 0; b.damage = 28; b.kx = d * 1.2f; b.stun = 16;
            }
            break;
        }
    }
    return b.damage > 0;
}

// やられ判定（論理座標の中心と幅・高さ）
static void GetHurtBox(const Fighter& f, float& cx, float& cy, float& w, float& h) {
    w = (f.charaIndex == 4) ? 48.0f : 30.0f;
    h = (f.charaIndex == 4) ? 60.0f : 50.0f;
    cx = f.x;
    cy = f.y;
    if (f.state == ACT_SLIP) {
        w = 46.0f; h = 26.0f; cy = f.y + 12.0f;
    }
    else if (f.crouching && (f.state == ACT_IDLE || f.state == ACT_GUARD)) {
        cy = f.y + h * 0.2f;
        h = h * 0.6f;
    }
    if (f.charaIndex == 2) {
        if (f.state == ACT_LIGHT1 || f.state == ACT_LIGHT2) {
            w = 130.0f;
            cx = f.x + (float)f.dir * 65.0f;
        }
        else if (f.state == ACT_MID_ATTACK) {
            w = 160.0f;
            cx = f.x + (float)f.dir * 80.0f;
        }
    }
}

// ----------------------------------------------------
// ダメージ
// ----------------------------------------------------
static void ApplyDamage(Fighter& def, Fighter& atk, int dmg, int stun, float kx, float ky, bool isLauncher, bool isSmash, bool unblockable = false, int srcTag = -1) {
    // 周回難易度：CPUの攻撃力アップ
    if (!g_is_net_battle && &atk == &g_other) dmg = dmg * (100 + GetHardLevel() * 18) / 100;
    // 壁バウンド後の追撃は火力を絞る（限定追撃）
    if (def.wallBounceUsed) dmg = dmg * 7 / 10;

    int ai = FighterIndex(atk);
    int di = FighterIndex(def);

    if (def.state == ACT_GUARD) {
        if (!unblockable) {
            int chipDmg = dmg / 5;
            if (chipDmg < 4) chipDmg = 4;
            def.hp -= chipDmg;
            def.hitStop = 4;
            def.vx = kx * 0.4f;
            int gs = stun / 2 + 2;
            if (def.guardStun < gs) def.guardStun = gs;
            g_stats[di].guards++;
            if (&def == &g_me) TutorialEvent(TE_GUARD, 0);
            SpawnHitFX(def.x + (float)def.dir * 16.0f, def.y - 15, BLUE, 6);
            SpawnSparks(def.x + (float)def.dir * 16.0f, def.y - 15, CYAN, 3, 18.0f);
            if (srcTag < 0) RecordAdvantage(atk, def, def.guardStun, true, false);
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
    def.guardStun = 0;
    def.comboTaken++;
    def.wallBounceReady = !def.wallBounceUsed && fabsf(kx) >= 6.5f;

    atk.comboCount++;
    atk.comboDisplayTimer = 60;

    g_stats[ai].hits++;
    g_stats[ai].damage += dmg;
    if (atk.comboCount > g_stats[ai].maxCombo) g_stats[ai].maxCombo = atk.comboCount;
    g_stats[di].taken++;

    if (&atk == &g_me) TutorialEvent(TE_HIT, srcTag >= 0 ? srcTag : (int)atk.state);
    if (g_mode == MODE_TRAINING && &atk == &g_me) {
        g_comboDmg += dmg;
        g_comboHits++;
        LogComboTag(srcTag >= 0 ? srcTag : (int)atk.state);
        if (srcTag < 0 && atk.state == ACT_BOAR_CHARGE && (atk.diceFace == 6 || atk.boarSuper)) LogComboTag(TAG_DICE6);
    }

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

    if (srcTag < 0) RecordAdvantage(atk, def, stun, false, isLauncher || isSmash);
}

// たけしの当身成功：受けた技が大きいほど返しも大きい
static void TriggerCounter(Fighter& def, Fighter& atk, int incoming) {
    bool sup = (def.stanceLevel >= 2);
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

    if (g_currentStageIdx == 1) {
        SpawnPopup(g_gimmick.x, GROUND_Y - 160, "感電コンテナ破壊!! 電流停止", CYAN);
    }
    if (g_currentStageIdx == 2) {
        g_gimmick.itemActive = true;
        g_gimmick.itemX = g_gimmick.x;
        g_gimmick.itemY = GROUND_Y;
    }
}

static void CheckAttackHit(Fighter& atk, Fighter& def) {
    if (atk.hasHit) return;

    FrameData fd = GetFrameData(atk.charaIndex, atk.state);
    if (atk.animTimer < fd.startup || atk.animTimer >= (fd.startup + fd.active)) return;

    if (atk.state == ACT_SUPER_ENHANCED && atk.fromAirLightCombo && atk.animTimer == fd.startup) {
        TriggerScreenShake(20);
        SpawnSparks(atk.x, atk.y - 20, YELLOW, 25, 75.0f);
        SpawnSparks(atk.x, atk.y - 20, RED, 25, 75.0f);
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
            def.guardStun = 0;
            int ai = FighterIndex(atk);
            g_stats[ai].hits++;
            g_stats[ai].damage += 95;
            g_stats[FighterIndex(def)].taken++;
            if (&atk == &g_me) TutorialEvent(TE_HIT, ACT_THROW);
            if (g_mode == MODE_TRAINING && &atk == &g_me) {
                g_comboDmg += 95;
                g_comboHits++;
                LogComboTag(ACT_THROW);
            }
            TriggerScreenShake(8);
            SpawnHitFX(def.x, def.y - 20, YELLOW, 20);
        }
        return;
    }

    AttackBox b;
    if (!GetAttackBox(atk, fabsf(atk.x - def.x), b)) return;
    int damage = b.damage;
    int hitStun = b.stun;

    float ax = atk.x + b.offX;
    float ay = atk.y + b.offY;
    DamageGimmick(atk, damage, ax, ay);

    float hcx, hcy, hw, hh;
    GetHurtBox(def, hcx, hcy, hw, hh);

    if (fabsf(ax - hcx) < (b.w + hw) / 2.0f && fabsf(ay - hcy) < (b.h + hh) / 2.0f) {
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
            g_stats[FighterIndex(def)].parries++;
            if (&def == &g_me) {
                LogComboTag(TAG_PARRY);
                TutorialEvent(TE_PARRY, 0);
            }
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

        ApplyDamage(def, atk, damage, hitStun, b.kx, b.ky, b.launcher, b.smash, b.unblockable);
        if (atk.state == ACT_BOAR_CHARGE) atk.vx *= 0.3f;
    }
}

static void UpdateStageGimmick(int stageIdx) {
    if (!g_gimmickEnabled) {
        g_gimmick.hazardActive = false;
        g_gimmick.hazardWarning = false;
        return;
    }
    g_gimmick.stageTimer++;

    // 感電床：予告3秒 → 通電1秒。ジャンプ・ガード・安全地帯・コンテナ破壊で回避できる
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
// 共通の物理・状態更新
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
        if (&f == &g_other) LogComboTag(TAG_WALL);
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
    if (f.guardStun > 0) f.guardStun--;

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

    if (isPlayer && g_currentStageIdx == 3 && !f.isGrounded) {
        f.vx -= 0.15f;
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
    if (&f == &g_me) TutorialEvent(TE_DASH, moveDir);
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
    SpawnPopup(f.x, f.y - 90, "BURST!! コンボ抜け", CYAN);
    if (&f == &g_me) TutorialEvent(TE_BURST, 0);
    return true;
}

// 通常技・必殺技の発動（btn 0:J 1:K 2:L）。プレイヤー・CPU共通
static bool StartAttackInner(Fighter& f, Fighter& opp, int btn) {
    int c = f.charaIndex;

    // いのししマン：どのボタンでも突進（サイコロ判定）
    if (c == 3) {
        if (!f.isGrounded) return false;
        f.state = ACT_BOAR_ROLL;
        f.stateTimer = 999;
        f.animTimer = 0;
        f.hasHit = false;
        f.boarSuper = false;
        f.boarTiming = 0;
        f.vx = 0;
        return true;
    }

    if (btn == 1 && !f.isGrounded) return false;
    if (btn == 2 && !f.isGrounded && c == 8) return false;

    f.pendingRiseVy = 0.0f;
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
            // 昇天脚：踏み込んでから、攻撃判定が出る瞬間に跳び上がる（地上の相手にも当たる）
            f.vx = (float)f.dir * 2.0f;
            f.pendingRiseVy = -16.5f;
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
        case 7: // サマーソルト（昇天脚と同じく発生してから跳ぶ）
            f.stateTimer = 30;
            f.vx = (float)f.dir * 2.5f;
            f.pendingRiseVy = -13.0f;
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

static bool StartAttack(Fighter& f, Fighter& opp, int btn) {
    if (!StartAttackInner(f, opp, btn)) return false;
    RecordMoveStart(f);
    return true;
}

// 地上超必殺技（J+K）
static void StartSuper(Fighter& f, Fighter& opp) {
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
        f.boarTiming = 0;
        f.vx = 0;
        g_superCutinName = "=== 確変!! 猪突猛進・出目6確定 ! ===";
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
    RecordMoveStart(f);
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
                int penalty = (f.state == ACT_SUPER_ATTACK) ? 40 : 55;
                f.stateTimer += penalty;
                MoveInfo& m = g_moveInfo[FighterIndex(f)];
                if (m.valid && m.state == f.state) { m.recovery += penalty; m.total += penalty; }
                SpawnPopup(f.x, f.y - 95, "スカッ…（大スキ!!）", GRAY);
            }
        }
    }

    // 発生フレームで跳び上がる（このフレームの当たり判定はまだ地上の位置で行われる）
    if (f.state == ACT_SKILL_ATTACK && f.pendingRiseVy != 0.0f &&
        f.animTimer >= GetFrameData(c, ACT_SKILL_ATTACK).startup) {
        f.vy = f.pendingRiseVy;
        f.isGrounded = false;
        f.pendingRiseVy = 0.0f;
        if (c == 0) TriggerScreenShake(6);
    }

    // 多段ヒットの超必殺
    if ((c == 6 || c == 7) && f.state == ACT_SUPER_ATTACK && f.animTimer % 6 == 0) f.hasHit = false;
    if (c == 7 && f.state == ACT_SUPER_ATTACK) f.vx = (float)f.dir * 2.5f;

    switch (f.state) {
    case ACT_BOAR_ROLL: {
        f.vx = 0;
        if (f.animTimer % 3 == 0) f.diceFace = 1 + rand() % 6;
        // CPU（とダミー）は確率で目押し成功
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
            const BoarFace& bf = BOAR_FACES[face];
            if (face >= 3) {
                f.state = ACT_BOAR_CHARGE;
                f.stateTimer = f.boarSuper ? 40 : bf.duration;
                f.vx = (float)f.dir * bf.speed;
                if (face >= 5) TriggerScreenShake(8);
                RecordMoveInfo(f);
            }
            else {
                // 失敗しても小転倒で済む
                f.state = ACT_SLIP;
                f.stateTimer = (face == 1) ? 40 : 24;
                f.vx = (float)f.dir * ((face == 1) ? 3.0f : 1.5f);
            }
            SpawnPopup(f.x, f.y - 95, bf.label, (face >= 5) ? YELLOW : ((face >= 3) ? WHITE : GRAY));
        }
        break;
    }
    case ACT_BOAR_CHARGE: {
        int face = f.boarSuper ? 6 : ClampI(f.diceFace, 3, 6);
        if (!f.hasHit) f.vx = (float)f.dir * BOAR_FACES[face].speed;
        if (f.animTimer % 3 == 0) SpawnHitFX(f.x - (float)f.dir * 20.0f, f.y + 15, GRAY, 2);
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
        if (f.animTimer == 2) SpawnPopup(f.x, f.y - 95, "ほらほら、打ってきなよ？", CYAN);
        if (f.animTimer == 40) {
            f.spGauge += 25; if (f.spGauge > 100) f.spGauge = 100;
            SpawnPopup(f.x, f.y - 75, "SP UP!", YELLOW);
            if (&f == &g_me && !g_is_net_battle && g_mode != MODE_TRAINING) {
                AddCpuTilt(2);
                SpawnPopup(opp.x, opp.y - 110, "ｲﾗｲﾗ…!!", RED);
            }
        }
        break;
    default:
        break;
    }
}

// ----------------------------------------------------
// 操作されるファイター（ストーリー・トレーニング・通信のホスト側で共通）
// ----------------------------------------------------
static void UpdateControlledFighter(Fighter& f, Fighter& opp, ControlState& cs, unsigned short bits) {
    UpdateControl(cs, bits);
    if (!UpdateFighterCommon(f, opp, true)) return;
    if (g_readyFightTimer < 80) return;

    UpdateStateSpecials(f, opp);
    int c = f.charaIndex;
    bool hJ = (cs.held & IN_J) != 0;
    bool hK = (cs.held & IN_K) != 0;
    bool hL = (cs.held & IN_L) != 0;
    bool hSpace = (cs.held & IN_SPACE) != 0;
    bool hLeft = (cs.held & IN_LEFT) != 0;
    bool hRight = (cs.held & IN_RIGHT) != 0;

    // コンボ抜け（被弾中に SPACE）
    if (IsHitState(f.state) && (cs.pressed & IN_SPACE)) {
        if (!TryBurst(f, opp)) {
            SpawnPopup(f.x, f.y - 90, (f.burstCooldown > 0) ? "バースト再使用まで待機" : "バーストにはSP50必要", GRAY);
        }
    }

    // いのしし：サイコロ回転中の目押し
    if (f.state == ACT_BOAR_ROLL && !f.boarSuper && f.boarTiming == 0 && (cs.pressed & (IN_J | IN_K | IN_L))) {
        bool good = (f.animTimer >= BOAR_TIMING_START && f.animTimer <= BOAR_TIMING_END);
        f.boarTiming = good ? 1 : 2;
        SpawnPopup(f.x, f.y - 120, good ? "目押し成功!!" : "ズレた…", good ? GREEN : GRAY);
    }

    // ==========================================
    // 空N（空中J）＞空SP（黒閃＆パチンコ風覚醒キャンセル）
    // ==========================================
    if (c == 0 && f.state == ACT_AIR_LIGHT && !f.isGrounded && hJ && hK && f.spGauge >= 100) {
        f.spGauge = 0;
        f.hasHit = false;
        f.animTimer = 0;
        f.fromAirLightCombo = true;
        f.state = ACT_SUPER_ENHANCED;
        f.stateTimer = 55;
        f.vx = (float)f.dir * 11.5f;
        f.vy = 12.0f;
        g_superCutinTimer = 60;
        g_superIsUltimate = true;
        g_cutinIsCounter = false;
        g_kokusenStrobe = 45; // 45フレーム空間反転
        g_superCutinName = "【大当り】真・極点昇天破!! (確変突入)";
        TriggerScreenShake(22);
        // 空間を裂く黒閃の稲妻スパーク
        SpawnSparks(f.x, f.y, RED, 30, 80.0f);
        SpawnSparks(f.x, f.y, YELLOW, 30, 80.0f);
        RecordMoveStart(f);
        return;
    }

    // 空中SP技
    if (c == 0 && !f.isGrounded && f.state == ACT_IDLE && hJ && hK && f.spGauge >= 100) {
        f.spGauge = 0;
        f.hasHit = false;
        f.animTimer = 0;
        f.fromAirLightCombo = false;
        f.state = ACT_SUPER_AIR;
        f.stateTimer = 45;
        f.vx = (float)f.dir * 9.5f;
        f.vy = 14.0f;
        g_superCutinTimer = 45;
        g_superIsUltimate = false;
        g_cutinIsCounter = false;
        g_superCutinName = "=== 天翔・金剛螺旋脚 (AIR SUPER) ===";
        TriggerScreenShake(12);
        SpawnSparks(f.x, f.y, ORANGE, 15, 45.0f);
        RecordMoveStart(f);
        return;
    }

    // 空中アクション：W再入力で2段ジャンプ / SS急降下 / AA・DD空中ダッシュ
    if (!f.isGrounded && (f.state == ACT_IDLE || f.state == ACT_AIR_LIGHT)) {
        if (f.state == ACT_IDLE && (cs.pressed & IN_UP) && !f.doubleJumpUsed && c != 4) {
            f.vy = CHARA_PROFILES[c].jumpPower * 0.85f;
            f.doubleJumpUsed = true;
            SpawnHitFX(f.x, f.y + 10, WHITE, 8);
            if (&f == &g_me) TutorialEvent(TE_DOUBLE_JUMP, 0);
        }
        if (cs.dbl & IN_DOWN) {
            f.vy = 17.0f;
            SpawnHitFX(f.x, f.y - 30, WHITE, 6);
            if (&f == &g_me) TutorialEvent(TE_FAST_FALL, 0);
        }
        if (f.state == ACT_IDLE && !f.airDashUsed && (cs.dbl & (IN_LEFT | IN_RIGHT))) {
            int md = (cs.dbl & IN_LEFT) ? -1 : 1;
            f.vx = (float)md * CHARA_PROFILES[c].dashSpeed * 0.9f;
            if (f.vy > -2.0f) f.vy = -2.0f;
            f.airDashUsed = true;
            SpawnHitFX(f.x - (float)md * 15.0f, f.y - 10, WHITE, 6);
        }
    }

    // パリィの受付が切れても SPACE を押し続けていればガードへ移行
    if (f.state == ACT_PARRY && f.parryWindow == 0 && hSpace && f.isGrounded) {
        f.state = ACT_GUARD;
        f.stateTimer = 0;
        f.animTimer = 0;
    }
    // SPACE を離したらガード解除（ガード硬直中は解除できない）
    if (f.state == ACT_GUARD && f.stateTimer == 0 && f.guardStun == 0 && (!hSpace || !f.isGrounded)) {
        f.state = ACT_IDLE;
        f.animTimer = 0;
    }

    // 地上ダッシュ（AA / DD）
    if (f.isGrounded && f.state == ACT_IDLE && (cs.dbl & (IN_LEFT | IN_RIGHT)) && f.stamina >= 6.0f) {
        f.stamina -= 6.0f;
        StartDash(f, (cs.dbl & IN_LEFT) ? -1 : 1);
    }

    // 回避（I / 左Shift）：押している方向へ、何も押していなければ後ろへ無敵ロール
    if ((cs.pressed & IN_DODGE) && f.isGrounded && (f.state == ACT_IDLE || f.state == ACT_DASH ||
        (f.state == ACT_GUARD && f.guardStun == 0)) && f.stamina >= 18.0f) {
        int md = hLeft ? -1 : (hRight ? 1 : -f.dir);
        f.stamina -= 18.0f;
        StartDodge(f, md);
    }

    if (f.state == ACT_IDLE || f.state == ACT_DASH) {
        bool dashing = (f.state == ACT_DASH);
        float speed = CHARA_PROFILES[c].walkSpeed;

        if (!dashing) {
            if (hLeft) f.vx = -speed;
            if (hRight) f.vx = speed;
        }

        if ((cs.held & IN_UP) && f.isGrounded && c != 4) {
            f.vy = CHARA_PROFILES[c].jumpPower;
            f.isGrounded = false;
            if (dashing) { f.state = ACT_IDLE; f.stateTimer = 0; f.animTimer = 0; }
            if (&f == &g_me) TutorialEvent(TE_JUMP, 0);
        }

        int jCost = (c == 3) ? 20 : 8;
        int kCost = (c == 3) ? 20 : 22;
        int lCost = (c == 3) ? 20 : 32;

        bool isForwardKey = (f.dir == 1) ? hRight : hLeft;
        if (isForwardKey && hJ && f.isGrounded && f.stamina >= 15.0f) {
            // 投げ
            f.stamina -= 15.0f;
            f.state = ACT_THROW;
            f.stateTimer = 22;
            f.animTimer = 0;
            f.hasHit = false;
            RecordMoveStart(f);
            return;
        }
        else if ((cs.pressed & IN_SPACE) && f.isGrounded && f.stamina >= 10.0f) {
            // タイミング押しでパリィ（たけしは受付が約3倍）
            f.stamina -= 10.0f;
            f.state = ACT_PARRY;
            f.stateTimer = (c == 8) ? 22 : 16;
            f.animTimer = 0;
            f.parryWindow = (c == 8) ? 14 : 5;
            f.vx = 0;
        }
        else if (hSpace && f.isGrounded) {
            // 押しっぱなしはガード
            f.state = ACT_GUARD;
            f.stateTimer = 0;
            f.animTimer = 0;
            f.vx = 0;
        }
        else if (hJ && hK && f.spGauge >= 100 && f.isGrounded) {
            StartSuper(f, opp);
            return;
        }
        else if (hJ && f.stamina >= jCost) {
            if (StartAttack(f, opp, 0)) f.stamina -= jCost;
        }
        else if (hK && f.stamina >= kCost) {
            if (StartAttack(f, opp, 1)) f.stamina -= kCost;
        }
        else if (hL && f.stamina >= lCost) {
            if (StartAttack(f, opp, 2)) f.stamina -= lCost;
        }
    }
    else if (f.state == ACT_LIGHT1 && f.hasHit && hJ && f.stamina >= 8.0f && c != 3) {
        f.stamina -= 8.0f;
        f.state = ACT_LIGHT2;
        f.stateTimer = 20;
        f.animTimer = 0;
        f.hasHit = false;
        f.vx = (float)f.dir * ((c == 8 || c == 1) ? 1.5f : 3.5f);
        RecordMoveStart(f);
    }

    CheckAttackHit(f, opp);
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
    RecordMoveStart(g_other);
}

// stage < 0 はトレーニングの「CPU行動」（キャラに合った技を素直に振る）
static void CpuThink(int stage, const CpuBossData& boss) {
    int hard = GetHardLevel();
    float dist = fabsf(g_me.x - g_other.x);
    float speed = CHARA_PROFILES[g_other.charaIndex].walkSpeed;
    bool generic = (stage < 0);

    // 相手の空振り・転倒・挑発には全力で差し込む
    bool playerOpen = (g_me.whiffChecked && !g_me.hasHit &&
        (g_me.state == ACT_MID_ATTACK || g_me.state == ACT_SKILL_ATTACK || g_me.state == ACT_SUPER_ATTACK))
        || g_me.state == ACT_SLIP || g_me.state == ACT_TAUNT;
    if (playerOpen) {
        g_other.vx = (float)g_other.dir * speed * 2.2f;
        if (dist <= 90.0f && g_other.isGrounded) {
            if (generic) StartAttack(g_other, g_me, 2);
            else CpuStartState(ACT_SKILL_ATTACK, 34);
        }
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

    if (generic) {
        g_other.vx = (float)g_other.dir * speed;
        if (dist > 150.0f && rand() % 70 == 0) { StartAttack(g_other, g_me, 1); return; }
        if (dist <= ((g_other.charaIndex == 4) ? 260.0f : 85.0f) && g_other.isGrounded && rand() % 12 == 0) {
            StartAttack(g_other, g_me, rand() % 3);
        }
        return;
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

// トレーニングのダミー（6:CPU行動 以外は絶対に攻撃してこない）
static void UpdateDummy() {
    Fighter& d = g_other;
    if (!UpdateFighterCommon(d, g_me, false)) return;
    UpdateStateSpecials(d, g_me);

    d.crouching = (g_dummyMode == DUMMY_CROUCH_GUARD);
    bool freeGuard = (d.state == ACT_GUARD && d.stateTimer == 0 && d.guardStun == 0);

    switch (g_dummyMode) {
    case DUMMY_STOP:
        if (freeGuard) d.state = ACT_IDLE;
        break;
    case DUMMY_STAND_GUARD:
    case DUMMY_CROUCH_GUARD:
        if (d.state == ACT_IDLE && d.isGrounded) {
            d.state = ACT_GUARD;
            d.stateTimer = 0;
        }
        break;
    case DUMMY_RANDOM_JUMP:
        if (freeGuard) d.state = ACT_IDLE;
        if (d.state == ACT_IDLE && d.isGrounded && d.charaIndex != 4 && rand() % 40 == 0) {
            d.vy = CHARA_PROFILES[d.charaIndex].jumpPower;
            d.isGrounded = false;
        }
        break;
    case DUMMY_RANDOM_GUARD:
        if (freeGuard) d.state = ACT_IDLE;
        if (d.state == ACT_IDLE && d.isGrounded && IsAttackState(g_me.state) && g_me.animTimer == 0 && rand() % 2 == 0) {
            d.state = ACT_GUARD;
            d.stateTimer = 30;
        }
        break;
    case DUMMY_CPU:
        if (freeGuard) d.state = ACT_IDLE;
        if (d.state == ACT_IDLE) CpuThink(-1, TRAINING_BOSS);
        CheckAttackHit(d, g_me);
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
}

static void UpdateShots() {
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

        float hcx, hcy, hw, hh;
        GetHurtBox(def, hcx, hcy, hw, hh);

        if (g_shots[i].damage > 0 && def.invulFrames == 0 &&
            fabsf(g_shots[i].x - hcx) < (hw / 2.0f + 15.0f) && fabsf(g_shots[i].y - (hcy - 15)) < hh / 2.0f + 10.0f) {
            bool reflect = (def.state == ACT_PARRY && def.parryWindow > 0) ||
                (def.state == ACT_COUNTER_STANCE && def.stanceLevel >= 2);
            if (reflect) {
                g_shots[i].vx *= -1.3f;
                g_shots[i].isFromMe = !g_shots[i].isFromMe;
                def.spGauge += 20; if (def.spGauge > 100) def.spGauge = 100;
                g_stats[FighterIndex(def)].parries++;
                if (&def == &g_me) {
                    LogComboTag(def.state == ACT_PARRY ? TAG_PARRY : TAG_COUNTER_SUPER);
                    if (def.state == ACT_PARRY) TutorialEvent(TE_PARRY, 0);
                }
                SpawnHitFX(def.x, def.y - 15, CYAN, 20);
                SpawnSparks(def.x, def.y - 15, CYAN, 10, 35.0f);
                if (def.state == ACT_COUNTER_STANCE) SpawnPopup(def.x, def.y - 90, "見切り!", CYAN);
                continue;
            }
            ApplyDamage(def, atk, g_shots[i].damage, 22, (g_shots[i].vx > 0 ? 1.0f : -1.0f) * 5.0f, 0, false, false, false, TAG_SHOT);
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

static void DrawHazard() {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;
    int floorY = (int)GROUND_Y + 22 + oy;
    if (!g_gimmick.hazardWarning && !g_gimmick.hazardActive) return;

    int sx1 = (int)(g_gimmick.x - HAZARD_SAFE_HALF) + ox;
    int sx2 = (int)(g_gimmick.x + HAZARD_SAFE_HALF) + ox;

    if (g_gimmick.hazardWarning) {
        // 予告：床が黄色く点滅し、カウントダウンを表示
        if ((g_frameCount / 8) % 2 == 0) {
            DrawBox(0, floorY, sx1, floorY + 4, YELLOW, TRUE);
            DrawBox(sx2, floorY, 640, floorY + 4, YELLOW, TRUE);
            for (int x = 10; x < 640; x += 40) {
                if (x > sx1 - 10 && x < sx2 + 10) continue;
                DrawTriangle(x, floorY - 14, x - 8, floorY - 2, x + 8, floorY - 2, YELLOW, FALSE);
            }
        }
        char buf[32];
        sprintf_s(buf, "感電まで %d", g_gimmick.hazardCountdown);
        DrawExtendString(232, 120, 2.0, 2.0, buf, (g_frameCount / 8) % 2 ? YELLOW : RED);
        DrawString(150, 158, "ジャンプ・ガード・緑の安全地帯・コンテナ破壊で回避！", WHITE);
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
    // 安全地帯（ゴムマット）
    DrawBox(sx1, floorY - 2, sx2, floorY + 4, GREEN, TRUE);
    DrawString((sx1 + sx2) / 2 - 16, floorY - 20, "SAFE", GREEN);
}

static void DrawGimmick(int stageIdx) {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;

    DrawTraps();
    if (!g_gimmickEnabled) return;

    if (stageIdx == 1) DrawHazard();

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
    if (stageIdx == 1) DrawString(x - 8, y - 120, "電", YELLOW);

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
        else DrawPole(sx, sy, f.dir, 0.7f, 180, false);
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
    if (f.state == ACT_STUN) col = (rand() % 2 == 0) ? YELLOW : GRAY;
    if (f.state == ACT_HIT || f.state == ACT_AIR_HIT) col = WHITE;
    if (f.state == ACT_SUPER_ENHANCED || f.state == ACT_SUPER_AIR) col = (rand() % 2 == 0) ? RED : YELLOW;
    if (f.state == ACT_BOAR_CHARGE) col = (rand() % 2 == 0) ? ORANGE : YELLOW;
    if (f.state == ACT_COUNTER_STRIKE) col = WHITE;

    int bx = (int)f.x + ox;
    int floorY = (int)f.y + FLOOR_DRAW_OFF + oy;   // 足元を画面の床の線に合わせる

    int bw = (c == 4) ? 24 : 14;
    int bh = (c == 4) ? 28 : 22;
    bool crouchPose = f.crouching && (f.state == ACT_IDLE || f.state == ACT_GUARD);
    if (crouchPose) bh = bh * 6 / 10;
    int by = floorY - bh;

    // 立ち状態は画像があれば画像で描く（通常64px / 大前128px の高さに合わせる）
    int img = g_charaGraph[c];
    bool useImage = (img != -1 && f.state == ACT_IDLE && !crouchPose);
    int imgH = (c == 4) ? 128 : 64;
    double imgScale = 1.0;
    if (useImage) {
        int gw = 0, gh = 0;
        GetGraphSize(img, &gw, &gh);
        if (gh > 0) imgScale = (double)imgH / (double)gh;
    }

    int markerBaseY = by - bh - 24;
    if (useImage && floorY - imgH - 12 < markerBaseY) markerBaseY = floorY - imgH - 12;
    if (isYou) {
        DrawString(bx - 10, markerBaseY - 14, "YOU", YELLOW);
        DrawTriangle(bx - 6, markerBaseY - 2, bx + 6, markerBaseY - 2, bx, markerBaseY + 6, YELLOW, TRUE);
        DrawTriangle(bx - 6, markerBaseY - 2, bx + 6, markerBaseY - 2, bx, markerBaseY + 6, WHITE, FALSE);
    }
    else {
        const char* targetLabel = g_is_net_battle ? "2P" : ((g_mode == MODE_TRAINING || g_mode == MODE_TUTORIAL) ? "DUMMY" : "COM");
        unsigned int markerCol = g_is_net_battle ? BLUE : RED;
        int lw = GetDrawStringWidth(targetLabel, (int)strlen(targetLabel));
        DrawString(bx - lw / 2, markerBaseY - 14, targetLabel, markerCol);
        DrawTriangle(bx - 6, markerBaseY - 2, bx + 6, markerBaseY - 2, bx, markerBaseY + 6, markerCol, TRUE);
        DrawTriangle(bx - 6, markerBaseY - 2, bx + 6, markerBaseY - 2, bx, markerBaseY + 6, WHITE, FALSE);
    }

    // いのししのサイコロと目押しバー
    if (f.state == ACT_BOAR_ROLL || f.diceShowTimer > 0) {
        int dx = bx, dy = by - bh - 72;
        DrawBox(dx - 12, dy - 12, dx + 12, dy + 12, WHITE, TRUE);
        DrawBox(dx - 12, dy - 12, dx + 12, dy + 12, (f.diceFace <= 2) ? RED : BLACK, FALSE);
        DrawFormatString(dx - 4, dy - 8, (f.diceFace <= 2) ? RED : BLACK, "%d", f.diceFace);
        if (f.state == ACT_BOAR_ROLL && !f.boarSuper) {
            int barX = dx - 40, barY = dy + 16, barW = 80;
            DrawBox(barX, barY, barX + barW, barY + 6, GetColor(40, 40, 40), TRUE);
            int gz1 = barX + barW * BOAR_TIMING_START / BOAR_ROLL_FRAMES;
            int gz2 = barX + barW * (BOAR_TIMING_END + 1) / BOAR_ROLL_FRAMES;
            DrawBox(gz1, barY, gz2, barY + 6, GREEN, TRUE);
            int cur = barX + barW * ClampI(f.animTimer, 0, BOAR_ROLL_FRAMES) / BOAR_ROLL_FRAMES;
            DrawLine(cur, barY - 3, cur, barY + 9, WHITE, 2);
            DrawBox(barX, barY, barX + barW, barY + 6, WHITE, FALSE);
        }
    }

    // 無敵中は半透明
    if (f.invulFrames > 0 && (f.state == ACT_DODGE || (g_frameCount / 2) % 2 == 0)) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 110);
    }

    if (useImage) {
        // 画像は右向きで描かれている前提。左を向いているときは左右反転する
        DrawRotaGraph(bx, floorY - imgH / 2, imgScale, 0.0, img, TRUE, (f.dir < 0) ? TRUE : FALSE);
        if (c == 8 && (g_frameCount / 40) % 3 == 0) DrawString(bx + 14, floorY - imgH - 4, "ﾆﾔﾆﾔ", CYAN);
        return;
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
        if (f.state == ACT_LIGHT1 || f.state == ACT_LIGHT2) {
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
        if (f.state == ACT_LIGHT1 || f.state == ACT_LIGHT2 || f.state == ACT_AIR_LIGHT) {
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
        // ガード中の盾エフェクト（ガード硬直中は明るく光る）
        int sx = bx + f.dir * (bw + 8);
        int pulse = (int)(sinf((float)g_frameCount * 0.3f) * 2.0f);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, f.guardStun > 0 ? 200 : 110);
        DrawOval(sx, by, 10 + pulse, bh + 12, f.guardStun > 0 ? WHITE : CYAN, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawOval(sx, by, 10 + pulse, bh + 12, BLUE, FALSE);
        DrawOval(sx, by, 13 + pulse, bh + 15, CYAN, FALSE);
        DrawString(bx - 20, by - bh - 44, "GUARD", CYAN);
    }

    if (f.state == ACT_STUN) {
        DrawString(bx - 20, by - 65, "*PIYO*", YELLOW);
    }
}

static void DrawFighterObj(const Fighter& f, bool isYou) {
    DrawFighterBody(f, isYou);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

// 当たり判定の表示（トレーニング F1）
static void DrawHitboxes() {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY + VIS_OFF_Y;
    const Fighter* fs[2] = { &g_me, &g_other };
    for (int i = 0; i < 2; i++) {
        const Fighter& f = *fs[i];
        const Fighter& opp = *fs[1 - i];
        float cx, cy, w, h;
        GetHurtBox(f, cx, cy, w, h);
        DrawBox((int)(cx - w / 2) + ox, (int)(cy - h / 2) + oy, (int)(cx + w / 2) + ox, (int)(cy + h / 2) + oy,
            f.invulFrames > 0 ? WHITE : BLUE, FALSE);

        if (f.state == ACT_THROW) {
            FrameData fd = GetFrameData(f.charaIndex, f.state);
            bool on = f.animTimer >= fd.startup && f.animTimer < fd.startup + fd.active;
            DrawBox((int)f.x - 45 + ox, (int)f.y - 35 + oy, (int)f.x + 45 + ox, (int)f.y + 35 + oy, on ? YELLOW : GetColor(120, 100, 30), FALSE);
            continue;
        }

        AttackBox b;
        if (!GetAttackBox(f, fabsf(f.x - opp.x), b)) continue;
        FrameData fd = GetFrameData(f.charaIndex, f.state);
        bool active = f.animTimer >= fd.startup && f.animTimer < fd.startup + fd.active;
        int x1 = (int)(f.x + b.offX - b.w / 2) + ox, y1 = (int)(f.y + b.offY - b.h / 2) + oy;
        int x2 = (int)(f.x + b.offX + b.w / 2) + ox, y2 = (int)(f.y + b.offY + b.h / 2) + oy;
        if (active && !f.hasHit) {
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
        if (b & IN_SPACE) strcat_s(buf, "_SP");
        if (b & IN_DODGE) strcat_s(buf, "_回");
        int w = GetDrawStringWidth(buf, (int)strlen(buf));
        char fr[8];
        sprintf_s(fr, "%d", e.frames > 99 ? 99 : e.frames);
        int fw = GetDrawStringWidth(fr, (int)strlen(fr));
        if (x + w + fw + 10 > 632) {
            if (y > 450) break;
            x = 8;
            y += 18;
        }
        unsigned int col = (i == 0) ? YELLOW : ((b & (IN_J | IN_K | IN_L | IN_SPACE | IN_DODGE)) ? WHITE : GRAY);
        DrawBox(x - 2, y - 1, x + w + fw + 6, y + 16, GetColor(10, 10, 20), TRUE);
        DrawString(x, y, buf, col);
        DrawString(x + w + 3, y, fr, GRAY);
        x += w + fw + 12;
    }
}

static void DrawHud(HudMode hud) {
    int ox = g_shakeOffsetX;
    int oy = g_shakeOffsetY;
    bool isNet = (hud == HUD_NET);
    bool practice = (hud == HUD_TRAINING || hud == HUD_TUTORIAL);

    int stage = ClampI(g_slots[g_currentSlot].stageProgress, 0, LAST_STAGE);

    // 1P: 大型HPゲージ
    DrawBox(30, 20, 280, 44, GetColor(50, 15, 15), TRUE);
    float myHpR = (float)g_me.hp / (float)g_me.maxHp;
    if (myHpR < 0) myHpR = 0;
    unsigned int hpCol1 = (myHpR < 0.25f) ? RED : ((myHpR < 0.5f) ? YELLOW : GREEN);
    DrawBox(30, 20, 30 + (int)(250.0f * myHpR), 44, hpCol1, TRUE);
    DrawBox(30, 20, 280, 44, WHITE, FALSE);
    if (hud == HUD_NET) {
        DrawFormatString(32, 5, WHITE, "1P: YOU [%s]", CHARA_PROFILES[g_me.charaIndex].charaId);
    }
    else if (practice) {
        DrawFormatString(32, 5, WHITE, (hud == HUD_TRAINING) ? "TRAINING: %s" : "TUTORIAL: %s", CHARA_PROFILES[g_me.charaIndex].charaId);
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
    if (hud == HUD_NET) {
        DrawFormatString(450, 5, WHITE, "2P: REMOTE [%s]", CHARA_PROFILES[g_other.charaIndex].charaId);
    }
    else if (practice) {
        DrawFormatString(400, 5, WHITE, "DUMMY: %s", CHARA_PROFILES[g_other.charaIndex].charaId);
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
    bool infiniteTime = practice || (isNet && g_netRules.timeLimit == 0);
    DrawBox(292, 16, 348, 52, (!infiniteTime && g_roundTime <= 10) ? RED : YELLOW, FALSE);
    if (infiniteTime) DrawString(312, 24, "∞", YELLOW);
    else DrawFormatString(304, 24, (g_roundTime <= 10) ? RED : YELLOW, "%02d", g_roundTime);

    // 周回・難易度
    if (hud == HUD_STORY) {
        int hard = GetHardLevel();
        DrawFormatString(288, 60, hard > 0 ? ORANGE : GRAY, "%d周目", GetCurrentLap());
        if (hard > 0) DrawFormatString(286, 76, RED, "HARD+%d", hard);
        if (g_cpuTilt > 0) {
            char bars[16] = "";
            for (int i = 0; i < g_cpuTilt && i < 6; i++) strcat_s(bars, "■");
            DrawFormatString(400, 62, RED, "COMイライラ %s", bars);
        }
    }

    // 通信対戦：取得ラウンド
    if (isNet) {
        int myIdx = g_is_host ? 0 : 1;
        for (int r = 0; r < g_netRules.roundsToWin; r++) {
            DrawCircle(250 - r * 16, 64, 6, (r < g_netRounds[myIdx]) ? YELLOW : GetColor(60, 60, 60), TRUE);
            DrawCircle(390 + r * 16, 64, 6, (r < g_netRounds[1 - myIdx]) ? YELLOW : GetColor(60, 60, 60), TRUE);
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
    if (!practice && g_me.comboCount > 1 && g_me.comboDisplayTimer > 0) {
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
        DrawInputHistory(444);
    }
    else {
        DrawString(25, 444, "[A/D]移動 [AA/DD]ダッシュ [W]跳 [空中SS]急降下 [I]回避 [TAB]技表", WHITE);
        DrawString(25, 462, "[J/K/L]攻撃 [前+J]投げ [J+K]超必 [SPACE]長押しガード/短押しパリィ", WHITE);
    }

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

// トレーニングの情報パネル
static void DrawTrainingOverlay() {
    DrawString(8, 62, "F1判定 F2フレーム F3入力 F4SP無限 F5リセット F6課題 F7画面端 TAB技表", GRAY);
    DrawFormatString(400, 80, YELLOW, "ダミー: [%d]%s", (int)g_dummyMode + 1, DUMMY_MODE_NAMES[g_dummyMode]);
    if (g_infiniteSp) DrawString(400, 98, "SP・スタミナ無限 ON", CYAN);

    if (g_showFrames) {
        DrawBox(6, 80, 336, 156, GetColor(0, 0, 0), TRUE);
        DrawBox(6, 80, 336, 156, GRAY, FALSE);
        const MoveInfo& m = g_moveInfo[0];
        if (m.valid) {
            DrawFormatString(12, 84, WHITE, "%s", GetMoveName(m.chara, m.state));
            DrawFormatString(12, 102, CYAN, "発生%dF 持続%dF 硬直%dF (全体%dF)", m.startup, m.active, m.recovery, m.total);
        }
        else {
            DrawString(12, 84, "技を出すとフレームを表示", GRAY);
        }
        if (g_adv.valid) {
            const char* who = g_adv.byDummy ? "ダミーの技" : "自分の技";
            if (g_adv.launch && !g_adv.guarded) {
                DrawFormatString(12, 120, GREEN, "%s: 打ち上げ・吹っ飛ばし（追撃可能）", who);
            }
            else {
                DrawFormatString(12, 120, g_adv.adv >= 0 ? GREEN : RED, "%s %s時: %+dF",
                    who, g_adv.guarded ? "ガード" : "ヒット", g_adv.adv);
            }
            if (g_adv.punishable) {
                DrawString(12, 138, g_adv.byDummy ? "確定反撃チャンス！ 最速技で反撃できる" : "確定反撃あり！ 相手の最速技で反撃される", (g_frameCount / 8) % 2 ? YELLOW : RED);
            }
        }
    }

    // コンボ情報と課題
    DrawBox(340, 116, 634, 196, GetColor(0, 0, 0), TRUE);
    DrawBox(340, 116, 634, 196, GRAY, FALSE);
    if (g_comboHits > 0) DrawFormatString(346, 120, YELLOW, "コンボ: %dHit %dダメージ", g_comboHits, g_comboDmg);
    else DrawFormatString(346, 120, WHITE, "前回: %dHit %dダメージ", g_lastComboHits, g_lastComboDmg);
    DrawFormatString(346, 138, ORANGE, "最大: %dHit / %dダメージ", g_maxComboHits, g_maxComboDmg);
    const ComboChallenge& ch = CHALLENGES[g_me.charaIndex][g_challengeIdx];
    DrawFormatString(346, 158, g_challengeDone[g_challengeIdx] ? GREEN : CYAN, "課題%d/3%s %s",
        g_challengeIdx + 1, g_challengeDone[g_challengeIdx] ? "[済]" : "", ch.desc);
    if (ch.hint[0]) DrawFormatString(346, 176, GRAY, "%s", ch.hint);
}

// コマンドリスト（TAB）
static void DrawCommandList(bool training) {
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 225);
    DrawBox(24, 30, 616, 450, GetColor(5, 8, 18), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(24, 30, 616, 450, YELLOW, FALSE);

    int pageCount = training ? 3 : 2;
    int page = ClampI(g_cmdListPage, 0, pageCount - 1);
    const char* pageName[3] = { "技表・コンボ", "共通システム", "トレーニング操作" };
    DrawFormatString(40, 40, YELLOW, "コマンドリスト  [%s]  %d/%d", pageName[page], page + 1, pageCount);
    DrawString(40, 424, "[A/D] キャラ切替   [W/S] ページ切替   [TAB] 閉じる", GRAY);

    int y = 72;
    if (page == 0) {
        int c = g_cmdListChara;
        DrawFormatString(40, y, WHITE, "%s  %s", CHARA_PROFILES[c].charaId, CHARA_PROFILES[c].archetype);
        y += 26;
        for (int i = 0; i < CMD_LINES && CMD_LIST[c][i]; i++) {
            DrawString(48, y, CMD_LIST[c][i], WHITE);
            y += 20;
        }
        y += 8;
        DrawString(40, y, "▼ おすすめコンボ（トレーニングの課題）", ORANGE);
        y += 22;
        for (int i = 0; i < 3; i++) {
            const ComboChallenge& ch = CHALLENGES[c][i];
            DrawFormatString(48, y, CYAN, "%d. %s", i + 1, ch.desc);
            if (ch.hint[0]) DrawFormatString(300, y, GRAY, "%s", ch.hint);
            y += 20;
        }
    }
    else {
        const char** lines = (page == 1) ? CMD_COMMON : CMD_TRAINING;
        for (int i = 0; lines[i]; i++) {
            DrawString(48, y, lines[i], WHITE);
            y += 24;
        }
    }
}

// TABでコマンドリストを開閉。開いている間のキー操作も処理する
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

static void DrawStatsBlock(int x, int y, int idx, int chara, unsigned int col) {
    const MatchStats& s = g_stats[idx];
    DrawFormatString(x, y, col, "使用技 %d回（最多: %s）", s.moves, MostUsedMove(idx, chara));
    DrawFormatString(x, y + 20, col, "ヒット %d / 被弾 %d / 与ダメージ %d", s.hits, s.taken, s.damage);
    DrawFormatString(x, y + 40, col, "ガード %d / パリィ %d / 最大コンボ %d", s.guards, s.parries, s.maxCombo);
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

static void DrawStageBackground(int stageIdx) {
    int oy = g_shakeOffsetY;
    if (g_kokusenStrobe > 0) {
        // 黒閃・空間反転ストロボ演出（画面全体が漆黒と真紅に反転）
        DrawBox(0, 0, 640, 480, (g_kokusenStrobe % 4 < 2) ? GetColor(0, 0, 0) : GetColor(180, 10, 10), TRUE);
        DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, GetColor(10, 10, 10), TRUE);
        DrawLine(0, (int)GROUND_Y + 25 + oy, 640, (int)GROUND_Y + 25 + oy, RED);
    }
    else {
        DrawBox(0, 0, 640, 480, STAGES[stageIdx].skyColor, TRUE);
        DrawBox(0, (int)GROUND_Y + 25 + oy, 640, 480, STAGES[stageIdx].groundColor, TRUE);
        DrawLine(0, (int)GROUND_Y + 25 + oy, 640, (int)GROUND_Y + 25 + oy, WHITE);
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
    g_cmdListOpen = false;
    g_mode = MODE_CHARA_SELECT;
}

// ----------------------------------------------------
// トレーニングモード
// ----------------------------------------------------
static void ResetTrainingPositions(bool corner) {
    float myX = corner ? 470.0f : 200.0f;
    float dmX = corner ? 590.0f : 440.0f;
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
}

static void SetupTraining() {
    g_is_net_battle = false;
    g_currentStageIdx = g_trainStage;
    g_gimmickEnabled = true;
    InitStageGimmick(g_trainStage);
    ClearBattleObjects();
    ResetFighter(g_me, 200.0f, 1, g_trainMyChara, PlayableMaxHp(g_trainMyChara));
    ResetFighter(g_other, 440.0f, -1, g_trainDummyChara, CHARA_PROFILES[g_trainDummyChara].maxHp);
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
    static const int numKeys[6] = { KEY_INPUT_1, KEY_INPUT_2, KEY_INPUT_3, KEY_INPUT_4, KEY_INPUT_5, KEY_INPUT_6 };
    static const int padKeys[6] = { KEY_INPUT_NUMPAD1, KEY_INPUT_NUMPAD2, KEY_INPUT_NUMPAD3, KEY_INPUT_NUMPAD4, KEY_INPUT_NUMPAD5, KEY_INPUT_NUMPAD6 };
    for (int i = 0; i < 6; i++) {
        if (KeyHit(numKeys[i]) || KeyHit(padKeys[i])) {
            g_dummyMode = (DummyMode)i;
            if (g_other.state == ACT_GUARD) { g_other.state = ACT_IDLE; g_other.stateTimer = 0; }
            SpawnPopup(g_other.x, g_other.y - 110, DUMMY_MODE_NAMES[i], YELLOW);
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
    UpdateTraps();
    UpdateStageGimmick(g_currentStageIdx);
    UpdateShots();
    UpdateEffects();

    if (g_infiniteSp) {
        g_me.spGauge = 100;
        g_me.stamina = g_me.maxStamina;
    }

    // コンボの区切り：ダミーが自由に動ける状態に戻ったら確定
    bool dummyNeutral = !IsHitState(g_other.state) && g_other.state != ACT_SLIP &&
        g_other.state != ACT_STUN && g_other.hitStop == 0;
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

    bool meNeutral = !IsHitState(g_me.state) && g_me.state != ACT_SLIP && g_me.state != ACT_STUN;
    g_meNeutralFrames = meNeutral ? g_meNeutralFrames + 1 : 0;
    if (g_meNeutralFrames > 45) g_me.hp = g_me.maxHp;
    if (g_me.hp <= 0) g_me.hp = 1;
}

// ----------------------------------------------------
// チュートリアル（3章構成。クリアするまで他のモードは遊べない）
// ----------------------------------------------------
enum TutKind { TK_MOVE, TK_JUMP, TK_HIT, TK_GUARD, TK_PARRY, TK_PARRY_HIT, TK_DASH, TK_AIR, TK_DODGE, TK_BURST };
enum TutDummy { TD_STOP, TD_GUARD, TD_ATTACK, TD_COMBO };
enum TutPhase { TP_INTRO, TP_PLAY, TP_STEP_CLEAR, TP_CHAPTER_CLEAR, TP_ALL_CLEAR };
static const int TUT_HIT_ANY_J = -1;
static const int TUT_HIT_SUPER = -3;

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
    { "通常技 J", "J は出の速い通常技。ダミーに3回当てよう", "近づいて J。当たった直後にもう一度 J で2段目が出る", TK_HIT, TUT_HIT_ANY_J, 3, TD_STOP, false },
    { "中距離技 K", "K は中距離技。大乗殿は気弾を撃つ", "少し離れた位置から K を当てよう", TK_HIT, TAG_SHOT, 1, TD_STOP, false },
    { "戦技 L", "L は戦技。大乗殿は昇天脚で相手を打ち上げる", "近づいて L を当てよう（跳んでくる相手の迎撃にも使える）", TK_HIT, ACT_SKILL_ATTACK, 1, TD_STOP, false },
    { "ガード", "相手の攻撃は SPACE 長押しでガード", "ダミーの頭に「！」が出たら SPACE を押しっぱなし。3回ガード", TK_GUARD, 0, 3, TD_ATTACK, false },
    // 第2章：投げとパリィ
    { "投げ", "ガードを固める相手には投げが有効", "密着して 前(相手の方向) + J。投げを1回決めよう", TK_HIT, ACT_THROW, 1, TD_GUARD, false },
    { "パリィ", "攻撃が当たる瞬間に SPACE を短く押すとパリィ", "「！」が消えた直後が狙い目。2回成功させよう", TK_PARRY, 0, 2, TD_ATTACK, false },
    { "パリィから反撃", "パリィ成功で相手はピヨり、次の一撃が1.5倍になる", "パリィ → すぐ近づいて J を当てよう", TK_PARRY_HIT, 0, 1, TD_ATTACK, false },
    // 第3章：ダッシュ・回避・バースト
    { "ダッシュ", "同じ方向キーを素早く2回押すとダッシュ", "DD と AA をそれぞれ1回ずつ出そう", TK_DASH, 0, 2, TD_STOP, false },
    { "空中アクション", "ジャンプ中に W でもう一度ジャンプ（2段ジャンプ）", "ジャンプ中に S を素早く2回で急降下。両方やってみよう", TK_AIR, 0, 2, TD_STOP, false },
    { "回避", "I（または左Shift）で無敵ロール。攻撃をすり抜ける", "ダミーの「！」を見てから回避！ 2回かわそう", TK_DODGE, 0, 2, TD_ATTACK, false },
    { "バースト", "連続で殴られている最中に SPACE でコンボから抜け出せる", "SPを50使う（今回は満タン）。殴られたら SPACE！", TK_BURST, 0, 1, TD_COMBO, true },
    { "超必殺技", "SPが100溜まったら J+K 同時押しで超必殺技", "近づいて超必殺技を当てよう（今回はSP満タン）", TK_HIT, TUT_HIT_SUPER, 1, TD_STOP, true },
};
static const int TUT_CHAPTER_COUNT = 3;
static const int TUT_CHAPTER_START[TUT_CHAPTER_COUNT + 1] = { 0, 6, 9, sizeof(TUT_STEPS) / sizeof(TUT_STEPS[0]) };
static const char* TUT_CHAPTER_NAMES[TUT_CHAPTER_COUNT] = { "基本操作", "投げとパリィ", "ダッシュ・回避・バースト" };
static const char* TUT_CHAPTER_DESC[TUT_CHAPTER_COUNT] = {
    "まずは「歩く・跳ぶ・攻撃・ガード」の4つを覚えよう",
    "守りを崩す「投げ」と、攻撃を弾き返す「パリィ」",
    "素早い移動と、ピンチから抜け出す「バースト」"
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
    int attackDelay;      // ダミーの攻撃予告（「！」が出ている残りフレーム）
    int attackCooldown;
};
static TutorialState g_tut;
static int g_tutorialChapters = 0;   // クリアした章の数（tutorial.dat に保存）
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
    g_tut.attackDelay = 0;
    g_tut.attackCooldown = 60;
    ResetFighter(g_me, 200.0f, 1, 0, PlayableMaxHp(0));
    ResetFighter(g_other, 440.0f, -1, 0, CHARA_PROFILES[0].maxHp);
    g_tut.lastX = g_me.x;
    for (int i = 0; i < MAX_SHOTS; i++) g_shots[i].active = false;
    ResetControl(g_ctrl[0]);
    g_readyFightTimer = 80;
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
        SpawnPopup(g_me.x, g_me.y - 110, "いいね！", GREEN);
    }
}

static bool IsSuperState(int s) {
    return s == ACT_SUPER_ATTACK || s == ACT_SUPER_AIR || s == ACT_SUPER_ENHANCED;
}

static void TutorialEvent(int ev, int param) {
    if (g_mode != MODE_TUTORIAL || g_tut.phase != TP_PLAY) return;
    const TutStep& s = TUT_STEPS[g_tut.step];
    bool isJ = (param == ACT_LIGHT1 || param == ACT_LIGHT2);

    switch (s.kind) {
    case TK_JUMP:
        if (ev == TE_JUMP) TutorialProgress();
        break;
    case TK_HIT:
        if (ev != TE_HIT) break;
        if (s.param == TUT_HIT_ANY_J) { if (isJ) TutorialProgress(); }
        else if (s.param == TUT_HIT_SUPER) { if (IsSuperState(param)) TutorialProgress(); }
        else if (param == s.param) TutorialProgress();
        break;
    case TK_GUARD:
        if (ev == TE_GUARD) TutorialProgress();
        break;
    case TK_PARRY:
        if (ev == TE_PARRY) TutorialProgress();
        break;
    case TK_PARRY_HIT:
        if (ev == TE_PARRY) {
            g_tut.parryTimer = 90;
            SpawnPopup(g_me.x, g_me.y - 120, "今だ！ J で反撃！", YELLOW);
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
        if (g_tut.attackDelay > 0 || IsAttackState(g_other.state)) TutorialProgress();
        else SpawnPopup(g_me.x, g_me.y - 110, "「！」が出てから回避しよう", GRAY);
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
        }
        break;
    case TD_ATTACK:
    case TD_COMBO:
        if (freeGuard) d.state = ACT_IDLE;
        if (g_tut.attackCooldown > 0) g_tut.attackCooldown--;
        if (d.state != ACT_IDLE || !d.isGrounded) break;
        if (g_tut.attackDelay > 0) {
            d.vx = 0;
            if (--g_tut.attackDelay == 0) {
                StartAttack(d, g_me, 0);
                g_tut.attackCooldown = 70;
            }
            break;
        }
        if (dist > 70.0f) {
            d.vx = (float)d.dir * 2.5f;
            break;
        }
        if (g_tut.attackCooldown == 0) {
            if (s.dummy == TD_COMBO) {
                StartAttack(d, g_me, 0);
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
        UpdateShots();
        UpdateEffects();

        if (s.fullSp) g_me.spGauge = 100;
        g_me.stamina = g_me.maxStamina;   // チュートリアル中はスタミナ切れで困らないように

        if (s.kind == TK_MOVE && g_tut.phase == TP_PLAY) {
            float dx = g_me.x - g_tut.lastX;
            if (g_me.isGrounded && (g_me.state == ACT_IDLE || g_me.state == ACT_DASH)) {
                if (dx > 0) g_tut.movedR += dx;
                else g_tut.movedL -= dx;
            }
            if (!g_tut.flagA && g_tut.movedR >= 60.0f) { g_tut.flagA = true; TutorialProgress(); }
            if (g_tut.phase == TP_PLAY && !g_tut.flagB && g_tut.movedL >= 60.0f) { g_tut.flagB = true; TutorialProgress(); }
        }
        g_tut.lastX = g_me.x;

        // 体力は減っても自動で戻る（負けて終わることはない）
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
        int dx = (int)g_other.x + g_shakeOffsetX;
        int dy = (int)g_other.y - 70 + g_shakeOffsetY;
        DrawCircle(dx, dy + 12, 10 + g_tut.attackDelay * 2, YELLOW, FALSE, 2);
        DrawExtendString(dx - 16, dy - 4, 2.0, 2.0, "！", RED);
    }

    // 指示パネル
    DrawBox(20, 64, 620, 166, GetColor(5, 10, 25), TRUE);
    DrawBox(20, 64, 620, 166, YELLOW, FALSE);
    DrawFormatString(30, 70, YELLOW, "第%d章 %s   STEP %d/%d", g_tut.chapter + 1, TUT_CHAPTER_NAMES[g_tut.chapter],
        g_tut.step - chapterStart + 1, stepsInChapter);
    DrawString(450, 70, "ESC:やめる TAB:技表", GRAY);
    DrawFormatString(30, 94, ORANGE, "【%s】", s.title);
    DrawFormatString(470, 94, WHITE, "%d / %d", g_tut.progress, s.target);
    DrawBox(530, 98, 610, 108, GetColor(40, 40, 40), TRUE);
    DrawBox(530, 98, 530 + 80 * g_tut.progress / s.target, 108, GREEN, TRUE);
    DrawString(30, 118, s.line1, WHITE);
    DrawString(30, 140, s.line2, CYAN);

    // 段階ごとの全画面表示
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
        DrawString(150, 300, "[Enter] 次の章へ    [Esc] タイトルへ（続きから再開できます）", GRAY);
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

// ----------------------------------------------------
// 通信対戦（高レベル）
// ----------------------------------------------------
static void SetupNetRound() {
    g_currentStageIdx = g_netRules.stage;
    g_gimmickEnabled = (g_netRules.gimmickOn != 0);
    InitStageGimmick(g_netRules.stage);
    ClearBattleObjects();
    int charaA = g_is_host ? g_myNetChara : g_enemyNetChara;
    int charaB = g_is_host ? g_enemyNetChara : g_myNetChara;
    // ホスト視点：g_me = A（ホスト） / g_other = B（クライアント）
    Fighter& fa = g_is_host ? g_me : g_other;
    Fighter& fb = g_is_host ? g_other : g_me;
    ResetFighter(fa, 160.0f, 1, charaA, PlayableMaxHp(charaA));
    ResetFighter(fb, 480.0f, -1, charaB, PlayableMaxHp(charaB));
    ResetBattleGlobals();
    g_roundTime = (g_netRules.timeLimit == 0) ? 99 : g_netRules.timeLimit;
    g_netPhase = NP_READY;
    g_netPhaseTimer = 0;
    g_remoteQueueCount = 0;
    g_remoteBits = 0;
}

static void SetupNetMatch() {
    g_netRounds[0] = g_netRounds[1] = 0;
    g_netRoundNo = 1;
    g_netWinner = 0;
    g_netGotSnapshot = false;
    g_netFrame = 0;
    ResetMatchStats();
    SetupNetRound();
}

static void PackFighter(NetFighter& n, const Fighter& f) {
    n.x = f.x; n.y = f.y; n.vx = f.vx; n.vy = f.vy;
    n.hp = (short)ClampI(f.hp, -30000, 30000);
    n.maxHp = (short)f.maxHp;
    n.state = (unsigned char)f.state;
    n.dir = (signed char)f.dir;
    n.chara = (unsigned char)f.charaIndex;
    n.flags = (unsigned char)((f.isGrounded ? 1 : 0) | (f.invulFrames > 0 ? 2 : 0) | (f.crouching ? 4 : 0) |
        (f.boarSuper ? 8 : 0) | (f.hasHit ? 16 : 0) | (f.whiffChecked ? 32 : 0) | (f.wallBounceUsed ? 64 : 0));
    n.animTimer = (short)ClampI(f.animTimer, 0, 30000);
    n.stateTimer = (short)ClampI(f.stateTimer, 0, 30000);
    n.sp = (unsigned char)ClampI(f.spGauge, 0, 255);
    n.stamina = (unsigned char)ClampI((int)f.stamina, 0, 255);
    n.diceFace = (unsigned char)f.diceFace;
    n.diceShowTimer = (unsigned char)ClampI(f.diceShowTimer, 0, 255);
    n.stanceLevel = (unsigned char)f.stanceLevel;
    n.comboCount = (unsigned char)ClampI(f.comboCount, 0, 255);
    n.comboDisplayTimer = (unsigned char)ClampI(f.comboDisplayTimer, 0, 255);
    n.moveDir = (signed char)f.moveDir;
    n.boarTiming = (unsigned char)f.boarTiming;
    n.guardStun = (unsigned char)ClampI(f.guardStun, 0, 255);
}

static void UnpackFighter(Fighter& f, const NetFighter& n) {
    f.x = n.x; f.y = n.y; f.vx = n.vx; f.vy = n.vy;
    f.hp = n.hp; f.maxHp = (n.maxHp > 0) ? n.maxHp : 1;
    f.state = (ActionState)ClampI(n.state, 0, ACT_STATE_COUNT - 1);
    f.dir = n.dir;
    f.charaIndex = ClampI(n.chara, 0, CHARA_COUNT - 1);
    f.isGrounded = (n.flags & 1) != 0;
    f.invulFrames = (n.flags & 2) ? 2 : 0;
    f.crouching = (n.flags & 4) != 0;
    f.boarSuper = (n.flags & 8) != 0;
    f.hasHit = (n.flags & 16) != 0;
    f.whiffChecked = (n.flags & 32) != 0;
    f.wallBounceUsed = (n.flags & 64) != 0;
    f.animTimer = n.animTimer;
    f.stateTimer = n.stateTimer;
    f.spGauge = n.sp;
    f.stamina = (float)n.stamina;
    f.maxStamina = 100.0f;
    f.diceFace = n.diceFace;
    f.diceShowTimer = n.diceShowTimer;
    f.stanceLevel = n.stanceLevel;
    f.comboCount = n.comboCount;
    f.comboDisplayTimer = n.comboDisplayTimer;
    f.moveDir = n.moveDir;
    f.boarTiming = n.boarTiming;
    f.guardStun = n.guardStun;
}

static void PackStats(NetStats& n, const MatchStats& s) {
    n.moves = (short)s.moves; n.hits = (short)s.hits; n.taken = (short)s.taken;
    n.guards = (short)s.guards; n.parries = (short)s.parries; n.maxCombo = (short)s.maxCombo;
    n.damage = s.damage;
}

static void UnpackStats(MatchStats& s, const NetStats& n) {
    s.moves = n.moves; s.hits = n.hits; s.taken = n.taken;
    s.guards = n.guards; s.parries = n.parries; s.maxCombo = n.maxCombo;
    s.damage = n.damage;
}

static void SendSnapshot() {
    static NetSnapshot s;
    memset(&s, 0, sizeof(s));
    s.frame = ++g_netFrame;
    PackFighter(s.a, g_me);
    PackFighter(s.b, g_other);
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!g_shots[i].active) continue;
        NetShot& ns = s.shots[s.shotCount++];
        ns.x = g_shots[i].x; ns.y = g_shots[i].y; ns.color = g_shots[i].color;
        ns.type = (unsigned char)g_shots[i].projType; ns.sub = (unsigned char)g_shots[i].subType;
        ns.radius = (unsigned char)g_shots[i].radius; ns.fromA = g_shots[i].isFromMe ? 1 : 0;
    }
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (!g_traps[i].active) continue;
        NetTrap& nt = s.traps[s.trapCount++];
        nt.x = g_traps[i].x; nt.type = (unsigned char)g_traps[i].type; nt.ownerA = g_traps[i].ownerIsMe ? 1 : 0;
        nt.life = (short)g_traps[i].life; nt.armDelay = (short)g_traps[i].armDelay;
    }
    s.gx = g_gimmick.x; s.gFallAngle = g_gimmick.fallAngle; s.itemX = g_gimmick.itemX;
    s.ghp = (short)g_gimmick.hp; s.gmaxHp = (short)g_gimmick.maxHp;
    s.gFlags = (unsigned char)((g_gimmick.isFalling ? 1 : 0) | (g_gimmick.fallen ? 2 : 0) | (g_gimmick.itemActive ? 4 : 0) |
        (g_gimmick.hazardActive ? 8 : 0) | (g_gimmick.hazardWarning ? 16 : 0));
    s.gDir = (signed char)g_gimmick.activeHitBox;
    s.gStageTimer = g_gimmick.stageTimer;
    s.roundTime = (signed char)g_roundTime;
    s.readyTimer = (unsigned char)ClampI(g_readyFightTimer, 0, 255);
    s.phase = (unsigned char)g_netPhase;
    s.phaseTimer = (unsigned char)ClampI(g_netPhaseTimer, 0, 255);
    s.roundsA = (unsigned char)g_netRounds[0];
    s.roundsB = (unsigned char)g_netRounds[1];
    s.roundNo = (unsigned char)g_netRoundNo;
    s.winner = (unsigned char)g_netWinner;
    s.cutinTimer = (unsigned char)ClampI(g_superCutinTimer, 0, 255);
    s.cutinFlags = (unsigned char)((g_superIsUltimate ? 1 : 0) | (g_cutinIsCounter ? 2 : 0));
    strncpy_s(s.cutinText, g_superCutinName, _TRUNCATE);
    s.shake = (unsigned char)ClampI(g_screenShakeTimer, 0, 255);
    s.flash = (unsigned char)ClampI(g_flashTimer, 0, 255);
    s.strobe = (unsigned char)ClampI(g_kokusenStrobe, 0, 255);
    s.parryNotice = (unsigned char)ClampI(g_parryNoticeTimer, 0, 255);
    s.kokusen = g_isKokusenKoFinish ? 1 : 0;
    PackStats(s.statsA, g_stats[0]);
    PackStats(s.statsB, g_stats[1]);
    s.fxCount = (unsigned char)g_netFxCount;
    memcpy(s.fx, g_netFxQueue, sizeof(NetFxEvent) * g_netFxCount);
    s.popupCount = (unsigned char)g_netPopupCount;
    memcpy(s.popups, g_netPopupQueue, sizeof(NetPopupEvent) * g_netPopupCount);
    g_netFxCount = 0;
    g_netPopupCount = 0;
    SendPacket(PKT_SNAPSHOT, &s, sizeof(s));
}

// クライアント：ホストから届いた状態をそのまま表示に反映（自分 = B）
static void ApplySnapshot(const NetSnapshot& s) {
    UnpackFighter(g_me, s.b);
    UnpackFighter(g_other, s.a);
    for (int i = 0; i < MAX_SHOTS; i++) g_shots[i].active = false;
    for (int i = 0; i < s.shotCount && i < MAX_SHOTS; i++) {
        const NetShot& ns = s.shots[i];
        Projectile& p = g_shots[i];
        p.active = true; p.x = ns.x; p.y = ns.y; p.color = ns.color;
        p.projType = ns.type; p.subType = ns.sub; p.radius = ns.radius;
        p.isFromMe = (ns.fromA == 0);
        p.vx = p.vy = 0; p.damage = 0; p.life = 2;
    }
    for (int i = 0; i < MAX_TRAPS; i++) g_traps[i].active = false;
    for (int i = 0; i < s.trapCount && i < MAX_TRAPS; i++) {
        const NetTrap& nt = s.traps[i];
        Trap& t = g_traps[i];
        t.active = true; t.x = nt.x; t.type = nt.type; t.ownerIsMe = (nt.ownerA == 0);
        t.life = nt.life; t.armDelay = nt.armDelay;
    }
    g_gimmick.x = s.gx; g_gimmick.fallAngle = s.gFallAngle; g_gimmick.itemX = s.itemX; g_gimmick.itemY = GROUND_Y;
    g_gimmick.hp = s.ghp; g_gimmick.maxHp = (s.gmaxHp > 0) ? s.gmaxHp : 1;
    g_gimmick.isFalling = (s.gFlags & 1) != 0;
    g_gimmick.fallen = (s.gFlags & 2) != 0;
    g_gimmick.itemActive = (s.gFlags & 4) != 0;
    g_gimmick.hazardActive = (s.gFlags & 8) != 0;
    g_gimmick.hazardWarning = (s.gFlags & 16) != 0;
    g_gimmick.activeHitBox = s.gDir;
    g_gimmick.stageTimer = s.gStageTimer;
    if (g_gimmick.hazardWarning) {
        int phase = g_gimmick.stageTimer % HAZARD_PERIOD;
        g_gimmick.hazardCountdown = (HAZARD_ACTIVE_START - phase + 59) / 60;
    }
    g_roundTime = s.roundTime;
    g_readyFightTimer = s.readyTimer;
    g_netPhase = (NetPhase)s.phase;
    g_netPhaseTimer = s.phaseTimer;
    g_netRounds[0] = s.roundsA;
    g_netRounds[1] = s.roundsB;
    g_netRoundNo = s.roundNo;
    g_netWinner = s.winner;
    g_superCutinTimer = s.cutinTimer;
    g_superIsUltimate = (s.cutinFlags & 1) != 0;
    g_cutinIsCounter = (s.cutinFlags & 2) != 0;
    strncpy_s(g_netCutinText, s.cutinText, _TRUNCATE);
    g_superCutinName = g_netCutinText;
    g_screenShakeTimer = s.shake;
    g_flashTimer = s.flash;
    g_kokusenStrobe = s.strobe;
    g_parryNoticeTimer = s.parryNotice;
    g_isKokusenKoFinish = (s.kokusen != 0);
    UnpackStats(g_stats[0], s.statsB);
    UnpackStats(g_stats[1], s.statsA);
    for (int i = 0; i < s.fxCount && i < NET_MAX_FX; i++) {
        const NetFxEvent& e = s.fx[i];
        if (e.kind == 0) SpawnHitFX(e.x, e.y, e.color, e.count);
        else SpawnSparks(e.x, e.y, e.color, e.count, (float)e.len);
    }
    for (int i = 0; i < s.popupCount && i < NET_MAX_POPUPS; i++) {
        char text[49];
        memcpy(text, s.popups[i].text, 48);
        text[48] = '\0';
        SpawnPopup(s.popups[i].x, s.popups[i].y, text, s.popups[i].color);
    }
    g_netGotSnapshot = true;
}

static void ProcessPacket(unsigned char type, const char* data, int len) {
    switch (type) {
    case PKT_SETUP:
        if (len == (int)sizeof(NetSetup) && !g_is_host) {
            memcpy(&g_netRules, data, sizeof(NetSetup));
            g_netRules.stage = (unsigned char)ClampI(g_netRules.stage, 0, 4);
            g_netRules.roundsToWin = (unsigned char)ClampI(g_netRules.roundsToWin, 1, 3);
            g_netRulesReceived = true;
            g_myNetReady = false;
            g_enemyNetReady = false;
            g_mode = MODE_NET_CHARA_SELECT;
        }
        break;
    case PKT_CHARA:
        if (len == (int)sizeof(NetCharaMsg)) {
            g_enemyNetChara = ClampI(((const NetCharaMsg*)data)->chara, 0, CHARA_COUNT - 1);
            g_enemyNetReady = true;
        }
        break;
    case PKT_INPUT:
        if (len == (int)sizeof(NetInputMsg) && g_is_host) {
            if (g_remoteQueueCount >= 16) {
                memmove(g_remoteQueue, g_remoteQueue + 1, sizeof(unsigned short) * 15);
                g_remoteQueueCount = 15;
            }
            g_remoteQueue[g_remoteQueueCount++] = ((const NetInputMsg*)data)->bits;
        }
        break;
    case PKT_SNAPSHOT:
        if (len == (int)sizeof(NetSnapshot) && !g_is_host && g_mode == MODE_NET_BATTLE_PLAY) {
            ApplySnapshot(*(const NetSnapshot*)data);
        }
        break;
    default:
        break;
    }
}

static void ReceivePackets() {
    if (g_peer == INVALID_SOCKET) return;
    for (;;) {
        int space = (int)sizeof(g_rx) - g_rx_used;
        if (space <= 0) { StopNetwork("受信バッファがあふれました"); return; }
        int n = recv(g_peer, g_rx + g_rx_used, space, 0);
        if (n == 0) { StopNetwork("相手が切断しました"); return; }
        if (n == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) break;
            StopNetwork("受信エラー"); return;
        }
        g_rx_used += n;
    }

    int pos = 0;
    while (g_rx_used - pos >= (int)sizeof(NetHeader)) {
        NetHeader h;
        memcpy(&h, g_rx + pos, sizeof(h));
        if (h.magic != MAGIC) { StopNetwork("不正なデータを受信しました"); return; }
        if (g_rx_used - pos < (int)sizeof(NetHeader) + h.len) break;
        ProcessPacket(h.type, g_rx + pos + sizeof(NetHeader), h.len);
        if (g_peer == INVALID_SOCKET) return;
        pos += sizeof(NetHeader) + h.len;
    }
    if (pos > 0) {
        memmove(g_rx, g_rx + pos, g_rx_used - pos);
        g_rx_used -= pos;
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

static void OnPeerConnected() {
    int one = 1;
    setsockopt(g_peer, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
    SetNonBlocking(g_peer);
    g_rx_used = 0;
    g_tx_used = 0;
    g_myNetReady = false;
    g_enemyNetReady = false;
    g_netRulesReceived = false;
    g_netRuleRow = 0;
    g_mode = MODE_NET_STAGE_SELECT;
    SendPacket(PKT_HELLO, NULL, 0);
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
        g_peer = s;
        CloseSocket(g_listen);
        OnPeerConnected();
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
    OnPeerConnected();
}

// ホスト：両者の入力で1フレーム進め、結果を送る
static void NetHostUpdate() {
    unsigned short localBits = g_cmdListOpen ? 0 : ReadLocalBits();
    if (g_remoteQueueCount > 0) {
        // 溜まりすぎたら古い入力を捨てて追いつく
        if (g_remoteQueueCount > 4) {
            int drop = g_remoteQueueCount - 2;
            memmove(g_remoteQueue, g_remoteQueue + drop, sizeof(unsigned short) * (g_remoteQueueCount - drop));
            g_remoteQueueCount -= drop;
        }
        g_remoteBits = g_remoteQueue[0];
        memmove(g_remoteQueue, g_remoteQueue + 1, sizeof(unsigned short) * (g_remoteQueueCount - 1));
        g_remoteQueueCount--;
    }

    g_netRecordFx = true;
    switch (g_netPhase) {
    case NP_READY:
    case NP_FIGHT:
        if (g_netPhase == NP_READY) {
            g_readyFightTimer++;
            if (g_readyFightTimer == 50) TriggerScreenShake(8);
            if (g_readyFightTimer >= 80) g_netPhase = NP_FIGHT;
        }
        else if (g_netRules.timeLimit > 0) {
            g_battleTimerFrames++;
            if (g_battleTimerFrames >= 60) {
                g_battleTimerFrames = 0;
                if (g_roundTime > 0) g_roundTime--;
            }
        }
        UpdateControlledFighter(g_me, g_other, g_ctrl[0], localBits);
        UpdateControlledFighter(g_other, g_me, g_ctrl[1], g_remoteBits);
        UpdateTraps();
        UpdateStageGimmick(g_currentStageIdx);
        UpdateShots();
        UpdateEffects();

        if (g_netPhase == NP_FIGHT) {
            bool ko = (g_me.hp <= 0 || g_other.hp <= 0);
            bool timeUp = (g_netRules.timeLimit > 0 && g_roundTime <= 0);
            if (ko || timeUp) {
                float ra = (float)g_me.hp / (float)g_me.maxHp;
                float rb = (float)g_other.hp / (float)g_other.maxHp;
                int roundWinner;
                if (ko) roundWinner = (g_me.hp <= 0 && g_other.hp <= 0) ? 3 : ((g_me.hp > 0) ? 1 : 2);
                else roundWinner = (fabsf(ra - rb) < 0.001f) ? 3 : ((ra > rb) ? 1 : 2);
                if (roundWinner == 1) g_netRounds[0]++;
                if (roundWinner == 2) g_netRounds[1]++;
                g_netWinner = roundWinner;
                g_netPhase = NP_ROUND_END;
                g_netPhaseTimer = 0;
                TriggerScreenShake(16);
            }
        }
        break;
    case NP_ROUND_END:
        UpdateEffects();
        if (++g_netPhaseTimer >= 120) {
            bool matchOver = g_netRounds[0] >= g_netRules.roundsToWin || g_netRounds[1] >= g_netRules.roundsToWin;
            if (matchOver) {
                g_netWinner = (g_netRounds[0] > g_netRounds[1]) ? 1 : ((g_netRounds[1] > g_netRounds[0]) ? 2 : 3);
                g_netPhase = NP_MATCH_END;
                g_netPhaseTimer = 0;
            }
            else {
                g_netRoundNo++;
                g_netWinner = 0;
                SetupNetRound();
            }
        }
        break;
    case NP_MATCH_END:
        UpdateEffects();
        g_netPhaseTimer++;
        break;
    }
    g_netRecordFx = false;
    SendSnapshot();
    if (g_netPhase == NP_MATCH_END && g_netPhaseTimer >= 90) {
        g_roundWon = (g_netWinner == 1);
        g_mode = MODE_NET_RESULT;
    }
}

// クライアント：入力を送り、届いた状態を表示する
static void NetClientUpdate() {
    unsigned short bits = g_cmdListOpen ? 0 : ReadLocalBits();
    NetInputMsg msg = { ++g_sequence, bits };
    SendPacket(PKT_INPUT, &msg, sizeof(msg));
    ReceivePackets();
    UpdateEffects();
    if (g_netPhase == NP_MATCH_END && g_netPhaseTimer >= 80) {
        g_roundWon = (g_netWinner == 2);
        g_mode = MODE_NET_RESULT;
    }
}

static void DrawNetPhaseBanner() {
    int myWinner = g_is_host ? 1 : 2;
    if (g_netPhase == NP_ROUND_END) {
        DrawBox(170, 180, 470, 260, BLACK, TRUE);
        DrawBox(170, 180, 470, 260, RED, FALSE);
        const char* text = (g_netWinner == 3) ? "DRAW" : ((g_netWinner == myWinner) ? "ROUND WIN!" : "ROUND LOSE…");
        DrawString(270, 200, (g_me.hp <= 0 || g_other.hp <= 0) ? "K. O." : "TIME OVER", YELLOW);
        DrawString(260, 228, text, (g_netWinner == myWinner) ? YELLOW : GRAY);
    }
    else if (g_netPhase == NP_MATCH_END) {
        DrawBox(150, 175, 490, 265, BLACK, TRUE);
        DrawBox(150, 175, 490, 265, YELLOW, FALSE);
        const char* text = (g_netWinner == 3) ? "DRAW GAME" : ((g_netWinner == myWinner) ? "YOU WIN!!" : "YOU LOSE…");
        DrawExtendString(230, 205, 2.0, 2.0, text, (g_netWinner == myWinner) ? YELLOW : RED);
    }
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
    LoadTutorialProgress();
    LoadCharaGraphs();
    memset(g_keyNow, 0, sizeof(g_keyNow));
    memset(g_keyPrev, 0, sizeof(g_keyPrev));

    // 通信対戦で選べるキャラ（最初から使えるキャラのみ）
    int netCharaList[CHARA_COUNT];
    int netCharaCount = 0;
    for (int i = 0; i < ROSTER_COUNT; i++) {
        if (CHARA_PROFILES[g_rosterOrder[i]].unlockLap <= 1) netCharaList[netCharaCount++] = g_rosterOrder[i];
    }
    // トレーニングで選べるキャラ
    int trainCharaList[CHARA_COUNT];
    int trainCharaCount = 0;
    int dummyCharaList[CHARA_COUNT];
    int dummyCharaCount = 0;

    while (ProcessMessage() == 0) {
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
            else if (g_titleMenu == 2) DrawString(150, 370, "システムやキャラの技を自由に練習できます", GRAY);

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
                        g_cmdListOpen = false;
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
            int cur = g_rosterOrder[g_charaCursor];
            bool curLocked = !IsCharaUnlocked(cur);

            HandleCommandListKeys(cur, false, false);

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

            DrawString(80, 455, "[A/D]: カードをめくる  [ENTER]: 決定して参戦  [TAB]: 技表  [ESC]: 戻る", WHITE);

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

            if (g_cmdListOpen) {
                if (curLocked) {
                    g_cmdListOpen = false;
                    g_lockedMsgTimer = 70;
                }
                else {
                    DrawCommandList(false);
                }
                break;
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
                        g_mode = MODE_STORY_INTRO;
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

        case MODE_STORY_INTRO: {
            g_phaseTimer++;
            int c = g_slots[g_currentSlot].chosenChara;
            DrawBox(0, 0, 640, 480, GetColor(10, 8, 20), TRUE);
            DrawString(260, 50, "- STORY -", YELLOW);
            DrawFormatString(60, 100, WHITE, "%s", CHARA_PROFILES[c].charaId);
            DrawFormatString(60, 124, ORANGE, "%s", CHARA_PROFILES[c].archetype);
            DrawBox(40, 160, 600, 290, GetColor(20, 20, 35), TRUE);
            DrawBox(40, 160, 600, 290, GRAY, FALSE);
            for (int i = 0; i < 3; i++) {
                // 1行ずつ順番に表示
                if (g_phaseTimer > 20 + i * 40) DrawString(60, 180 + i * 34, STORY_PROLOGUE[c][i], WHITE);
            }
            if (GetHardLevel() > 0) {
                DrawFormatString(60, 310, ORANGE, "%d周目：敵がさらに手強くなっている…！", GetCurrentLap());
            }
            DrawString(200, 400, "[Enter]: 戦いへ向かう", (g_frameCount / 20) % 2 ? WHITE : GRAY);

            if (s_keyCooldown == 0 && g_phaseTimer > 20 && CheckHitKey(KEY_INPUT_RETURN)) {
                s_keyCooldown = 20;
                g_mode = MODE_VS_SCREEN;
                g_phaseTimer = 0;
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
            if (curStage == 1) DrawString(150, 380, "注意: 床に電流が流れる。予告の点滅を見て回避しよう", CYAN);

            if (g_phaseTimer > 120 || (CheckHitKey(KEY_INPUT_RETURN) && s_keyCooldown == 0 && g_phaseTimer > 20)) {
                SetupBattleRound();
                g_mode = MODE_BATTLE_PLAY;
            }
            break;
        }

        case MODE_BATTLE_PLAY: {
            HandleCommandListKeys(g_me.charaIndex, false, true);

            if (!g_cmdListOpen) {
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

                UpdateControlledFighter(g_me, g_other, g_ctrl[0], ReadLocalBits());
                UpdateCpu();
                UpdateTraps();
                UpdateStageGimmick(curStage);
                UpdateShots();
                UpdateEffects();
            }

            DrawStageBackground(curStage);
            DrawGimmick(curStage);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            DrawHud(HUD_STORY);

            if (g_cmdListOpen) {
                DrawCommandList(false);
                break;
            }

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
            UpdateEffects();

            DrawStageBackground(curStage);
            DrawGimmick(curStage);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            DrawHud(HUD_STORY);

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
            UpdateEffects();
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
            DrawHud(HUD_STORY);

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

            DrawString(220, 60, "=== VICTORY! ===", YELLOW);
            DrawFormatString(60, 110, WHITE, "敗者 %s: 「%s」", CPU_ROSTER[prevStage].name, CPU_ROSTER[prevStage].winQuote);

            float winRate = 0.0f;
            if (s.totalFights > 0) {
                winRate = ((float)s.wins / (float)s.totalFights) * 100.0f;
            }
            DrawFormatString(60, 150, CYAN, "通算戦績: %d戦 %d勝 %d敗 %d分 (勝率: %.1f%%)",
                s.totalFights, s.wins, s.losses, s.draws, winRate);

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
            DrawString(240, 100, "=== DEFEAT... ===", RED);
            DrawString(220, 150, "CONTINUE CHALLENGE?", WHITE);

            DrawBox(50, 190, 590, 270, GetColor(25, 10, 10), TRUE);
            DrawBox(50, 190, 590, 270, GRAY, FALSE);
            DrawString(60, 195, "この試合の記録", ORANGE);
            DrawStatsBlock(60, 215, 0, g_me.charaIndex, WHITE);

            DrawString(200, 310, "[Enter]: このステージから再挑戦", GREEN);
            DrawString(200, 340, "[Esc]: スロット選択画面へ戻る", GRAY);
            DrawString(130, 390, "ヒント: タイトルの TRAINING で技やコンボを練習できます", GRAY);

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
                DrawExtendString(236, 20, 3.0, 3.0, "CLEAR!!", cc);
                DrawString(150, 76, "【伝説達成】裏ボス・真 天輪太子を撃破した！", RED);
            }
            else {
                DrawExtendString(184, 24, 2.0, 2.0, "FINAL STAGE CLEAR", YELLOW);
                DrawString(90, 76, "※ノーコンティニュー＆タイムオーバー無しで… 何かが現れる？", GRAY);
            }

            // 専用エンディング
            DrawBox(40, 100, 600, 220, GetColor(10, 12, 24), TRUE);
            DrawBox(40, 100, 600, 220, GRAY, FALSE);
            DrawFormatString(52, 104, ORANGE, "- %s ENDING -", CHARA_PROFILES[c].charaId);
            for (int i = 0; i < 3; i++) {
                if (g_phaseTimer > 30 + i * 50) DrawString(52, 128 + i * 22, STORY_ENDING[c][i], WHITE);
            }
            if (g_endingSecret && g_phaseTimer > 180) DrawString(52, 196, STORY_ENDING_SECRET[c], YELLOW);

            DrawFormatString(60, 232, YELLOW, "称号『%s』を獲得！", CHARA_TITLES[c][g_endingRank - 1]);

            float winRate = 0.0f;
            if (s.totalFights > 0) {
                winRate = ((float)s.wins / (float)s.totalFights) * 100.0f;
            }
            DrawFormatString(60, 256, CYAN, "最終戦績: %d戦 %d勝 %d敗 %d分 (勝率: %.1f%%)",
                s.totalFights, s.wins, s.losses, s.draws, winRate);
            DrawFormatString(60, 280, ORANGE, "次は %d周目：敵の攻撃力・AIが強化されます (難易度+%d)", GetCurrentLap(), GetHardLevel());

            if (g_endingUnlockChara >= 0 && (g_phaseTimer / 10) % 3 != 0) {
                DrawFormatString(60, 312, CYAN, "NEW CHALLENGER!! 次の周回から『%s』が使用可能に！",
                    CHARA_PROFILES[g_endingUnlockChara].charaId);
            }

            DrawString(170, 380, "[Enter]: キャラクター選択へ (周回プレイ)", WHITE);
            DrawString(170, 405, "[Esc]: タイトルへ戻る", GRAY);

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
                        // TUTORIAL
                        // ==========================================
        case MODE_TUTORIAL: {
            if (g_tut.phase == TP_PLAY) HandleCommandListKeys(g_me.charaIndex, false, true);
            if (!g_cmdListOpen) UpdateTutorialFrame();

            DrawStageBackground(0);
            DrawGimmick(0);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            DrawHud(HUD_TUTORIAL);
            DrawTutorialOverlay();
            if (g_cmdListOpen) {
                DrawCommandList(false);
                break;
            }

            if (g_tut.phase == TP_CHAPTER_CLEAR && g_tut.timer > 30) {
                if (KeyHit(KEY_INPUT_RETURN)) SetupTutorial(g_tut.chapter + 1);
                else if (KeyHit(KEY_INPUT_ESCAPE)) { g_mode = MODE_TITLE; s_keyCooldown = 15; }
            }
            else if (g_tut.phase == TP_ALL_CLEAR) {
                if (g_tut.timer > 30 && (KeyHit(KEY_INPUT_RETURN) || KeyHit(KEY_INPUT_ESCAPE))) {
                    g_mode = MODE_TITLE;
                    g_titleMenu = 1;
                    s_keyCooldown = 15;
                }
            }
            else if (KeyHit(KEY_INPUT_ESCAPE)) {
                g_mode = MODE_TITLE;
                s_keyCooldown = 15;
            }
            break;
        }

                        // ==========================================
                        // TRAINING
                        // ==========================================
        case MODE_TRAINING_SELECT: {
            DrawBox(0, 0, 640, 480, GetColor(8, 14, 20), TRUE);
            DrawString(230, 30, "=== TRAINING MODE ===", YELLOW);
            DrawString(130, 56, "反撃してこないサンドバッグ相手に自由に練習できます", GRAY);

            int myPos = 0, dmPos = 0;
            for (int i = 0; i < trainCharaCount; i++) if (trainCharaList[i] == g_trainMyChara) myPos = i;
            for (int i = 0; i < dummyCharaCount; i++) if (dummyCharaList[i] == g_trainDummyChara) dmPos = i;
            g_trainMyChara = trainCharaList[myPos];
            g_trainDummyChara = dummyCharaList[dmPos];

            const char* rowName[4] = { "自分のキャラ", "ダミーのキャラ", "ステージ", "トレーニング開始" };
            for (int r = 0; r < 4; r++) {
                int y = 110 + r * 60;
                bool sel = (g_trainRow == r);
                DrawBox(80, y, 560, y + 46, sel ? GetColor(40, 40, 20) : GetColor(20, 24, 34), TRUE);
                DrawBox(80, y, 560, y + 46, sel ? YELLOW : GRAY, FALSE);
                DrawString(96, y + 14, rowName[r], sel ? YELLOW : WHITE);
                if (r == 0) DrawFormatString(260, y + 14, WHITE, "< %s >", CHARA_PROFILES[g_trainMyChara].charaId);
                if (r == 1) DrawFormatString(260, y + 14, WHITE, "< %s >", CHARA_PROFILES[g_trainDummyChara].charaId);
                if (r == 2) DrawFormatString(260, y + 14, WHITE, "< %s >", STAGES[g_trainStage].name);
            }
            DrawString(70, 360, "※未解放キャラは、どれかのセーブデータで解放すると使えます", GRAY);
            DrawString(70, 400, "[W/S]: 項目  [A/D]: 変更  [ENTER]: 開始  [TAB]: 技表  [ESC]: 戻る", WHITE);

            HandleCommandListKeys(g_trainMyChara, true, true);
            if (g_cmdListOpen) {
                DrawCommandList(true);
                break;
            }

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
                if (CheckHitKey(KEY_INPUT_RETURN)) {
                    s_keyCooldown = 15;
                    SetupTraining();
                    g_mode = MODE_TRAINING;
                }
                if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                    s_keyCooldown = 15;
                    g_mode = MODE_TITLE;
                }
            }
            break;
        }

        case MODE_TRAINING: {
            HandleCommandListKeys(g_me.charaIndex, true, true);
            if (!g_cmdListOpen) {
                HandleTrainingKeys();
                UpdateTrainingFrame();
            }

            DrawStageBackground(g_currentStageIdx);
            DrawGimmick(g_currentStageIdx);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            if (g_showHitbox) DrawHitboxes();
            DrawHud(HUD_TRAINING);
            DrawTrainingOverlay();

            if (g_cmdListOpen) {
                DrawCommandList(true);
            }
            else if (KeyHit(KEY_INPUT_ESCAPE)) {
                s_keyCooldown = 15;
                g_mode = MODE_TRAINING_SELECT;
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
                if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                    g_is_net_battle = false;
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
            else {
                DrawString(200, 400, "ホストがステージとルールを選んでいます...", YELLOW);
            }
            if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                CloseSocket(g_peer);
                g_is_net_battle = false;
                g_mode = MODE_TITLE;
                s_keyCooldown = 15;
            }
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
            DrawBox(140, 100, 180, 160, CHARA_PROFILES[g_myNetChara].baseColor, TRUE);
            DrawFormatString(45, 180, WHITE, "NAME: %s", CHARA_PROFILES[g_myNetChara].charaId);

            DrawBox(350, 45, 610, 275, GetColor(35, 20, 25), TRUE);
            DrawBox(350, 45, 610, 275, g_enemyNetReady ? GREEN : BLUE, FALSE);
            DrawString(365, 55, "2P: REMOTE", g_enemyNetReady ? GREEN : BLUE);
            if (g_enemyNetReady) {
                DrawBox(460, 100, 500, 160, CHARA_PROFILES[g_enemyNetChara].baseColor, TRUE);
                DrawFormatString(365, 180, WHITE, "NAME: %s", CHARA_PROFILES[g_enemyNetChara].charaId);
            }
            else {
                DrawString(365, 180, "選択中...", GRAY);
            }

            for (int i = 0; i < netCharaCount; i++) {
                int cx = 22 + i * 100;
                int cy = 295;
                int c = netCharaList[i];
                DrawBox(cx, cy, cx + 94, cy + 90, GetColor(20, 25, 35), TRUE);
                DrawBox(cx, cy, cx + 94, cy + 90, (i == g_netCharaCursor) ? YELLOW : GRAY, FALSE);
                DrawBox(cx + 32, cy + 20, cx + 62, cy + 50, CHARA_PROFILES[c].baseColor, TRUE);
                DrawFormatString(cx + 4, cy + 60, WHITE, "%s", CHARA_PROFILES[c].charaId);
            }

            HandleCommandListKeys(g_myNetChara, false, true);
            if (g_cmdListOpen) {
                DrawCommandList(false);
            }
            else if (!g_myNetReady) {
                DrawString(120, 415, "A / D: キャラ選択   Enter: 決定   TAB: 技表", WHITE);
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
            else {
                DrawString(200, 415, "相手の決定を待機中...", YELLOW);
            }

            // WaitTimer を使わず、そのまま試合へ（READY演出はフレームで進む）
            if (g_myNetReady && g_enemyNetReady) {
                g_cmdListOpen = false;
                SetupNetMatch();
                g_mode = MODE_NET_BATTLE_PLAY;
            }
            break;
        }

        case MODE_NET_BATTLE_PLAY: {
            HandleCommandListKeys(g_me.charaIndex, false, false);
            if (g_is_host) {
                ReceivePackets();
                if (g_mode != MODE_NET_BATTLE_PLAY) break;
                NetHostUpdate();
            }
            else {
                NetClientUpdate();
            }
            FlushSend();
            if (g_mode == MODE_NET_ERROR) break;

            DrawStageBackground(g_currentStageIdx);
            DrawGimmick(g_currentStageIdx);
            DrawFighterObj(g_me, true);
            DrawFighterObj(g_other, false);
            DrawHud(HUD_NET);
            DrawNetPhaseBanner();
            if (!g_is_host && !g_netGotSnapshot) {
                DrawBox(200, 300, 440, 330, BLACK, TRUE);
                DrawString(230, 307, "ホストと同期中...", YELLOW);
            }
            if (g_cmdListOpen) DrawCommandList(false);

            if (KeyHit(KEY_INPUT_ESCAPE)) {
                CloseSocket(g_peer);
                g_is_net_battle = false;
                g_mode = MODE_TITLE;
                s_keyCooldown = 15;
            }
            break;
        }

        case MODE_NET_RESULT: {
            DrawBox(0, 0, 640, 480, GetColor(10, 10, 15), TRUE);
            if (g_netWinner == 3) {
                DrawString(250, 40, "=== DRAW GAME ===", GRAY);
            }
            else if (g_roundWon) {
                DrawString(240, 40, "=== YOU WIN! ===", YELLOW);
            }
            else {
                DrawString(240, 40, "=== YOU LOSE... ===", RED);
            }
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
            if (s_keyCooldown == 0 && CheckHitKey(KEY_INPUT_ESCAPE)) {
                CloseSocket(g_peer);
                g_is_net_battle = false;
                g_mode = MODE_TITLE;
                s_keyCooldown = 15;
            }
            break;
        }

        case MODE_NET_ERROR: {
            DrawBox(0, 0, 640, 480, BG_DARK, TRUE);
            DrawString(100, 180, "[通信エラー / 切断]", RED);
            DrawString(100, 220, g_net_status, WHITE);
            DrawString(100, 280, "[ESC] タイトルへ戻る", GRAY);
            if (CheckHitKey(KEY_INPUT_ESCAPE)) {
                g_is_net_battle = false;
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
    DeleteCharaGraphs();
    DxLib_End();
    return 0;
}
