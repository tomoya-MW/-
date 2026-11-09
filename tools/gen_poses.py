# キャラの立ち絵（64x64 / 大前は128x128）から、各ポーズ画像と半身の顔絵を加工生成する
#   出力は 24bit BMP（黒背景＝透過色）。このプロジェクトの DxLib は PNG 読み込みが無効のため
#   使い方: python -I gen_poses.py <出力先の Debug フォルダ>
import sys
import os
from collections import Counter, deque

import numpy as np
from PIL import Image, ImageDraw

OUT = sys.argv[1]

# 基本名, スケール(1=64px / 2=128px), 元絵が左向きか, オーラ色
CHARS = [
    ("rikidou",   1, False, (255, 140, 20)),
    ("wanataro",  1, False, (255, 150, 30)),
    ("ananda",    1, False, (60, 220, 255)),
    ("inoshishi", 1, False, (90, 230, 120)),
    ("omae",      2, False, (255, 70, 70)),
    ("mama",      1, False, (130, 170, 255)),
    ("boss",      1, False, (255, 220, 70)),
    ("yoshida",   1, False, (240, 240, 255)),   # 元絵は左向き。リポジトリの yoshida.bmp は右向きに直し済み
    ("takeshi",   1, False, (60, 220, 255)),
]

POSES = ["crouch", "jump", "light", "heavy", "low", "air", "special", "upper", "guard", "cguard",
         "hit", "down", "stun", "throw", "held", "dash", "roll", "super", "win"]


# ----------------------------------------------------------------------
# メガトン大前（元絵がないので描く）
# ----------------------------------------------------------------------
def draw_omae():
    S = 128
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    skin = (232, 182, 140, 255)
    skin_d = (196, 140, 104, 255)
    skin_l = (248, 210, 176, 255)
    belt = (44, 54, 100, 255)
    belt_d = (26, 32, 64, 255)
    hair = (24, 22, 30, 255)

    # 脚（四股を踏むような広いスタンス）
    d.line([(44, 90), (30, 120)], fill=skin_d, width=24)
    d.line([(82, 90), (94, 120)], fill=skin, width=24)
    d.ellipse([16, 114, 46, 127], fill=skin_d)
    d.ellipse([80, 114, 112, 127], fill=skin)
    d.ellipse([84, 100, 100, 112], fill=skin_l)
    # 胴体（大きな腹）
    d.ellipse([24, 36, 104, 108], fill=skin)
    d.ellipse([28, 64, 70, 106], fill=skin_d)
    d.ellipse([60, 44, 98, 84], fill=skin_l)
    d.arc([46, 60, 96, 100], 200, 320, fill=skin_d, width=2)
    d.ellipse([70, 80, 74, 84], fill=skin_d)
    # まわし
    d.rounded_rectangle([26, 82, 102, 100], radius=6, fill=belt)
    d.rectangle([26, 92, 102, 100], fill=belt_d)
    d.polygon([(62, 96), (82, 96), (78, 118), (66, 118)], fill=belt)
    for x in (66, 70, 74, 78):
        d.line([(x, 100), (x - 1, 120)], fill=belt_d, width=1)
    # 腕（突っ張りの構え）
    d.line([(40, 52), (30, 74), (44, 84)], fill=skin_d, width=15, joint="curve")
    d.ellipse([38, 76, 54, 90], fill=skin_d)
    d.line([(88, 50), (104, 58), (116, 66)], fill=skin, width=15, joint="curve")
    d.ellipse([108, 58, 126, 74], fill=skin)
    d.line([(112, 62), (120, 62)], fill=skin_d, width=1)
    # 頭とちょんまげ
    d.ellipse([60, 10, 96, 46], fill=skin)
    d.ellipse([60, 8, 94, 26], fill=hair)
    d.rectangle([60, 18, 70, 34], fill=hair)
    d.ellipse([70, 0, 84, 10], fill=hair)
    d.line([(76, 8), (78, 14)], fill=hair, width=3)
    d.ellipse([64, 26, 72, 36], fill=skin_d)            # 耳
    d.line([(82, 24), (92, 22)], fill=hair, width=3)    # 眉
    d.rectangle([86, 27, 89, 30], fill=(20, 20, 20, 255))  # 目
    d.line([(88, 38), (94, 38)], fill=(150, 70, 60, 255), width=2)  # 口
    d.ellipse([92, 30, 98, 36], fill=skin_d)            # 鼻
    return add_outline(im, (60, 34, 24, 255), 2)


# ----------------------------------------------------------------------
# 共通処理
# ----------------------------------------------------------------------
def load_clean(path):
    """黒背景を透明化し、グリーンバックの緑フチを取り除く"""
    rgb = np.array(Image.open(path).convert("RGB")).astype(int)
    h, w, _ = rgb.shape
    dark = rgb.max(axis=2) <= 14
    bg = np.zeros((h, w), bool)
    q = deque()
    for x in range(w):
        for y in (0, h - 1):
            if dark[y, x] and not bg[y, x]:
                bg[y, x] = True
                q.append((y, x))
    for y in range(h):
        for x in (0, w - 1):
            if dark[y, x] and not bg[y, x]:
                bg[y, x] = True
                q.append((y, x))
    while q:
        y, x = q.popleft()
        for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
            if 0 <= ny < h and 0 <= nx < w and dark[ny, nx] and not bg[ny, nx]:
                bg[ny, nx] = True
                q.append((ny, nx))
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    greenish = (g > 130) & (g > r + 55) & (g > b + 55)
    removed = 0
    for _ in range(3):
        nb = np.zeros_like(bg)
        nb[1:, :] |= bg[:-1, :]
        nb[:-1, :] |= bg[1:, :]
        nb[:, 1:] |= bg[:, :-1]
        nb[:, :-1] |= bg[:, 1:]
        kill = greenish & nb & ~bg
        removed += int(kill.sum())
        bg |= kill
    # 内側に残った強い緑は、周りの緑以外の色の平均で塗り直す
    strong = (g > 150) & (g > r + 70) & (g > b + 70) & ~bg
    for y, x in zip(*np.nonzero(strong)):
        acc = []
        for ny in range(max(0, y - 1), min(h, y + 2)):
            for nx in range(max(0, x - 1), min(w, x + 2)):
                if not bg[ny, nx] and not strong[ny, nx]:
                    acc.append(rgb[ny, nx])
        if acc:
            rgb[y, x] = np.mean(acc, axis=0)
        else:
            bg[y, x] = True
    out = np.zeros((h, w, 4), np.uint8)
    out[..., :3] = rgb
    out[..., 3] = np.where(bg, 0, 255)
    out[bg, :3] = 0
    return Image.fromarray(out, "RGBA"), removed


def add_outline(img, color, px):
    a = np.array(img)
    alpha = a[..., 3] > 0
    grown = alpha.copy()
    for _ in range(px):
        g2 = grown.copy()
        g2[1:, :] |= grown[:-1, :]
        g2[:-1, :] |= grown[1:, :]
        g2[:, 1:] |= grown[:, :-1]
        g2[:, :-1] |= grown[:, 1:]
        grown = g2
    ring = grown & ~alpha
    a[ring] = color
    return Image.fromarray(a, "RGBA")


def glow(img, color, px):
    """スプライトの外側に光るフチを足す"""
    base = add_outline(img, color + (255,), px)
    inner = add_outline(img, tuple(min(255, c + 80) for c in color) + (255,), 1)
    base.paste(inner, (0, 0), inner)
    base.paste(img, (0, 0), img)
    return base


def bbox(img):
    return img.getchannel("A").getbbox()


def blank(W, H):
    return Image.new("RGBA", (W, H), (0, 0, 0, 0))


def split(img, y):
    a = np.array(img)
    up = a.copy()
    lo = a.copy()
    up[y:, :, 3] = 0
    up[y:, :, :3] = 0
    lo[:y, :, 3] = 0
    lo[:y, :, :3] = 0
    return Image.fromarray(up, "RGBA"), Image.fromarray(lo, "RGBA")


def shear(img, k, pivot_y):
    """k>0 で上ほど前（右）へ傾ける"""
    a = np.array(img)
    h, w, _ = a.shape
    out = np.zeros_like(a)
    for y in range(h):
        dx = int(round(k * (pivot_y - y)))
        if dx >= w or -dx >= w:
            continue
        if dx >= 0:
            out[y, dx:] = a[y, :w - dx]
        else:
            out[y, :w + dx] = a[y, -dx:]
    return Image.fromarray(out, "RGBA")


def shift(img, dx, dy):
    c = blank(*img.size)
    c.paste(img, (dx, dy), img)
    return c


def squash_rows(img, y0, y1, new_h, anchor_bottom=True):
    """y0..y1 の行を new_h の高さに縮める"""
    a = np.array(img)
    part = Image.fromarray(a[y0:y1].copy(), "RGBA")
    a[y0:y1] = 0
    rest = Image.fromarray(a, "RGBA")
    part = part.resize((part.width, max(1, new_h)), Image.NEAREST)
    top = (y1 - new_h) if anchor_bottom else y0
    rest.paste(part, (0, top), part)
    return rest


def drop_to_bottom(img):
    bb = bbox(img)
    if not bb:
        return img
    return shift(img, 0, img.height - bb[3])


def center_x(img, cx):
    bb = bbox(img)
    if not bb:
        return img
    return shift(img, int(round(cx - (bb[0] + bb[2]) / 2)), 0)


def rotate_about(img, angle, cx, cy):
    return img.rotate(angle, resample=Image.NEAREST, center=(cx, cy))


def dominant(img, y0, y1, keep):
    a = np.array(img)[max(0, y0):max(1, y1)]
    px = a[a[..., 3] > 0][:, :3].astype(int)
    px = [p for p in px if keep(p)]
    if not px:
        return None
    cnt = Counter(tuple((p // 24) * 24) for p in px)
    key = cnt.most_common(1)[0][0]
    sel = [p for p in px if tuple((p // 24) * 24) == key]
    m = np.mean(sel, axis=0)
    return tuple(int(v) for v in m)


def lum(p):
    return 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]


def darker(c, f=0.45):
    return tuple(int(v * f) for v in c[:3])


def limb(d, p0, p1, width, color, end_r=0, end_color=None):
    """輪郭・影・ハイライト付きの手足（ドット絵風）"""
    oc = darker(color, 0.35) + (255,)
    sh = darker(color, 0.72) + (255,)
    hi = tuple(min(255, int(v * 1.15 + 20)) for v in color[:3]) + (255,)
    d.line([p0, p1], fill=oc, width=width + 2)
    if end_r:
        x, y = p1
        d.ellipse([x - end_r - 1, y - end_r - 1, x + end_r + 1, y + end_r + 1], fill=oc)
    d.line([p0, p1], fill=sh, width=width)
    # 上側半分を本来の色、さらに上端にハイライト
    up = max(1, width // 2)
    d.line([(p0[0], p0[1] - up // 2), (p1[0], p1[1] - up // 2)], fill=tuple(color[:3]) + (255,), width=up)
    d.line([(p0[0], p0[1] - up), (p1[0], p1[1] - up)], fill=hi, width=1)
    if end_r:
        x, y = p1
        ec = tuple((end_color or color)[:3])
        d.ellipse([x - end_r, y - end_r, x + end_r, y + end_r], fill=darker(ec, 0.8) + (255,))
        d.ellipse([x - end_r, y - end_r, x + end_r - 1, y + end_r - 1], fill=ec + (255,))
        d.point((x - end_r // 2, y - end_r // 2), fill=(255, 255, 255, 255))


def front_x(img, y, default):
    """その行で一番前（右）にある不透明ピクセルの x"""
    a = np.array(img)
    if y < 0 or y >= a.shape[0]:
        return default
    xs = np.nonzero(a[y, :, 3] > 0)[0]
    return int(xs.max()) if len(xs) else default


def streaks(d, pts, color=(255, 255, 255, 170), width=1):
    for p0, p1 in pts:
        d.line([p0, p1], fill=color, width=width)


# ----------------------------------------------------------------------
# ポーズ生成
# ----------------------------------------------------------------------
def make_poses(base64, s, aura):
    """base64: 64*s 四方の立ち絵(RGBA, 右向き)。96*s x 80*s のキャンバスに各ポーズを作る"""
    W, H = 96 * s, 80 * s
    canvas = blank(W, H)
    canvas.paste(base64, ((W - base64.width) // 2, H - base64.height), base64)
    base = canvas
    l, t, r, b = bbox(base)
    h = b - t
    w = r - l
    cx = (l + r) // 2
    shoulder = int(t + 0.30 * h)
    hip = int(t + 0.58 * h)
    chest = int(cx + 0.10 * w)

    sleeve = dominant(base, int(t + 0.28 * h), int(t + 0.55 * h), lambda p: lum(p) > 45) or (200, 200, 200)
    pants = dominant(base, int(t + 0.62 * h), int(t + 0.92 * h), lambda p: lum(p) > 35) or sleeve
    skin = dominant(base, t, b, lambda p: p[0] > p[1] > p[2] and p[0] - p[2] > 35 and p[0] > 120) or sleeve

    def crouched(img_src=None):
        src = img_src or base
        lo_h = b - hip
        new_lo = int(lo_h * 0.5)
        up, lo = split(src, hip)
        lo = squash_rows(lo, hip, b, new_lo)
        up = shift(up, s, lo_h - new_lo)
        c = blank(W, H)
        c.paste(lo, (0, 0), lo)
        c.paste(up, (0, 0), up)
        return c, b - (lo_h - new_lo) - int(0.30 * h) + (lo_h - new_lo) // 3

    poses = {}

    # しゃがみ
    cr, cr_sh = crouched()
    poses["crouch"] = cr

    # ジャンプ（脚をたたむ）
    up, lo = split(base, hip)
    lo = squash_rows(lo, hip, b, int((b - hip) * 0.6), anchor_bottom=False)
    jump = blank(W, H)
    jump.paste(lo, (0, 0), lo)
    jump.paste(up, (0, 0), up)
    jump = drop_to_bottom(jump)
    poses["jump"] = jump

    # 弱攻撃（突き）
    up, lo = split(base, hip)
    up = shift(shear(up, 0.16, hip), 2 * s, 0)
    c = blank(W, H)
    c.paste(lo, (0, 0), lo)
    c.paste(up, (0, 0), up)
    d = ImageDraw.Draw(c)
    ay = shoulder + 4 * s
    fx = front_x(c, ay, r)
    fist = (min(W - 4 * s, fx + 13 * s), ay - 1 * s)
    limb(d, (fx - 4 * s, ay), fist, 3 * s, sleeve, 3 * s, skin)
    streaks(d, [((fist[0] - 12 * s, fist[1] - 4 * s), (fist[0] - 4 * s, fist[1] - 4 * s)),
                ((fist[0] - 13 * s, fist[1] + 4 * s), (fist[0] - 5 * s, fist[1] + 4 * s))])
    poses["light"] = c

    # 強攻撃（蹴り）
    up, lo = split(base, hip)
    up = shear(up, -0.12, hip)
    c = blank(W, H)
    c.paste(lo, (0, 0), lo)
    c.paste(up, (0, 0), up)
    d = ImageDraw.Draw(c)
    ky = hip + 2 * s
    fx = front_x(c, ky, r)
    foot = (min(W - 4 * s, fx + 16 * s), hip - 4 * s)
    limb(d, (fx - 5 * s, ky), foot, 4 * s, pants, 3 * s, skin)
    d.arc([foot[0] - 20 * s, foot[1] - 12 * s, foot[0] + 4 * s, foot[1] + 14 * s], 280, 360,
          fill=(255, 255, 255, 180), width=s)
    poses["heavy"] = c

    # しゃがみ攻撃（足払い）
    c, _ = crouched()
    d = ImageDraw.Draw(c)
    ly = b - 6 * s
    fx = front_x(c, ly, r)
    foot = (min(W - 4 * s, fx + 16 * s), b - 2 * s)
    limb(d, (fx - 6 * s, ly), foot, 4 * s, pants, 3 * s, skin)
    streaks(d, [((foot[0] - 18 * s, b - 1 * s), (foot[0] - 6 * s, b - 1 * s))])
    poses["low"] = c

    # 空中攻撃（飛び蹴り）
    c = rotate_about(jump, -12, cx, (t + b) // 2)
    jb = bbox(c)
    d = ImageDraw.Draw(c)
    hy = int(jb[1] + 0.62 * (jb[3] - jb[1]))
    fx = front_x(c, hy, r)
    foot = (min(W - 4 * s, fx + 11 * s), jb[3] - 1 * s)
    limb(d, (fx - 5 * s, hy), foot, 4 * s, pants, 3 * s, skin)
    poses["air"] = drop_to_bottom(c)

    # 必殺技（踏み込み掌底＋オーラ）
    c = shift(shear(base, 0.20, b), 2 * s, 0)
    d = ImageDraw.Draw(c)
    for k, yy in enumerate((shoulder + 3 * s, shoulder + 9 * s)):
        fx = front_x(c, yy, r)
        limb(d, (fx - 4 * s, yy), (min(W - 4 * s, fx + (12 - 2 * k) * s), yy - s), 3 * s, sleeve, 3 * s, skin)
    poses["special"] = glow(c, aura, 2 * s)

    # 昇り（対空・打ち上げ）
    c = squash_rows(base, t, b, min(H - 2 * s, int(h * 1.06)))
    d = ImageDraw.Draw(c)
    nb = bbox(c)
    sx0 = front_x(c, shoulder + 2 * s, r) - 5 * s
    fist = (sx0 + 4 * s, max(4 * s, nb[1] - 5 * s))
    limb(d, (sx0, shoulder + 2 * s), fist, 3 * s, sleeve, 3 * s, skin)
    streaks(d, [((cx - 8 * s, b - 6 * s), (cx - 8 * s, t + 8 * s)), ((cx + 18 * s, b - 4 * s), (cx + 18 * s, t + 14 * s))],
            (255, 255, 255, 140))
    poses["upper"] = glow(c, aura, s)

    # ガード（腕を交差）
    c = shear(base, -0.08, b)
    d = ImageDraw.Draw(c)
    gx = front_x(c, shoulder + 4 * s, r) - 3 * s
    limb(d, (gx - 3 * s, shoulder - 1 * s), (gx + 5 * s, shoulder + 10 * s), 3 * s, sleeve)
    limb(d, (gx - 3 * s, shoulder + 10 * s), (gx + 5 * s, shoulder - 1 * s), 3 * s, sleeve, 2 * s, skin)
    poses["guard"] = c

    # しゃがみガード
    c, _ = crouched()
    cb = bbox(c)
    sy = int(cb[1] + 0.30 * (cb[3] - cb[1]))
    gx = front_x(c, sy + 4 * s, r) - 3 * s
    d = ImageDraw.Draw(c)
    limb(d, (gx - 3 * s, sy - 1 * s), (gx + 5 * s, sy + 9 * s), 3 * s, sleeve)
    limb(d, (gx - 3 * s, sy + 9 * s), (gx + 5 * s, sy - 1 * s), 3 * s, sleeve, 2 * s, skin)
    poses["cguard"] = c

    # やられ（のけぞり）
    poses["hit"] = drop_to_bottom(rotate_about(shift(base, -2 * s, 0), 16, cx, b))

    # ダウン（寝そべり）
    c = rotate_about(base, 90, cx, b - h // 2)
    c = center_x(drop_to_bottom(c), W // 2)
    poses["down"] = c

    # ピヨり
    c = squash_rows(base, t, b, int(h * 0.95))
    poses["stun"] = drop_to_bottom(rotate_about(c, 7, cx, b))

    # 投げ（両手でつかむ）
    c = shift(shear(base, 0.14, b), 2 * s, 0)
    d = ImageDraw.Draw(c)
    for yy in (shoulder + 2 * s, shoulder + 8 * s):
        fx = front_x(c, yy, r)
        limb(d, (fx - 4 * s, yy), (min(W - 4 * s, fx + 9 * s), yy - s), 3 * s, sleeve, 3 * s, skin)
    poses["throw"] = c

    # つかまれ（持ち上げられて苦しむ）
    c = rotate_about(base, -22, cx, (t + b) // 2)
    poses["held"] = drop_to_bottom(c)

    # ダッシュ（前傾＋残像線）
    c = shear(base, 0.30, b)
    d = ImageDraw.Draw(c)
    for i in range(4):
        y = t + int(h * (0.25 + 0.17 * i))
        streaks(d, [((max(0, l - 18 * s), y), (max(0, l - 4 * s), y))], (255, 255, 255, 150))
    poses["dash"] = c

    # 回避（転がる）
    small = base.resize((int(W * 0.72), int(H * 0.72)), Image.NEAREST)
    c = blank(W, H)
    c.paste(small, ((W - small.width) // 2, H - small.height), small)
    c = rotate_about(c, -75, W // 2, H - small.height // 2)
    poses["roll"] = center_x(drop_to_bottom(c), W // 2)

    # 超必殺技（オーラ）
    poses["super"] = glow(base, aura, 3 * s)

    # 勝利ポーズ（拳を突き上げる）
    c = base.copy()
    d = ImageDraw.Draw(c)
    sx0 = front_x(c, shoulder + 2 * s, r) - 5 * s
    fist = (sx0 + 3 * s, max(4 * s, t - 6 * s))
    limb(d, (sx0, shoulder + 2 * s), fist, 3 * s, sleeve, 3 * s, skin)
    poses["win"] = c

    return poses, (sleeve, pants, skin)


def make_face(base, s):
    """上半身を切り出して拡大した顔窓用の絵"""
    l, t, r, b = bbox(base)
    h = b - t
    y0 = max(0, t - 2 * s)
    y1 = min(base.height, int(t + 0.62 * h))
    size = y1 - y0
    cx = (l + r) // 2
    x0 = cx - size // 2
    crop = blank(size, size)
    region = base.crop((x0, y0, x0 + size, y1))
    crop.paste(region, (0, 0), region)
    return crop.resize((96, 96), Image.NEAREST)


def save_rgb_black(img, path):
    rgb = Image.new("RGB", img.size, (0, 0, 0))
    rgb.paste(img, (0, 0), img)
    rgb.save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    preview_rows = []
    for name, s, face_left, aura in CHARS:
        path = os.path.join(OUT, name + ".bmp")
        if name == "omae" and not os.path.exists(path):
            base = draw_omae()
            removed = 0
        else:
            base, removed = load_clean(path)
        if face_left:
            base = base.transpose(Image.FLIP_LEFT_RIGHT)
        # 立ち絵も緑フチを消して右向きにそろえて保存し直す
        save_rgb_black(base, path)
        poses, cols = make_poses(base, s, aura)
        for p in POSES:
            save_rgb_black(poses[p], os.path.join(OUT, f"{name}_{p}.bmp"))
        save_rgb_black(make_face(base, s), os.path.join(OUT, f"{name}_face.bmp"))
        print(f"{name}: 緑フチ除去 {removed}px  袖{cols[0]} 脚{cols[1]} 肌{cols[2]}")
        preview_rows.append((name, s, base, poses))

    # 確認用の一覧（スクラッチに出力）
    if len(sys.argv) > 2:
        cell = 96
        sheet = Image.new("RGBA", (cell * (len(POSES) + 1), cell * len(preview_rows)), (60, 60, 90, 255))
        for row, (name, s, base, poses) in enumerate(preview_rows):
            imgs = [base] + [poses[p] for p in POSES]
            for col, im in enumerate(imgs):
                k = cell / max(im.width, im.height)
                im2 = im.resize((max(1, int(im.width * k)), max(1, int(im.height * k))), Image.NEAREST)
                sheet.paste(im2, (col * cell + (cell - im2.width) // 2, row * cell + cell - im2.height), im2)
        sheet.save(sys.argv[2])


if __name__ == "__main__":
    main()
