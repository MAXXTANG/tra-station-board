from pathlib import Path
from PIL import Image,ImageFont,ImageDraw
import json,ssl,socket
import argparse,urllib.request,tempfile
parser=argparse.ArgumentParser();parser.add_argument('--font',required=True,help='Path to NotoSansTC[wght].ttf');parser.add_argument('--refresh-stations',action='store_true');args=parser.parse_args()
p=Path(__file__).resolve().parents[1]
if args.refresh_stations:
 url='https://ods.railway.gov.tw/tra-ods-web/ods/download/dataResource/0518b833e8964d53bfea3f7691aea0ee'
 raw=json.load(urllib.request.urlopen(url,timeout=30))
 stations=[{'id':x['stationCode'],'name':x['stationName'],'region':x['stationAddrTw'][:3]}for x in raw if x['stationCode'].isdigit()]
 (p/'data/stations.json').write_text(json.dumps(stations,ensure_ascii=False,indent=2))
else: stations=json.loads((p/'data/stations.json').read_text())
# Official station IDs are kept as strings, including leading zeroes.
rows=',\n'.join('{'+json.dumps(x['id'])+','+json.dumps(x['name'],ensure_ascii=False)+','+json.dumps(x['region'],ensure_ascii=False)+'}' for x in stations)
(p/'include/stations.h').write_text('#pragma once\nstruct Station {const char *id,*name,*region;};\nstatic const Station STATIONS[]={\n'+rows+'\n};\nconstexpr size_t STATION_COUNT=sizeof(STATIONS)/sizeof(STATIONS[0]);\n')
f=ImageFont.truetype(args.font,16)
f.set_variation_by_axes([500])
# Complete basic CJK block: destinations are not limited to the initially selected station.
codes=list(range(32,127))+list(range(0x3000,0x3040))+list(range(0x4e00,0xa000))+[0xff08,0xff09,0xff1a,0xff0c,0xff1f]
codes=sorted(set(codes+[0xb7,0x2026,0x30fb]))
buf=[]
for c in codes:
 im=Image.new('L',(16,16));d=ImageDraw.Draw(im);box=f.getbbox(chr(c));d.text((0,-5),chr(c),font=f,fill=255)
 if c<127: im=im.crop((0,0,max(8,int(f.getlength(chr(c))+.5)),16)).resize((8,16),Image.Resampling.LANCZOS).crop((0,0,16,16))
 for y in range(16):
  row=0
  for x in range(16): row=(row<<1)|int(im.getpixel((x,y))>=96)
  buf.extend([row>>8,row&255])
(p/'include/font16.h').write_text('#pragma once\n#include <stdint.h>\nstatic const uint16_t FONT_CODES[]={'+','.join(map(str,codes))+'};\nstatic const uint8_t FONT_BITS[]={\n'+',\n'.join(','.join(str(x) for x in buf[i:i+64]) for i in range(0,len(buf),64))+'\n};\n')
ctx=ssl.create_default_context()
selected=[]
for info,der in zip(ctx.get_ca_certs(),ctx.get_ca_certs(binary_form=True)):
 if any('TWCA' in value for group in info['subject'] for _,value in group):
  print(info['subject']);selected.append(ssl.DER_cert_to_PEM_cert(der))
pem=''.join(selected)
assert pem
# Verify that these roots really validate the live endpoint before embedding them.
verify=ssl.create_default_context(cadata=pem)
with verify.wrap_socket(socket.socket(),server_hostname='tdx.transportdata.tw') as s:
 s.settimeout(15);s.connect(('tdx.transportdata.tw',443));print('TDX TLS roots verified')
(p/'include/tdx_ca.h').write_text('#pragma once\nstatic const char TDX_CA[] = R"CERT('+pem+')CERT";\n')
print('Generated',len(stations),'stations;',len(codes),'glyphs')
