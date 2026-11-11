from pathlib import Path
import math
import json
from PIL import Image
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import Color
from reportlab.lib.utils import ImageReader

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[1]
PDF = OUT / 'プログラム説明資料（SKYBREAK）.pdf'
QA = ROOT / 'tmp/pdfs/review'
QA.mkdir(parents=True, exist_ok=True)
pdfmetrics.registerFont(TTFont('JP', 'C:/Windows/Fonts/YuGothM.ttc', subfontIndex=0))
pdfmetrics.registerFont(TTFont('JPBold', 'C:/Windows/Fonts/YuGothB.ttc', subfontIndex=0))
pdfmetrics.registerFont(TTFont('LatinBold', 'C:/Windows/Fonts/arialbd.ttf'))
W, H = 960, 540
BLACK = Color(0, 0, 0)
WHITE = Color(1, 1, 1)
C = canvas.Canvas(str(PDF), pagesize=(W, H), pageCompression=1)
C.setTitle('SKYBREAK プログラム説明資料')
C.setAuthor('小泉 羚')
checks = []
page_no = 0

def text(s, x, y, size=22, bold=False, font=None):
    font = font or ('JPBold' if bold else 'JP')
    width = pdfmetrics.stringWidth(s, font, size)
    assert x >= 40 and x + width <= W - 40, (page_no, 'horizontal overflow', s)
    assert y >= 30 and y + size <= H - 35, (page_no, 'vertical overflow', s)
    C.setFillColor(BLACK)
    C.setFont(font, size)
    C.drawString(x, H - y - size * .88, s)
    checks.append({'page': page_no, 'text': s, 'size': size, 'x': x, 'y': y})

def lines(s, x, y, size=22, leading=35, bold=False):
    for i, row in enumerate(s.split('\n')):
        text(row, x, y + i * leading, size, bold)

def centered(s, y, size=22, bold=False, font=None):
    font = font or ('JPBold' if bold else 'JP')
    x = (W - pdfmetrics.stringWidth(s, font, size)) / 2
    text(s, x, y, size, bold, font)

def start(title=None):
    global page_no
    page_no += 1
    C.setFillColor(WHITE)
    C.rect(0, 0, W, H, stroke=0, fill=1)
    if title:
        text(title, 60, 48, 32, True)

def end():
    C.showPage()

def line(x1, y1, x2, y2, width=1.25):
    C.setStrokeColor(BLACK)
    C.setLineWidth(width)
    C.line(x1, H - y1, x2, H - y2)

def arrow(x1, y1, x2, y2):
    line(x1, y1, x2, y2)
    angle = math.atan2(y2 - y1, x2 - x1)
    for delta in (-.55, .55):
        line(x2, y2, x2 - 8 * math.cos(angle + delta), y2 - 8 * math.sin(angle + delta))

def box(label, x, y, w, h=66):
    C.setStrokeColor(BLACK)
    C.setLineWidth(1)
    C.rect(x, H - y - h, w, h, fill=0, stroke=1)
    width = pdfmetrics.stringWidth(label, 'JP', 22)
    text(label, x + (w - width) / 2, y + (h - 22) / 2, 22)

def photo(name, x, y, w, h):
    im = Image.open(OUT / 'assets' / name).convert('RGB')
    # 表示用にウィンドウ枠を除く。ゲーム画面の内容は変更しない。
    im = im.crop((1, 31, im.width - 1, im.height - 1))
    ratio = w / h
    if im.width / im.height > ratio:
        crop_width = round(im.height * ratio)
        left = (im.width - crop_width) // 2
        im = im.crop((left, 0, left + crop_width, im.height))
    else:
        crop_height = round(im.width / ratio)
        top = (im.height - crop_height) // 2
        im = im.crop((0, top, im.width, top + crop_height))
    C.drawImage(ImageReader(im), x, H - y - h, w, h)

# 表紙は作品名、資料名、氏名、所属だけにする。
start()
centered('SKYBREAK', 145, 54, font='LatinBold')
centered('プログラム説明資料', 222, 28)
centered('小泉 羚', 351, 24)
centered('日本工学院専門学校', 395, 20)
centered('ゲームクリエイター科', 426, 20)
end()

start('作品概要')
lines('自動で前進する機体を操作し、敵編隊とボスを倒す\n3Dレールシューティングです。', 60, 121)
photo('boss-current.jpg', 60, 222, 480, 270)
text('制作人数：1名（個人制作）', 582, 219, 19)
text('使用言語：C++20', 582, 258, 19)
text('描画：DirectX 12', 582, 297, 19)
text('担当箇所', 582, 354, 23, True)
lines('自機・敵・弾、進行管理、\nUI・演出、描画処理の調整', 582, 395, 19, 30)
text('モデル・フォントは外部素材を使用。', 582, 469, 17)
end()

start('回避処理')
lines('入力してすぐ横へ動き、最後はゆっくり止まる回避にしました。\n移動量にイージングを使い、速度の変化を付けています。', 60, 123)
for i, label in enumerate(('入力を受け取る', '移動・回転・無敵', '通常移動に戻る')):
    x = 60 + i * 294
    box(label, x, 239, 250)
    if i < 2:
        arrow(x + 262, 272, x + 282, 272)
lines('移動・機体の回転・無敵時間は、回避の進み具合に合わせて更新します。', 60, 355, 21)
lines('再使用の直前に押した入力も短時間受け付け、\n続けて回避するときに入力を取りこぼしにくくしました。', 60, 410, 21, 34)
end()

start('敵の攻撃')
lines('予備動作と撃ち終わりの隙を作り、\n弾を避けてから反撃できるようにしました。', 60, 123)
for i, label in enumerate(('狙いを合わせる', '狙いを固定', '連射', '待機')):
    x = 60 + i * 214
    box(label, x, 240, 188)
    if i < 3:
        arrow(x + 194, 273, x + 208, 273)
lines('EnemyFireControlで、攻撃の状態と残り時間を管理しています。\n発射直前に狙いを固定するため、横移動でかわす余地が残ります。', 60, 354, 21, 34)
lines('敵ごとに連射数や待ち時間を変え、同じ仕組みで攻撃に違いを出しています。', 60, 450, 20)
end()

start('ステージの進行管理')
lines('景色の前進と敵の出現を別々に管理し、\n敵を倒す前に次の編隊へ進んでしまう問題を調整しました。', 60, 123)
text('景色の前進', 64, 254, 22, True)
arrow(243, 267, 285, 267)
box('戦闘中も進み続ける', 305, 235, 330)
text('敵の出現', 64, 354, 22, True)
arrow(243, 367, 285, 367)
box('敵がいなくなる', 305, 335, 240)
arrow(562, 367, 600, 367)
box('次の編隊', 618, 335, 240)
lines('敵がいなくなった後は、次の出現までの待ち時間を短くしています。\n出現する順番を保ちながら、何も起きない時間を減らしました。', 60, 435, 20, 32)
end()

start('弾とエフェクトの再利用')
lines('弾やヒット演出を、あらかじめ用意して使い回しています。\nオブジェクトプールを使い、戦闘中の生成・破棄を減らしました。', 60, 123)
for label, x in (('プールから取得', 60), ('初期化して使用', 354), ('終了後に返却', 648)):
    box(label, x, 241, 250)
arrow(322, 274, 342, 274)
arrow(616, 274, 636, 274)
line(773, 323, 773, 351)
line(773, 351, 185, 351)
arrow(185, 351, 185, 323)
lines('再利用する際は、位置・寿命・命中履歴などを初期化し、\n前の弾の状態が残らないようにしています。', 60, 413, 22, 35)
end()

start('スキルとフィーバー')
text('残像連撃', 60, 125, 25, True)
lines('照準に近い敵を選び、順番に攻撃します。\n対象がいないときは発動せず、無駄に消費しないようにしています。', 60, 174, 21, 34)
lines('攻撃の進み具合と再使用待ちを管理し、\n通常射撃の命中や撃破で待ち時間を短縮しています。', 60, 262, 21, 34)
text('フィーバー', 60, 361, 25, True)
lines('ゲージが満タンになると自動で発動します。\n移動速度・攻撃性能・スコア倍率と、画面の演出を切り替えます。', 60, 410, 21, 34)
end()

start('チュートリアルの進行')
lines('説明を表示して終わりではなく、操作ができたことを\n確認してから次へ進むようにしました。', 60, 123)
for y, label, result in (
    (242, '射撃', '標的を撃破したら進む'),
    (303, 'チャージ', 'チャージ弾が命中したら進む'),
    (364, '回避', '近づいてきた敵弾を回避したら進む'),
):
    text(label, 85, y, 23, True)
    arrow(254, y + 12, 289, y + 12)
    text(result, 321, y, 22)
lines('未達成の間は標的を残し、必要なら再配置します。\n本編へ移る際は、HP・スコア・ゲージを初期化しています。', 60, 438, 20, 32)
end()

C.save()
(QA / 'layout-checks.json').write_text(json.dumps(checks, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'pages': page_no, 'bytes': PDF.stat().st_size, 'pdf': str(PDF)}, ensure_ascii=False))
