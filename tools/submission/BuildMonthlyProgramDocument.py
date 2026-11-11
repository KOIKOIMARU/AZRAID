"""Build the program explanation with game captures and verified source excerpts."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import textwrap
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph


WORKSPACE = Path(__file__).resolve().parents[2]
W, H = landscape(A4)
INK = colors.HexColor("#23272c")
QUIET = colors.HexColor("#5f6871")
BLUE = colors.HexColor("#245b8a")
ORANGE = colors.HexColor("#b45225")
RULE = colors.HexColor("#c5cbd0")
CODE_BG = colors.HexColor("#f0f2f4")
HIGHLIGHT = colors.HexColor("#fff0d8")
CONTENTS = (
    ("ゲーム全体の処理", 3),
    ("出撃中の移動とカメラ", 4),
    ("機体の分解と落下", 5),
    ("炎の配置とアニメーション", 6),
    ("照準から発射方向を求める", 7),
    ("チャージ弾の範囲攻撃", 8),
    ("弾の再利用", 9),
    ("クラスごとの役割", 10),
    ("まとめ", 11),
)
TOTAL_PAGES = 11


def excerpt(relative, start, count=None, end=None, after=None):
    """Keep displayed code tied to the actual file, including real line numbers."""
    lines = (WORKSPACE / relative).read_text(encoding="utf-8").splitlines()
    first = 0
    if after:
        first = next(i for i, line in enumerate(lines) if after in line)
    begin = next(i for i in range(first, len(lines)) if start in lines[i])
    finish = (begin + count) if count else next(
        i + 1 for i in range(begin, len(lines)) if end in lines[i])
    selected = lines[begin:finish]
    indent = min((len(line) - len(line.lstrip()) for line in selected if line.strip()), default=0)
    return [(begin + i + 1, line[indent:]) for i, line in enumerate(selected)]


class Document:
    def __init__(self, output, account, assets):
        pdfmetrics.registerFont(TTFont("JP", "C:/Windows/Fonts/meiryo.ttc"))
        pdfmetrics.registerFont(TTFont("JPBold", "C:/Windows/Fonts/meiryob.ttc"))
        pdfmetrics.registerFont(TTFont("Code", "C:/Windows/Fonts/consola.ttf"))
        pdfmetrics.registerFontFamily("JP", normal="JP", bold="JPBold")
        output.parent.mkdir(parents=True, exist_ok=True)
        self.pdf = canvas.Canvas(str(output), pagesize=(W, H))
        self.pdf.setTitle("アズレイド プログラム説明資料")
        self.pdf.setAuthor("小泉 羚")
        self.account, self.assets, self.page = account, assets, 0
        self.images = {}

    def text(self, value, x, top, size=12, color=INK, bold=False):
        self.pdf.setFillColor(color)
        self.pdf.setFont("JPBold" if bold else "JP", size)
        self.pdf.drawString(x, H - top - size, value)

    def paragraph(self, value, x, top, width, size=12, color=INK, leading=None, limit=548):
        style = ParagraphStyle("p", fontName="JP", fontSize=size,
                               leading=leading or size * 1.7, textColor=color, wordWrap="CJK")
        p = Paragraph(escape(value).replace("\n", "<br/>"), style)
        _, height = p.wrap(width, H)
        if top + height > limit:
            raise ValueError(f"Text exceeds page {self.page}: {value[:40]}")
        p.drawOn(self.pdf, x, H - top - height)
        return height

    def line(self, x1, top1, x2, top2, color=RULE, width=0.7, dashed=False):
        self.pdf.saveState()
        self.pdf.setStrokeColor(color)
        self.pdf.setLineWidth(width)
        if dashed:
            self.pdf.setDash(4, 3)
        self.pdf.line(x1, H - top1, x2, H - top2)
        self.pdf.restoreState()

    def begin(self, title, cover=False):
        if self.page:
            self.pdf.showPage()
        self.page += 1
        self.pdf.setFillColor(colors.white)
        self.pdf.rect(0, 0, W, H, fill=1, stroke=0)
        self.pdf.bookmarkPage(f"page{self.page}")
        self.pdf.addOutlineEntry(title, f"page{self.page}", level=0)
        if cover:
            self.pdf.setFillColor(INK)
            self.pdf.setFont("JPBold", 38)
            self.pdf.drawCentredString(W / 2, H / 2 + 20, title)
            return
        self.text(title, 44, 34, 25, INK, True)
        self.line(44, 83, W - 44, 83)
        self.text("アズレイド / プログラム説明資料", 44, H - 26, 8, QUIET)
        self.pdf.setFillColor(QUIET)
        self.pdf.setFont("JP", 8)
        self.pdf.drawRightString(W - 44, 18, f"{self.page} / {TOTAL_PAGES}")

    def heading(self, value, x, top, size=14):
        self.text(value, x, top, size, BLUE, True)

    def image(self, path, x, top, width, height=None, crop=None):
        path = Path(path)
        if not path.is_absolute():
            path = self.assets / path
        key = str(path)
        if key not in self.images:
            self.images[key] = ImageReader(str(path))
        image = self.images[key]
        iw, ih = image.getSize()
        # Crop only within the PDF. The original capture is preserved unchanged.
        sx, sy, sw, sh = crop or (1, 31, iw - 2, ih - 32)
        height = height or width * sh / sw
        fx, fy = width / sw, height / sh
        self.pdf.saveState()
        clipping = self.pdf.beginPath()
        clipping.rect(x, H - top - height, width, height)
        self.pdf.clipPath(clipping, stroke=0)
        self.pdf.drawImage(image, x - sx * fx, H - top - (ih - sy) * fy,
                           iw * fx, ih * fy, mask="auto")
        self.pdf.restoreState()
        self.pdf.setStrokeColor(RULE)
        self.pdf.setLineWidth(0.5)
        self.pdf.rect(x, H - top - height, width, height, fill=0, stroke=1)
        return height

    def caption(self, value, x, top, width=752):
        self.paragraph(value, x, top, width, 9, QUIET, 14, limit=565)

    def note(self, value, x, top, target_x, target_top, color=BLUE):
        self.line(x, top + 16, target_x, target_top, color, 1)
        self.pdf.setFillColor(color)
        self.pdf.circle(target_x, H - target_top, 2.5, fill=1, stroke=0)
        self.pdf.setFillColor(colors.white)
        text_width = pdfmetrics.stringWidth(value, "JPBold", 10)
        self.pdf.rect(x - 3, H - top - 16, text_width + 6, 18, fill=1, stroke=0)
        self.text(value, x, top, 10, color, True)

    def code(self, relative, function, rows, x, top, width, size=10, highlight=()):
        available = width - 54
        displayed = []
        for number, code in rows:
            if number is None:
                displayed.append((None, "..."))
                continue
            if pdfmetrics.stringWidth(code, "Code", size) > available:
                maximum = int(available / pdfmetrics.stringWidth("M", "Code", size))
                for i, piece in enumerate(textwrap.wrap(code, width=maximum,
                                                       subsequent_indent="    ",
                                                       break_long_words=False,
                                                       break_on_hyphens=False)):
                    displayed.append((number if i == 0 else "", piece))
            else:
                displayed.append((number, code))
        leading = size * 1.43
        height = 35 + len(displayed) * leading
        if top + height > 549:
            raise ValueError(f"Code overflow on page {self.page}: {function}")
        self.pdf.setFillColor(CODE_BG)
        self.pdf.rect(x, H - top - height, width, height, fill=1, stroke=0)
        self.text(Path(relative).name + " / " + function, x + 12, top + 7, 8.5, QUIET)
        self.line(x + 12, top + 26, x + width - 12, top + 26)
        for i, (number, code) in enumerate(displayed):
            y = H - top - 36 - i * leading
            if any(value in code for value in highlight):
                self.pdf.setFillColor(HIGHLIGHT)
                self.pdf.rect(x + 40, y - 3, width - 49, leading, fill=1, stroke=0)
            self.pdf.setFont("Code", size)
            self.pdf.setFillColor(QUIET)
            if number not in (None, ""):
                self.pdf.drawRightString(x + 35, y, str(number))
            self.pdf.setFillColor(INK)
            self.pdf.drawString(x + 46, y, code)
        return height

    def source(self, value):
        self.text(value, 44, 552, 8, QUIET)

    def arrow(self, x1, y1, x2, y2, color=BLUE, width=1.3, dashed=False):
        self.line(x1, y1, x2, y2, color, width, dashed)
        angle = math.atan2(y2 - y1, x2 - x1)
        for delta in (-0.5, 0.5):
            self.line(x2, y2, x2 - 7 * math.cos(angle + delta),
                      y2 - 7 * math.sin(angle + delta), color, width)

    def box(self, title, detail, x, top, width, height=61, fill=None):
        self.pdf.setStrokeColor(RULE)
        self.pdf.setFillColor(fill or colors.white)
        self.pdf.rect(x, H - top - height, width, height, fill=1, stroke=1)
        self.text(title, x + 12, top + 8, 12, BLUE, True)
        if detail:
            self.paragraph(detail, x + 12, top + 30, width - 24, 10, leading=14)

    def polygon(self, points, fill=CODE_BG, stroke=BLUE):
        path = self.pdf.beginPath()
        path.moveTo(points[0][0], H - points[0][1])
        for x, y in points[1:]:
            path.lineTo(x, H - y)
        path.close()
        self.pdf.setFillColor(fill)
        self.pdf.setStrokeColor(stroke)
        self.pdf.drawPath(path, fill=1, stroke=1)

    def dot(self, x, top, radius=3, color=ORANGE):
        self.pdf.setFillColor(color)
        self.pdf.circle(x, H - top, radius, fill=1, stroke=0)

    def cover(self):
        self.begin("アズレイド", cover=True)
        self.pdf.setFillColor(BLUE)
        self.pdf.setFont("JP", 12)
        self.pdf.drawCentredString(W / 2, H / 2 + 73, "3Dレールシューティング")
        self.pdf.setFillColor(INK)
        self.pdf.setFont("JP", 16)
        self.pdf.drawCentredString(W / 2, H / 2 - 28, "プログラム説明資料")
        self.line(W / 2 - 112, H / 2 + 65, W / 2 + 112, H / 2 + 65)
        self.pdf.setFont("JP", 12)
        self.pdf.drawCentredString(W / 2, H / 2 - 112, "小泉 羚")
        self.pdf.setFillColor(QUIET)
        self.pdf.setFont("JP", 10)
        self.pdf.drawCentredString(W / 2, H / 2 - 137, self.account)

    def contents(self):
        self.begin("目次")
        self.text("内容", 60, 114, 10, QUIET)
        self.text("ページ", 742, 114, 10, QUIET)
        for i, (title, page) in enumerate(CONTENTS):
            top = 150 + i * 38
            label = f"{i + 1}. {title}" if i < 8 else title
            self.text(label, 60, top, 14)
            self.pdf.setFillColor(QUIET)
            self.pdf.setFont("JP", 12)
            self.pdf.drawRightString(778, H - top - 15, str(page))
            self.line(60, top + 29, 778, top + 29, RULE, 0.5)
            self.pdf.linkAbsolute("", f"page{page}", Rect=(60, H - top - 29, 778, H - top + 1))

    def overview(self):
        self.begin("1. " + CONTENTS[0][0])
        self.paragraph("自動で前進しながら照準を動かし、敵編隊とボスを撃破する3Dレールシューティングです。\nGameSceneが更新と描画を呼び出し、GameRuntimeが自機・敵・弾・カメラを管理します。", 44, 102, 752)
        self.box("GameScene", "シーンの開始・終了・遷移要求", 44, 172, 218)
        self.arrow(274, 203, 317, 203)
        self.box("GameRuntime", "プレイ中の更新と戦闘の進行", 328, 172, 468)
        self.caption("Update() / Draw() を呼び出す", 52, 249, 245)
        self.line(562, 233, 562, 268, BLUE, 1.1)
        self.line(168, 268, 684, 268, BLUE, 1.1)
        for x in (168, 426, 684):
            self.arrow(x, 268, x, 291)
        self.box("Player", "自機の移動・姿勢・HP", 44, 296, 240)
        self.box("Enemy", "各敵の行動とHP", 302, 296, 238)
        self.box("BulletPool → Bullet", "弾の再利用 → 個々の弾の移動", 558, 296, 238)
        self.caption("主なクラスを抜粋しています。実線は、更新の呼び出しやオブジェクトの保持を表しています。", 44, 368)
        self.heading("射撃したときの流れ", 44, 405, 13)
        names = [("入力・照準", "狙う方向を求める"), ("弾の取得", "待機中の弾を再利用"),
                 ("移動・命中", "球同士で判定"), ("ダメージ・演出", "HP・得点・見た目を更新")]
        for i, (title, detail) in enumerate(names):
            x = 44 + i * 194
            self.box(title, detail, x, 437, 170, 59)
            if i < 3:
                self.arrow(x + 174, 466, x + 188, 466)
        self.caption("命中判定はGameRuntimeCollision.cpp、弾の取得・返却はBulletPool.cppで行います。", 44, 518)
        self.source("参照：GameScene.cpp / GameRuntime.h / GameRuntimeCollision.cpp")

    def entry(self):
        self.begin("2. " + CONTENTS[1][0])
        self.paragraph("ステージ選択からゲーム開始へ移る際、背景や機体の位置が急に変わるのが不自然でした。\n同じステージを描き続け、機体が隙間を抜けた後に通常の飛行へつなげています。", 44, 102, 752)
        self.image("portal.png", 44, 163, 360, 176, crop=(1, 100, 1280, 626))
        self.image("start.png", 436, 163, 360, 176, crop=(1, 100, 1280, 626))
        self.caption("隙間へ向かう機体を、後ろから追う", 44, 348, 360)
        self.caption("通過後も同じ機体と背景のまま降下する", 436, 348, 360)
        self.heading("通過前：位置と速度を決めて補間", 44, 389, 12.5)
        self.paragraph("エルミート補間で通過点をつなぎます。\n各点の速度も指定し、急な方向転換を抑えています。", 44, 415, 347, 11)
        self.heading("通過後：出口の速度を引き継ぐ", 436, 389, 12.5)
        self.paragraph("出口の位置と速度を引き継ぎ、5次曲線で通常の飛行へつなぎます。\nカメラも進行方向に合わせ、追走距離を徐々に戻します。", 436, 415, 360, 11)
        self.text("飛行軌道の模式図", 44, 493, 9, QUIET)
        path = self.pdf.beginPath()
        path.moveTo(166, H - 518)
        path.curveTo(229, H - 478, 295, H - 515, 381, H - 514)
        path.curveTo(465, H - 513, 484, H - 534, 590, H - 534)
        self.pdf.setStrokeColor(BLUE)
        self.pdf.setLineWidth(2)
        self.pdf.drawPath(path)
        self.line(381, 487, 381, 542, RULE, 1, True)
        self.dot(381, 514)
        self.arrow(385, 514, 432, 514, ORANGE)
        self.text("隙間の出口", 313, 478, 9, QUIET)
        self.text("接続点の速度を引き継ぐ", 446, 489, 10, ORANGE)
        self.text("通常の前進へ", 610, 522, 10, BLUE)
        self.source("参照：GameRuntimeEntry.cpp / StageFlightSpline・StageFlightApproach・UpdateStageOverview")

    def defeat(self):
        self.begin("3. " + CONTENTS[2][0])
        self.paragraph("撃墜時に機体が一瞬で消えると、壊れた様子が伝わりにくいため、部品が残るようにしました。\n失速 → 分解 → 接地の順に進め、残骸が見える状態でリザルトを表示します。", 44, 102, 752)
        shots = [("defeat-fall.jpg", "失速して傾く", (480, 300, 440, 270)),
                 ("defeat-break.jpg", "胴体・機首・左右の翼へ分解", (480, 300, 440, 270)),
                 ("defeat-ground.jpg", "路面に部品が残る", (480, 300, 440, 270))]
        for i, (name, label, crop) in enumerate(shots):
            x = 44 + i * 258
            self.image(name, x, 165, 236, 145, crop=crop)
            self.heading(str(i + 1) + "　" + label, x, 321, 10.7)
        self.heading("モデルを4つの部品に分ける", 44, 365, 13)
        self.paragraph("元モデルの頂点・UV・材質を使い、三角形の位置に応じて胴体・機首・左右の翼に分けます。\n部品のモデルはゲーム開始時に用意し、撃墜時の生成を避けています。", 44, 395, 350, 11)
        self.heading("回転後の形状で接地位置を求める", 436, 365, 12)
        self.paragraph("部品の頂点を回転させ、最も低い点が地面にめり込まないように位置を補正します。", 436, 395, 360, 11)
        self.polygon([(492, 476), (620, 452), (662, 486), (537, 509)])
        self.line(461, 509, 764, 509, QUIET, 1.2)
        self.dot(537, 509)
        self.text("回転後の最下点", 645, 470, 10, ORANGE)
        self.arrow(648, 489, 543, 508, ORANGE, 1)
        self.text("地面", 734, 515, 9, QUIET)
        self.caption("初速・回転速度・反発の強さをランダムに変え、\n毎回同じ壊れ方にならないようにしています。", 44, 487, 350)
        self.source("参照：GameRuntimeDefeat.cpp / InitializePlayerDefeatDebris・DebrisBottom・UpdatePlayerDefeat")

    def fire(self):
        self.begin("4. " + CONTENTS[3][0])
        self.paragraph("炎が部品とは別の場所に浮いて見えたため、火元の位置と画像の動かし方を見直しました。\n炎の画像には配布素材を使用し、部品への配置とアニメーションの処理を実装しています。", 44, 102, 752)
        self.image("defeat-ground.jpg", 44, 163, 280, 187, crop=(540, 330, 420, 280))
        self.note("胴体の表面を火元にする", 53, 316,
                  44 + (722 - 540) * 280 / 420, 163 + (449 - 330) * 187 / 280, ORANGE)
        self.heading("部品の回転に合わせて火元を移す", 358, 165, 13)
        self.paragraph("部品の三角形から、表側・裏側の火元の候補を選びます。\n回転後に上側になる点を使います。", 358, 194, 438, 11)
        self.polygon([(376, 281), (467, 263), (486, 286), (395, 302)])
        self.dot(426, 280)
        self.polygon([(575, 263), (632, 287), (676, 270), (618, 246)])
        self.dot(624, 264)
        self.arrow(503, 279, 551, 279)
        self.text("回転前", 395, 312, 10, QUIET)
        self.text("回転後", 608, 312, 10, QUIET)
        self.text("●：火元の候補（模式図）", 358, 338, 9, QUIET)
        self.heading("画像の下端を固定し、上部だけを揺らす", 44, 384, 12)
        self.paragraph("上部ほどUVを大きくずらし、火元になる下端は動かさないようにしています。", 44, 413, 318, 10.7)
        self.heading("隣り合うコマを混ぜ、切り替わりを滑らかにする", 394, 384, 12)
        relative = "project/shaders/Object3d.PS.hlsl"
        rows = excerpt(relative, "float3 emission = lerp(SampleFlameFrame", count=2, after="float4 DrawFlame")
        self.code(relative, "DrawFlame", rows, 394, 415, 402, 8.9, highlight=("lerp(",))
        texture = WORKSPACE / "project/resources/effects/wreck_fire_flipbook.png"
        for i, frame in enumerate((26, 27)):
            self.image(texture, 64 + i * 101, 466, 39, 62.4,
                       crop=(frame % 16 * 128, frame // 16 * 1024 / 5, 128, 1024 / 5))
        self.text("コマ n", 59, 533, 9, QUIET)
        self.text("コマ n + 1", 151, 533, 9, QUIET)
        self.arrow(218, 501, 272, 501, ORANGE)
        self.paragraph("フリップブック\nコマの間を補間", 286, 489, 240, 10, leading=16)
        self.caption("lerpで、現在のコマと次のコマの色を混ぜます。", 394, 505, 402)
        self.source("配置：GetDefeatFireOrigin（GameRuntimeDefeat.cpp）／ 炎素材：同梱のwreck_fire_LICENSE.txt参照")

    def aim(self):
        self.begin("5. " + CONTENTS[4][0])
        self.paragraph("照準は2Dの画面座標、自機と敵は3D空間の座標で管理しています。\nカメラと同じ向きに弾を飛ばすだけでは、銃口の位置によって照準からずれます。", 44, 102, 752)
        self.heading("画面の座標から3D空間の方向を求める", 44, 164, 12.5)
        steps = [("1", "照準の画面座標を変換", "画面座標を -1～1 の範囲に変換する"),
                 ("2", "逆射影で手前・奥の2点を求める", "ビュー射影行列の逆行列を使う"),
                 ("3", "2点を結び、照準が指す方向を求める", "敵の奥行きから、3Dの狙い位置を決める")]
        for i, (number, title, detail) in enumerate(steps):
            top = 201 + i * 77
            self.text(number, 44, top, 17, BLUE, True)
            self.text(title, 72, top, 11.5, INK, True)
            self.text(detail, 72, top + 24, 10, QUIET)
        self.caption("上から見た位置関係（模式図）", 488, 166, 307)
        self.box("カメラ", "", 494, 203, 95, 36)
        self.dot(513, 335, 5, BLUE)
        self.text("自機の銃口", 483, 351, 10)
        self.dot(750, 263, 7, ORANGE)
        self.text("狙い位置", 713, 280, 10, ORANGE)
        self.arrow(593, 220, 742, 263, QUIET, 1.1, True)
        self.arrow(516, 334, 742, 266, BLUE, 1.8)
        self.arrow(516, 334, 772, 408, RULE, 1.1, True)
        self.text("平行に撃つと照準から外れる", 570, 384, 10, QUIET)
        self.heading("銃口から狙い位置へのベクトルを正規化する", 44, 440, 13)
        relative = "project/src/app/runtime/GameRuntime.cpp"
        rows = excerpt(relative, "return Math::Normalize({ aimPoint.x - origin.x", count=1,
                       after="Math::Vector3 GameRuntime::CalculateAimDirection")
        self.code(relative, "CalculateAimDirection", rows, 44, 470, 752, 9.8,
                  highlight=("Math::Normalize",))
        self.source("aimPoint：3Dの狙い位置　origin：銃口の位置　／　ロック中の敵がいない場合は、一定の奥行きで狙い位置を決める")

    def splash(self):
        self.begin("6. " + CONTENTS[5][0])
        self.paragraph("通常射撃だけでは単調だったため、チャージ弾に爆風を付けました。\n狙った敵の周囲も巻き込めるよう、直撃と爆風の判定を分けています。", 44, 102, 752)
        crop = (400, 270, 700, 321)
        self.image("charge-before.png", 44, 165, 360, 165, crop=crop)
        self.image("charge-hit.png", 436, 165, 360, 165, crop=crop)
        self.caption("命中前：中央の敵を狙う", 44, 339, 360)
        self.caption("命中後：小型機3機を巻き込んで撃破", 436, 339, 360)
        self.heading("爆風の中心からの距離で選ぶ", 44, 382, 13)
        self.paragraph("爆風の半径内にいる敵だけにダメージを与えます。\n画面上で重なっていても、3D空間で離れている敵には届きません。", 44, 411, 357, 11)
        self.paragraph("直撃した敵は爆風の対象から外し、ダメージの重複を防ぎます。\n爆風による連鎖爆発も発生させません。", 44, 482, 357, 10.5)
        self.pdf.saveState()
        self.pdf.setStrokeColor(RULE)
        self.pdf.setDash(3, 3)
        self.pdf.circle(550, H - 448, 59, fill=0, stroke=1)
        self.pdf.restoreState()
        self.dot(550, 448, 6, ORANGE)
        self.dot(592, 426, 5, BLUE)
        self.dot(702, 417, 5, QUIET)
        self.arrow(553, 447, 590, 427, ORANGE, 1)
        self.text("直撃対象", 519, 458, 10, ORANGE)
        self.text("範囲内", 607, 420, 10, BLUE)
        self.text("範囲外", 717, 409, 10, QUIET)
        self.caption("判定の模式図。実際は3D空間の距離を使います。", 436, 521, 360)
        self.source("比較用に敵配置を固定。判定：GameRuntimeCollision.cpp / CheckBulletEnemyCollisions・ApplyChargeSplash")

    def reuse(self):
        self.begin("7. " + CONTENTS[6][0])
        self.paragraph("連射中に生成・破棄を繰り返さないよう、オブジェクトプールで弾を再利用しています。\n弾と管理用リストのノードをゲーム開始時に用意し、発射時に待機中から使用中へ移します。", 44, 102, 752)
        self.heading("発射前", 66, 173, 12)
        self.heading("発射後", 454, 173, 12)
        for base in (66, 454):
            self.text("待機中", base, 210, 11, QUIET)
            self.text("使用中", base, 267, 11, QUIET)
        for i, label in enumerate(("A", "B", "C")):
            self.box(label, "", 142 + i * 65, 202, 48, 32)
        for i, label in enumerate(("A", "B")):
            self.box(label, "", 531 + i * 65, 202, 48, 32)
        self.box("C", "", 531, 259, 48, 32, fill=colors.HexColor("#e4eef5"))
        self.arrow(348, 244, 413, 244, BLUE, 1.6)
        self.caption("末尾の弾Cをコピーせず、使用中のリストへ移す。", 66, 310, 330)
        self.arrow(600, 276, 700, 276, ORANGE)
        self.arrow(700, 276, 700, 217, ORANGE)
        self.arrow(700, 217, 667, 217, ORANGE)
        self.caption("寿命切れ・画面外に出た弾を返却", 590, 310, 207)
        rows = excerpt("project/src/app/combat/BulletPool.cpp", "const auto entry = std::prev(available_.end());", count=2)
        self.code("project/src/app/combat/BulletPool.cpp", "Acquire", rows, 44, 351, 752, 11,
                  highlight=("splice(",))
        self.caption("spliceでノードそのものを移し、弾のアドレスを変えずに管理先を切り替えます。", 44, 431)
        self.heading("返す前に、関連する情報も解除する", 44, 468, 12.5)
        self.paragraph("追尾対象や回避済み判定を解除してから返却し、次の弾に前回の情報を残さないようにしています。\n空きがない場合は新規確保せず、取得失敗を記録します。弾の初期化は取得側で行います。", 44, 496, 752, 10.7)
        self.source("参照：BulletPool.cpp / GameRuntime.cpp　所有にはunique_ptrを使い、利用側にはポインタを返す")

    def structure(self):
        self.begin("8. " + CONTENTS[7][0])
        self.paragraph("GameRuntimeに集中していた状態と処理を、役割ごとのクラスに分けています。\n敵を撃破したときは、得点の更新と撃破エフェクトの呼び出しを別々に行います。", 44, 102, 752)
        self.box("GameRuntime", "OnEnemyDestroyed() から呼び分ける", 44, 168, 252, 65)
        self.arrow(310, 188, 367, 188)
        self.arrow(310, 222, 367, 271)
        self.box("CombatProgress", "RegisterDefeat() / AddScore()：連続撃破と得点", 380, 164, 416, 62)
        self.box("CombatEffects", "EnemyDestroyed()：見た目と寿命", 380, 246, 416, 62)
        self.caption("得点はAddScore()、演出はAddEnemyHitEffect()を経由します。", 380, 318, 416)
        self.heading("主なクラスと管理する情報", 44, 354, 13)
        rows = [
            ("CombatProgress", "スコア・連続撃破・倍率"),
            ("FeverState", "ゲージ・発動中の残り時間"),
            ("BulletPool", "使用中・待機中の弾"),
            ("RailScenery", "背景の配置と可視判定"),
        ]
        for i, (name, description) in enumerate(rows):
            y = 389 + i * 29
            self.text(name, 44, y, 10.5, BLUE, True)
            self.text(description, 200, y, 10.5)
            self.line(44, y + 23, 432, y + 23, RULE, 0.5)
        self.heading("GameRuntimeに残っている処理", 477, 354, 13)
        self.paragraph("HUD・ポーズ・出撃演出の状態は、まだGameRuntimeに残っています。\n画面表示と入力が混ざる部分を引き続き整理します。", 477, 386, 319, 11)
        self.heading("整理の方針", 477, 471, 12)
        self.paragraph("描画や演出のクラスから、得点やHPを直接変更しないようにしています。", 477, 499, 319, 10.5)
        self.source("関係図は一部を抜粋。各クラスはGameRuntimeのメンバとして保持しています。")

    def summary(self):
        self.begin(CONTENTS[8][0])
        rows = (
            ("出撃・破壊の演出",
             "機体の位置と速度を出撃後の飛行に引き継ぎ、カメラも通常の追走へ戻しています。\n撃墜後は部品を残し、回転後の形状に合わせて接地位置と火元を求めています。"),
            ("射撃・弾の管理",
             "照準から3D空間の発射方向を求め、チャージ弾には範囲攻撃を追加しました。\n弾をプールから取り出して再利用し、連射中の生成・破棄を避けています。"),
            ("クラスの整理",
             "得点・フィーバー・弾・背景の管理を、役割ごとのクラスに分けています。\n演出とゲームのルールを分け、変更する箇所を見つけやすくしています。"),
        )
        for i, (title, detail) in enumerate(rows):
            top = 142 + i * 108
            self.heading(title, 44, top, 13)
            self.paragraph(detail, 226, top, 570, 11.3)
            self.line(44, top + 80, 796, top + 80, RULE, 0.6)
        self.heading("次に進めること", 44, 472, 13)
        self.paragraph("敵配置とスキルのバランス調整、新規ステージの制作を進めます。\nあわせて、GameRuntimeに残るHUD・ポーズ・出撃演出の処理を整理します。", 44, 503, 752, 11)

    def build(self):
        for section in (self.cover, self.contents, self.overview, self.entry,
                        self.defeat, self.fire, self.aim, self.splash,
                        self.reuse, self.structure, self.summary):
            section()
        assert self.page == TOTAL_PAGES
        self.pdf.save()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--account", required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    Document(args.output, args.account, args.assets).build()
    print("PROGRAM_DOCUMENT_REVISED_OK pages=11 source_excerpts=3")


if __name__ == "__main__":
    main()
