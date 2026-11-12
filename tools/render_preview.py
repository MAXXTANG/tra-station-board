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
 for x,t in [(16,'時間'),(98,'開往'),(190,'車種'),(268,'車次')]:text(t,x,4,'bg')
 rows=[('12:58','樹林','自強','175','#bd5c24'),('13:19','臺北','新自強','217','#be2129'),('13:31','七堵','區間快','4037','#166baf')] if north else [('12:53','蘇澳','區間','4182','#166baf'),('13:16','花蓮','新自強','218','#be2129'),('13:35','臺東','自強','176','#bd5c24')]
 if page:
  rows=[(f'{14+(page-1)//3:02d}:{((page-1)%3)*18+i*5:02d}',dest,typ,str(int(no)+page*10),c) for i,(tm,dest,typ,no,c) in enumerate(rows)]
 for i,(tm,dest,typ,no,c) in enumerate(rows):
  y=29+i*29;d.line((0,y+26,319,y+26),fill='#283d2d');text(tm,8,y+4,width=56);text(dest,70,y+4,width=90);d.rectangle((164,y+1,243,y+22),fill=c);text(typ,174,y+4,width=64);text(no,260,y+4,width=56)
 text('更新時間 12:45',4,119,'gray',264);text(f'{page+1}/5',276,119,'gray',44)
 d.rectangle((0,138,319,169),fill='#102014');text('12:45:08',4,147,'amber');text('礁溪站',78,147,width=84)
 for x,t,active in [(170,'北上',north),(244,'南下',not north)]:
  d.rounded_rectangle((x,138,x+69,169),radius=3,fill=colors['amber'] if active else '#202620');text(t,x+19,147,'bg' if active else 'white')
 return im
def render_picker():
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
 text('常用車站',8,5,'amber')
 for x,t in [(208,'全部'),(264,'返回')]:d.rounded_rectangle((x,1,x+51,27),radius=3,fill='#202620');text(t,x+10,6)
 for i,name in enumerate(['礁溪','宜蘭','羅東','臺北','樹林','花蓮']):
  x=6+(i%2)*158;y=32+(i//2)*26;selected=i==0
  d.rounded_rectangle((x,y,x+149,y+22),radius=3,fill=colors['amber'] if selected else '#202620');text(name,x+7,y+3,'bg' if selected else 'white',136)
 d.rectangle((0,140,319,169),fill='#102014');text('上一頁',16,147);text('1/1',144,147,'gray');text('下一頁',256,147)
 return im
render_picker().resize((960,510),Image.Resampling.NEAREST).save(p/'station-picker-preview.png')
north,south=render(),render(False)
north.transpose(Image.Transpose.ROTATE_180).resize((960,510),Image.Resampling.NEAREST).save(p/'board-preview-inverted.png')
north.resize((960,510),Image.Resampling.NEAREST).save(p/'board-preview.png')
south.resize((960,510),Image.Resampling.NEAREST).save(p/'board-preview-south.png')
used=set('礁溪站北上南下時間開往車種樹林自強臺新七堵區間快更新蘇澳花蓮東表定每分示範資料・0123456789:/ ')
compact={c:glyphs[c] for c in used if c in glyphs}
html='''<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>臺鐵小看板 · 介面預覽</title><style>body{background:#111916;color:#f4f6ef;font:16px system-ui;margin:0;padding:6vh 5vw}main{max-width:960px;margin:auto}h1{font-size:32px}p{color:#b5c4bb;line-height:1.8}canvas{width:100%;image-rendering:pixelated;border-radius:8px;outline:10px solid #29362f;cursor:pointer;margin:30px 0}button{background:#e1b967;color:#17271d;border:0;border-radius:7px;padding:12px 26px;margin-right:12px;font:inherit}canvas.inverted{transform:rotate(180deg)}.tag{color:#e1b967;letter-spacing:3px}</style><main><span class="tag">TRA · DESKTOP DEPARTURES</span><h1>下一班，從礁溪出發。</h1><p>320 × 170 橫向介面，參考實體臺鐵資訊看板。站名與方向切換位於底部，時間在左下角。點列車區域可翻頁，每頁三班。</p><canvas id="board" width="320" height="170"></canvas><div><button onclick="show(true,0)">北上</button><button onclick="show(false,0)">南下</button><button onclick="nextPage()">下一頁</button><button onclick="openPicker()">切換車站</button><button id="rotate" onclick="rotateScreen()" aria-pressed="false">旋轉 180°</button></div><p>介面示意，車次與時間為示範資料。實機每 2 分鐘檢查臺鐵 OpenData，顯示表定時刻，無準點／誤點欄位；配網頁可選 246 個臺鐵車站。</p></main><script>'''
# Embed verified pixel previews; this artifact works without a server or external assets.
import base64,io
images=[]
for direction in [True,False]:
 pages=[]
 for page in range(5):
  b=io.BytesIO();render(direction,page).save(b,format='PNG');pages.append('data:image/png;base64,'+base64.b64encode(b.getvalue()).decode())
 images.append(pages)
stations=json.loads((p/'data/stations.json').read_text())
needed=set(''.join(x['name']+x['region']for x in stations)+'常用車站選擇縣市全部返回上一頁下一頁礁溪站0123456789/')
font={c:glyphs[c] for c in needed if c in glyphs}
html+='const sources='+json.dumps(images)+';const stations='+json.dumps(stations,ensure_ascii=False)+';const font='+json.dumps(font,ensure_ascii=False)+';'
html+=r'''let north=true,page=0,inverted=false,view='board',pickerPage=0,county='',chosen='7210';
const c=document.querySelector('canvas'),ctx=c.getContext('2d'),recent=['7210',...['宜蘭','羅東','臺北','樹林','花蓮'].map(name=>stations.find(s=>s.name===name)?.id)].filter(Boolean);
const counties=[...new Set(stations.map(s=>s.region))];
function text(s,x,y,color='#f7f7f2',width=320){const start=x;ctx.fillStyle=color;for(const ch of s){const w=ch.codePointAt(0)<127?8:16;if(x+w>start+width)break;const b=font[ch]||[];for(let yy=0;yy<16;yy++){const row=((b[yy*2]||0)<<8)|(b[yy*2+1]||0);for(let xx=0;xx<w;xx++)if(row&(0x8000>>xx))ctx.fillRect(x+xx,y+yy,1,1)}x+=w}}
function button(x,y,w,h,s,active=false){ctx.fillStyle=active?'#f0b568':'#202620';ctx.fillRect(x,y,w,h);text(s,x+7,y+3,active?'#080c08':'#f7f7f2',w-14)}
function options(){return view==='regions'?counties:(view==='recent'?recent.map(id=>stations.find(s=>s.id===id)).filter(Boolean):stations.filter(s=>s.region===county))}
function drawPicker(){ctx.fillStyle='#080c08';ctx.fillRect(0,0,320,170);const list=options(),pages=Math.max(1,Math.ceil(list.length/8));pickerPage=(pickerPage+pages)%pages;text(view==='recent'?'常用車站':view==='regions'?'選擇縣市':county,8,5,'#f0b568',192);if(view==='recent')button(208,1,52,27,'全部');button(264,1,52,27,'返回');list.slice(pickerPage*8,pickerPage*8+8).forEach((s,i)=>button(6+(i%2)*158,32+Math.floor(i/2)*26,150,23,view==='regions'?s:s.name,view!=='regions'&&s.id===chosen));ctx.fillStyle='#102014';ctx.fillRect(0,140,320,30);text('上一頁',16,147);text(`${pickerPage+1}/${pages}`,144,147,'#83918a',40);text('下一頁',256,147)}
function show(n,p=0){north=n;page=p;view='board';const im=new Image;im.onload=()=>{if(view!=='board')return;ctx.drawImage(im,0,0);ctx.fillStyle='#102014';ctx.fillRect(74,138,92,32);text((stations.find(s=>s.id===chosen)?.name||'礁溪')+'站',78,147,'#f7f7f2',84)};im.src=sources[n?0:1][p]}
function openPicker(){view='recent';pickerPage=0;drawPicker()}
function nextPage(){if(view==='board')show(north,(page+1)%5);else{pickerPage++;drawPicker()}}
function rotateScreen(){inverted=!inverted;c.classList.toggle('inverted',inverted);const b=document.querySelector('#rotate');b.setAttribute('aria-pressed',String(inverted));b.textContent=inverted?'恢復正常方向':'旋轉 180°'}
c.onclick=e=>{const r=c.getBoundingClientRect();let x=Math.min(319,Math.floor((e.clientX-r.left)*320/r.width)),y=Math.min(169,Math.floor((e.clientY-r.top)*170/r.height));if(inverted){x=319-x;y=169-y}if(view==='board'){if(y>=138&&x>=74&&x<166)openPicker();else if(y>=138&&x>=170)show(x<242,0);else if((y>=27&&y<116)||(y>=116&&y<138&&x>=270))nextPage();return}if(y<29){if(x>=264){view=view==='stations'?'regions':view==='regions'?'recent':'board';pickerPage=0;if(view==='board')show(north,0);else drawPicker()}else if(view==='recent'&&x>=208){view='regions';pickerPage=0;drawPicker()}return}if(y>=140){if(x<100)pickerPage--;else if(x>=220)pickerPage++;drawPicker();return}if(y<32||y>=136)return;const col=x<160?0:1,row=Math.floor((y-32)/26);if((y-32)%26>=23||x<6||(col===0&&x>=156)||(col===1&&(x<164||x>=314)))return;const list=options(),item=list[pickerPage*8+row*2+col];if(!item)return;if(view==='regions'){county=item;view='stations';pickerPage=0;drawPicker()}else{chosen=item.id;const i=recent.indexOf(chosen);if(i>=0)recent.splice(i,1);recent.unshift(chosen);recent.splice(6);show(north,0)}};
show(true,0);</script></html>'''
(p/'board-preview.html').write_text(html)
