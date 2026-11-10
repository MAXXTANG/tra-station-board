from PIL import Image,ImageDraw
from pathlib import Path
import re,json
p=Path(__file__).resolve().parents[1];s=(p/'include/font16.h').read_text();codes=list(map(int,re.search(r'FONT_CODES\[\]=\{(.*?)\}',s,re.S)[1].split(',')));bits=list(map(int,re.search(r'FONT_BITS\[\]=\{(.*?)\}',s,re.S)[1].split(',')));glyphs={chr(c):bits[i*32:(i+1)*32] for i,c in enumerate(codes)}
colors={'bg':'#080c08','white':'#f7f7f2','amber':'#f0b568','gray':'#83918a'}
def render(north=True,page=0):
 im=Image.new('RGB',(320,170),colors['bg']);d=ImageDraw.Draw(im)
 def text(t,x,y,c='white',width=320):
  start=x
  for ch in t:
   w=8 if ord(ch)<127 else 16
   if x+w>start+width:break
   b=glyphs.get(ch,[0]*32)
   for yy in range(16):
    row=(b[yy*2]<<8)|b[yy*2+1]
    for xx in range(w):
     if row&(0x8000>>xx):d.point((x+xx,y+yy),colors.get(c,c))
   x+=w
 d.rectangle((0,0,319,25),fill=colors['white'])
 for x,t in [(8,'時間'),(65,'開往'),(130,'車種'),(195,'車次'),(264,'備註')]:text(t,x,4,'bg')
 rows=[('12:58','樹林','自強','175','準點','#bd5c24'),('13:19','臺北','新自強','217','晚5分','#be2129'),('13:31','七堵','區間快','4037','待更新','#166baf')] if north else [('12:53','蘇澳','區間','4182','準點','#166baf'),('13:16','花蓮','新自強','218','晚3分','#be2129'),('13:35','臺東','自強','176','準點','#bd5c24')]
 if page:
  rows=[(f'{14+(page-1)//3:02d}:{((page-1)%3)*18+i*5:02d}',dest,typ,str(int(no)+page*10),r,c) for i,(tm,dest,typ,no,r,c) in enumerate(rows)]
 for i,(tm,dest,typ,no,r,c) in enumerate(rows):
  y=29+i*29;d.line((0,y+26,319,y+26),fill='#283d2d');text(tm,4,y+4,width=54);text(dest,60,y+4,width=64);d.rectangle((126,y+1,187,y+22),fill=c);text(typ,129,y+4,width=58);text(no,192,y+4,width=44);text(r,240,y+4,'#d5ec45' if r=='準點' else 'amber',80)
 text('示範資料・非即時',4,119,'gray',264);text(f'{page+1}/5',276,119,'gray',44)
 d.rectangle((0,138,319,169),fill='#102014');text('12:45:08',4,147,'amber');text('礁溪站',78,147,width=84)
 for x,t,active in [(170,'北上',north),(244,'南下',not north)]:
  d.rounded_rectangle((x,138,x+69,169),radius=3,fill=colors['amber'] if active else '#202620');text(t,x+19,147,'bg' if active else 'white')
 return im
north,south=render(),render(False)
north.resize((960,510),Image.Resampling.NEAREST).save(p/'board-preview.png')
south.resize((960,510),Image.Resampling.NEAREST).save(p/'board-preview-south.png')
used=set('礁溪站北上南下時間開往車種備註樹林自強臺新晚分七堵區間快待更新蘇澳花蓮東準點示範資料非即時・0123456789:/ ')
compact={c:glyphs[c] for c in used if c in glyphs}
html='''<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>臺鐵小看板 · 介面預覽</title><style>body{background:#111916;color:#f4f6ef;font:16px system-ui;margin:0;padding:6vh 5vw}main{max-width:960px;margin:auto}h1{font-size:32px}p{color:#b5c4bb;line-height:1.8}canvas{width:100%;image-rendering:pixelated;border-radius:8px;outline:10px solid #29362f;cursor:pointer;margin:30px 0}button{background:#e1b967;color:#17271d;border:0;border-radius:7px;padding:12px 26px;margin-right:12px;font:inherit}.tag{color:#e1b967;letter-spacing:3px}</style><main><span class="tag">TRA · DESKTOP DEPARTURES</span><h1>下一班，從礁溪出發。</h1><p>320 × 170 橫向介面，參考實體臺鐵資訊看板。站名與方向切換位於底部，時間在左下角。點列車區域可翻頁，每頁三班。</p><canvas id="board" width="320" height="170"></canvas><div><button onclick="show(true,0)">北上</button><button onclick="show(false,0)">南下</button><button onclick="nextPage()">下一頁</button></div><p>介面示意，所有車次與時間均為測試資料，非即時資訊。實機會透過 TDX 取得所選車站資料；配網頁可選 246 個臺鐵車站。</p></main><script>'''
# Embed verified pixel previews; this artifact works without a server or external assets.
import base64,io
images=[]
for direction in [True,False]:
 pages=[]
 for page in range(5):
  b=io.BytesIO();render(direction,page).save(b,format='PNG');pages.append('data:image/png;base64,'+base64.b64encode(b.getvalue()).decode())
 images.append(pages)
html+='const sources='+json.dumps(images)+';let north=true,page=0;const c=document.querySelector("canvas"),ctx=c.getContext("2d");function show(n,p=0){north=n;page=p;const im=new Image;im.onload=()=>ctx.drawImage(im,0,0);im.src=sources[n?0:1][p]}function nextPage(){show(north,(page+1)%5)}c.onclick=e=>{const r=c.getBoundingClientRect(),x=(e.clientX-r.left)*320/r.width,y=(e.clientY-r.top)*170/r.height;if(y>=138&&x>=170)show(x<242,0);else if((y>=27&&y<116)||(y>=116&&y<138&&x>=270))nextPage()};show(true,0);</script></html>'
(p/'board-preview.html').write_text(html)
