#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include "rail_model.h"
#include "font16.h"
#include "stations.h"
#include "tdx_ca.h"
#include "portal.h"
#include "display_orientation.h"

struct Config {String ssid,password,station="7210",client,secret; bool flip=false,ns=true,automatic=false,rotate180=false; uint32_t interval=120;} cfg;
TFT_eSPI tft; TFT_eSprite frame(&tft);
Preferences prefs;WebServer server(80);DNSServer dns;
SemaphoreHandle_t modelMutex;
std::vector<rail::Train> trains;
String apiMessage="等待連線",scheduleDate,token;
time_t lastLive=0,tokenExpiry=0;
bool portalActive=false;
uint8_t touchAddress=0;volatile bool touchIRQ=false;
String apName,apPassword,csrf;
uint32_t apStarted=0,rebootAt=0,lastAction=0;
int direction=1,page=0;
constexpr uint16_t BG=0x0841,WHITE=0xf7be,AMBER=0xf5ad,GRAY=0x8410,RED=0xf986;
struct PsramAllocator {void* allocate(size_t n){return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}void deallocate(void*p){heap_caps_free(p);}void* reallocate(void*p,size_t n){return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}};
using Document=BasicJsonDocument<PsramAllocator>;
const Station* stationInfo(){for(const auto &s:STATIONS)if(cfg.station==s.id)return &s;return &STATIONS[0];}
String dateNow(){time_t now=time(nullptr);struct tm t;localtime_r(&now,&t);char b[12];strftime(b,sizeof(b),"%Y-%m-%d",&t);return b;}
void setMessage(const String&s){xSemaphoreTake(modelMutex,portMAX_DELAY);apiMessage=s;xSemaphoreGive(modelMutex);Serial.println(s);}
String htmlEscape(String s){s.replace("&","&amp;");s.replace("<","&lt;");s.replace(">","&gt;");s.replace("\"","&quot;");s.replace("'","&#39;");return s;}
String encoded(const String&s){String out;const char*hex="0123456789ABCDEF";for(size_t i=0;i<s.length();i++){uint8_t c=s[i];if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out+=(char)c;else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
uint32_t nextChar(const char*&p){uint8_t c=*p++;if(c<128)return c;if((c&0xe0)==0xc0){uint32_t a=c&31;return (a<<6)|(*p++&63);}if((c&0xf0)==0xe0){uint32_t a=c&15;a=(a<<6)|(*p++&63);return(a<<6)|(*p++&63);}while((*p&0xc0)==0x80)p++;return '?';}
void label(const char*s,int x,int y,uint16_t color=WHITE,int maxWidth=320){
 int start=x;while(*s){uint32_t c=nextChar(s);int w=c<127?8:16;if(x+w>start+maxWidth)break;
 int lo=0,hi=sizeof(FONT_CODES)/sizeof(FONT_CODES[0])-1,index=-1;while(lo<=hi){int m=(lo+hi)/2;uint16_t v=pgm_read_word(FONT_CODES+m);if(v==c){index=m;break;}if(v<c)lo=m+1;else hi=m-1;}
 if(index>=0)for(int yy=0;yy<16;yy++){uint16_t bits=(pgm_read_byte(FONT_BITS+index*32+yy*2)<<8)|pgm_read_byte(FONT_BITS+index*32+yy*2+1);for(int xx=0;xx<w;xx++)if(bits&(0x8000>>xx))frame.drawPixel(x+xx,y+yy,color);}
 x+=w;}
}
void label(const String&s,int x,int y,uint16_t c=WHITE,int w=320){label(s.c_str(),x,y,c,w);}
void beginPortal(){
 if(portalActive)return;
 WiFi.mode(WIFI_AP_STA);WiFi.softAP(apName.c_str(),apPassword.c_str());dns.start(53,"*",WiFi.softAPIP());portalActive=true;apStarted=millis();
 Serial.printf("Setup AP: %s (password on display)\n",apName.c_str());
}
void loadConfig(){
 prefs.begin("tra-board",false);DynamicJsonDocument d(2048);deserializeJson(d,prefs.getString("config","{}"));
 cfg.ssid=d["ssid"] | "";cfg.password=d["password"] | "";cfg.station=d["station"] | "7210";
 cfg.client=d["client"] | "";cfg.secret=d["secret"] | "";cfg.flip=d["flip"] | false;cfg.ns=d["ns"] | true;cfg.automatic=d["auto"] | false;cfg.interval=d["interval"] | 120;
 cfg.rotate180=(d["rotation"] | 0)==180;
 bool valid=false;for(auto&s:STATIONS)if(cfg.station==s.id)valid=true;if(!valid)cfg.station="7210";
 cfg.interval=constrain(cfg.interval,120U,3600U);
}
void setupWeb(){
 server.on("/",HTTP_GET,[]{
  if(!portalActive){server.send(403,"text/plain","Open setup using the device button.");return;}
  server.sendHeader("Cache-Control","no-store");server.setContentLength(CONTENT_LENGTH_UNKNOWN);server.send(200,"text/html; charset=utf-8",PORTAL_HEAD);
  server.sendContent("<input type='hidden' name='csrf' value='"+csrf+"'><label>Wi-Fi 名稱</label><input name='ssid' required maxlength='32' value='"+htmlEscape(cfg.ssid)+"' autocomplete='off'>");
  server.sendContent(PORTAL_MIDDLE);
  String chunk;chunk.reserve(1600);for(auto&s:STATIONS){chunk+="<option value='"+String(s.id)+"' data-region='"+htmlEscape(s.region)+"'"+(cfg.station==s.id?" selected":"")+">"+htmlEscape(s.name)+" · "+s.id+"</option>";if(chunk.length()>1400){server.sendContent(chunk);chunk="";}}server.sendContent(chunk);
  server.sendContent("</select><script type='application/json' id='current'>{\"labels\":\""+String(cfg.ns?"ns":"cw")+"\",\"flip\":\""+String(cfg.flip?"1":"0")+"\",\"interval\":\""+String(cfg.interval)+"\",\"rotation\":\""+String(cfg.rotate180?180:0)+"\",\"auto\":"+String(cfg.automatic?"true":"false")+"}</script>");
  server.sendContent(PORTAL_TAIL);server.sendContent("");
 });
 server.on("/save",HTTP_POST,[]{
  if(!portalActive||server.arg("csrf")!=csrf){server.send(403,"text/plain","Invalid setup session");return;}
  String ssid=server.arg("ssid"),pw=server.arg("password"),station=server.arg("station"),client=server.arg("client"),secret=server.arg("secret");
  bool valid=false;for(auto&s:STATIONS)if(station==s.id)valid=true;
  bool isOpen=server.hasArg("open");
  if(ssid.length()==0||ssid.length()>32||pw.length()>63||(!pw.isEmpty()&&pw.length()<8)||!valid||client.length()>160||secret.length()>200||(!client.isEmpty()&&client!=cfg.client&&secret.isEmpty())){server.send(400,"text/plain; charset=utf-8","請檢查 Wi-Fi 名稱、密碼（8–63 字元）、車站與 TDX 金鑰。變更 Client ID 時需同步輸入 Secret。");return;}
  if(!isOpen&&pw.isEmpty()&&cfg.password.isEmpty()){server.send(400,"text/plain; charset=utf-8","請輸入 Wi-Fi 密碼，或勾選無密碼 Wi-Fi。");return;}
  if(!isOpen&&pw.isEmpty()&&ssid!=cfg.ssid){server.send(400,"text/plain; charset=utf-8","切換 Wi-Fi 時請輸入新密碼。");return;}
  if(server.hasArg("rotation")&&server.arg("rotation")!="0"&&server.arg("rotation")!="180"){server.send(400,"text/plain; charset=utf-8","螢幕方向僅支援正常或旋轉 180 度。");return;}
  Config nextCfg=cfg;
  if(server.hasArg("rotation"))nextCfg.rotate180=server.arg("rotation")=="180";
  nextCfg.ssid=ssid;if(isOpen)nextCfg.password="";else if(!pw.isEmpty())nextCfg.password=pw;
  nextCfg.station=station;if(!client.isEmpty())nextCfg.client=client;if(!secret.isEmpty())nextCfg.secret=secret;
  nextCfg.ns=server.arg("labels")!="cw";nextCfg.flip=server.arg("flip")=="1";nextCfg.automatic=server.hasArg("auto");nextCfg.interval=constrain((uint32_t)server.arg("interval").toInt(),120U,3600U);
  DynamicJsonDocument d(2048);d["ssid"]=nextCfg.ssid;d["password"]=nextCfg.password;d["station"]=nextCfg.station;d["client"]=nextCfg.client;d["secret"]=nextCfg.secret;d["ns"]=nextCfg.ns;d["flip"]=nextCfg.flip;d["auto"]=nextCfg.automatic;d["interval"]=nextCfg.interval;d["rotation"]=nextCfg.rotate180?180:0;String value;serializeJson(d,value);
  if(!prefs.putString("config",value)){server.send(500,"text/plain","Storage failed");return;}
  server.send(200,"text/html; charset=utf-8","<meta name='viewport' content='width=device-width'><h2>已儲存，正在重新連線</h2><p>請查看看板。若 30 秒內無法連線，設定熱點會重新開啟。</p>");rebootAt=millis()+1500;
 });
 server.onNotFound([]{if(portalActive){server.sendHeader("Location","http://192.168.4.1/");server.send(302,"text/plain","");}else server.send(404,"text/plain","Not found");});server.begin();
}
int request(const String&url,Document&d,const String*form=nullptr){
 WiFiClientSecure tls;tls.setCACert(TDX_CA);tls.setTimeout(15);HTTPClient http;http.setConnectTimeout(10000);http.setTimeout(15000);http.useHTTP10(true);
 if(!http.begin(tls,url))return -1;
 http.addHeader("Accept","application/json");if(!form)http.addHeader("Authorization","Bearer "+token);else http.addHeader("Content-Type","application/x-www-form-urlencoded");
 int code=form?http.POST(*form):http.GET();
 if(code==200){DeserializationError e=deserializeJson(d,http.getStream());if(e)code=-2;}
 http.end();if(code==401)tokenExpiry=0;return code;
}
bool authenticate(){
 if(token.length()&&time(nullptr)<tokenExpiry)return true;
 Document d(8192);String form="grant_type=client_credentials&client_id="+encoded(cfg.client)+"&client_secret="+encoded(cfg.secret);
 int code=request("https://tdx.transportdata.tw/auth/realms/TDXConnect/protocol/openid-connect/token",d,&form);
 if(code!=200||d["access_token"].isNull()){setMessage("TDX 認證失敗 "+String(code));return false;}
 token=d["access_token"].as<String>();tokenExpiry=time(nullptr)+std::max(30,(d["expires_in"] | 300)-60);return true;
}
void networkTask(void*){
 uint32_t nextFetch=0,nextSchedule=0;
 for(;;){
  if(rebootAt){vTaskDelay(pdMS_TO_TICKS(100));continue;}
  if(WiFi.status()!=WL_CONNECTED){vTaskDelay(pdMS_TO_TICKS(500));continue;}
  if(time(nullptr)<1735689600){setMessage("正在校時");vTaskDelay(pdMS_TO_TICKS(3000));continue;}
  String today=dateNow();
  if((int32_t)(millis()-nextFetch)<0){vTaskDelay(pdMS_TO_TICKS(250));continue;}
  nextFetch=millis()+cfg.interval*1000;
  if(cfg.client.isEmpty()||cfg.secret.isEmpty()){setMessage("請設定 TDX 金鑰");vTaskDelay(pdMS_TO_TICKS(1000));continue;}
  if(!authenticate()){nextFetch=millis()+300000;continue;}
  if(today!=scheduleDate||(int32_t)(millis()-nextSchedule)>=0){
   Document d(524288);int code=request("https://tdx.transportdata.tw/api/basic/v3/Rail/TRA/DailyStationTimetable/Today/Station/"+cfg.station+"?$format=JSON",d);
   std::vector<rail::Train> updated;
   if(code==200&&rail::parseSchedule(d.as<JsonVariantConst>(),cfg.station.c_str(),today.c_str(),updated)){
    xSemaphoreTake(modelMutex,portMAX_DELAY);trains.swap(updated);scheduleDate=today;xSemaphoreGive(modelMutex);
    nextSchedule=millis()+14400000;
   }else {setMessage("時刻表更新失敗 "+String(code));nextSchedule=millis()+300000;if(code==429){nextFetch=millis()+900000;continue;}}
  }
  Document d(131072);int code=request("https://tdx.transportdata.tw/api/basic/v3/Rail/TRA/StationLiveBoard/Station/"+cfg.station+"?$format=JSON",d);
  if(code==200){xSemaphoreTake(modelMutex,portMAX_DELAY);bool ok=rail::mergeLive(d.as<JsonVariantConst>(),cfg.station.c_str(),today.c_str(),time(nullptr),trains);lastLive=rail::isoTime(d["SrcUpdateTime"] | "");apiMessage=ok?"TDX 資料有延遲，請以站內看板為準":"即時資料格式錯誤";xSemaphoreGive(modelMutex);}
  else{setMessage("即時更新失敗 "+String(code));nextFetch=millis()+(code==429?900000:300000);}
  vTaskDelay(pdMS_TO_TICKS(100));
 }
}
void IRAM_ATTR onTouch(){touchIRQ=true;}
void setupTouch(){
 pinMode(21,OUTPUT);digitalWrite(21,LOW);delay(30);digitalWrite(21,HIGH);delay(150);Wire.begin(18,17);Wire.setTimeOut(30);
 for(uint8_t a:{0x1a,0x15}){Wire.beginTransmission(a);if(Wire.endTransmission()==0){touchAddress=a;break;}}
 if(touchAddress==0x1a){Wire.beginTransmission(0x1a);Wire.write(0xd1);Wire.write(0x09);Wire.endTransmission();}
 if(touchAddress==0x15){Wire.beginTransmission(0x15);Wire.write(0xfe);Wire.write(1);Wire.endTransmission();}
 pinMode(16,INPUT_PULLUP);attachInterrupt(16,onTouch,FALLING);Serial.printf("Touch address: 0x%02x\n",touchAddress);
}
bool touchPoint(int&x,int&y){
 if(!touchAddress||!touchIRQ)return false;touchIRQ=false;uint8_t b[7]={};Wire.beginTransmission(touchAddress);
 if(touchAddress==0x1a){Wire.write(0xd0);Wire.write(0);}else Wire.write(0);
 if(Wire.endTransmission(false)!=0)return false;
 if(Wire.requestFrom((int)touchAddress,7)!=7)return false;for(int i=0;i<7;i++)b[i]=Wire.read();
 int px,py;if(touchAddress==0x1a){Wire.beginTransmission(touchAddress);Wire.write(0xd0);Wire.write(0);Wire.write(0xab);Wire.endTransmission();if(!(b[5]&15)||(b[0]&15)!=6)return false;px=(b[1]<<4)|(b[3]>>4);py=(b[2]<<4)|(b[3]&15);}
 else{if(!(b[2]&15)||(b[3]>>6)==1)return false;px=((b[3]&15)<<8)|b[4];py=((b[5]&15)<<8)|b[6];}
 if(!display::mapTouch(px,py,cfg.rotate180,x,y))return false;
 Serial.printf("Touch x=%d y=%d\n",x,y);return true;
}
void draw(){
 frame.fillSprite(BG);time_t now=time(nullptr);bool clock=now>1735689600;
 if(portalActive){
  label("臺鐵看板設定",8,5,AMBER);label("手機連接 Wi-Fi",8,30);label(apName,8,51,WHITE);
  label("密碼："+apPassword,8,74,AMBER);label("瀏覽器開啟",8,99);label("192.168.4.1",8,122,WHITE);
  label(WiFi.status()==WL_CONNECTED?"Wi-Fi 已連線，儲存後返回看板":"完成配網、選站後開始顯示",8,149,GRAY);frame.pushSprite(0,0);return;
 }
 const Station*s=stationInfo();
 int north=(cfg.station.toInt()>=6000?1:0)^(cfg.flip?1:0);
 const char*left=cfg.ns?"北上":"順行";const char*right=cfg.ns?"南下":"逆行";int leftDir=cfg.ns?north:0;
 frame.fillRect(0,0,320,26,WHITE);label("時間",8,4,BG);label("開往",65,4,BG);label("車種",130,4,BG);label("車次",195,4,BG);label("備註",264,4,BG);
 std::vector<rail::Train> rows;String msg;time_t updated;
 xSemaphoreTake(modelMutex,portMAX_DELAY);if(clock)rows=rail::upcoming(trains,direction,now);msg=apiMessage;updated=lastLive;xSemaphoreGive(modelMutex);
 int pages=std::max(1,((int)rows.size()+2)/3);if(page>=pages)page=0;
 if(rows.empty()){label(clock?"暫無班次／等待資料":"等待 Wi-Fi 連線與校時",40,56,AMBER);label(msg,8,86,GRAY,304);}
 else for(int i=0;i<3;i++){
  int n=page*3+i,y=29+i*29;if(n>=(int)rows.size())break;auto&t=rows[n];
  frame.drawFastHLine(0,y+26,320,0x2945);struct tm local;localtime_r(&t.departure,&local);char tm[8];strftime(tm,sizeof(tm),"%H:%M",&local);
  label(tm,4,y+4,WHITE,54);label(t.destination.c_str(),60,y+4,WHITE,64);
  uint16_t color=(t.code=="6"||t.code=="10")?0x1278:(t.code=="4"?0xc9e0:0xc986);if(t.type.find("3000")!=std::string::npos)color=0xc8c3;
  frame.fillRect(126,y+1,62,22,color);label(rail::shortType(t),129,y+4,WHITE,58);label(t.no.c_str(),192,y+4,WHITE,44);
  auto r=rail::remark(t,now,WiFi.status()==WL_CONNECTED);label(r.c_str(),240,y+4,r=="準點"?0xd760:AMBER,80);
 }
 frame.fillRect(0,138,320,32,0x10e3);char b[12]="--:--:--";if(clock){struct tm local;localtime_r(&now,&local);strftime(b,sizeof(b),"%H:%M:%S",&local);}label(b,4,147,AMBER);
 label(String(s->name)+"站",78,147,WHITE,84);
 frame.fillRoundRect(170,138,70,32,3,direction==leftDir?AMBER:0x2124);frame.fillRoundRect(244,138,70,32,3,direction!=leftDir?AMBER:0x2124);
 label(left,189,147,direction==leftDir?BG:WHITE);label(right,263,147,direction!=leftDir?BG:WHITE);
 String info;if(WiFi.status()!=WL_CONNECTED)info="離線 · 待更新";else if(!cfg.client.length())info="請長按按鈕設定金鑰";else if(!rail::fresh(updated,now))info="資料待更新";else info="更新 "+String((int)(now-updated))+"秒前";
 label(info,4,119,GRAY,264);label(String(page+1)+"/"+String(pages),276,119,GRAY,44);frame.pushSprite(0,0);
}
void setup(){
 pinMode(15,OUTPUT);digitalWrite(15,HIGH);pinMode(0,INPUT_PULLUP);pinMode(14,INPUT_PULLUP);
 Serial.begin(115200);delay(200);Serial.println("TRA Board 0.1.2 / LILYGO T-Display-S3 Touch");
 modelMutex=xSemaphoreCreateMutex();loadConfig();direction=cfg.ns?((cfg.station.toInt()>=6000?1:0)^(cfg.flip?1:0)):0;
 tft.init();tft.setRotation(display::landscapeRotation(cfg.rotate180));tft.fillScreen(BG);pinMode(38,OUTPUT);digitalWrite(38,HIGH);frame.setColorDepth(16);frame.setAttribute(PSRAM_ENABLE,true);
 if(!frame.createSprite(320,170)){Serial.println("Framebuffer allocation failed");while(true)delay(1000);}
 setupTouch(); // This release caches timetables in RAM and preserves the original filesystem.
 uint64_t mac=ESP.getEfuseMac();char suffix[7];snprintf(suffix,sizeof(suffix),"%06X",(unsigned)(mac&0xffffff));apName="TRA-"+String(suffix);
 char secret[9];snprintf(secret,sizeof(secret),"%08X",(unsigned)esp_random());apPassword=secret;csrf=String(esp_random(),HEX)+String(esp_random(),HEX)+String(esp_random(),HEX);
 WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);if(cfg.ssid.length())WiFi.begin(cfg.ssid.c_str(),cfg.password.c_str());else beginPortal();
 configTzTime("CST-8","time.cloudflare.com","time.google.com","pool.ntp.org");setupWeb();xTaskCreatePinnedToCore(networkTask,"tdx",16384,nullptr,1,nullptr,0);lastAction=millis();draw();
}
void loop(){
 if(Serial.available()) {
  char cmd=Serial.read();
  if(cmd=='?')Serial.printf("INFO station=%s wifi=%d touch=0x%02x psram=%u heap=%u portal=%d rotation=%d version=0.1.2\n",cfg.station.c_str(),(int)WiFi.status(),touchAddress,ESP.getPsramSize(),ESP.getFreeHeap(),portalActive,cfg.rotate180?180:0);
  if(cmd=='b'){direction=1-direction;page=0;draw();Serial.printf("DIRECTION %d\n",direction);}
  if(cmd=='p'){beginPortal();draw();}
  if(cmd=='s'){
   draw();Serial.println("FRAME_RGB888 320 170");
   uint8_t row[320*3];for(int y=0;y<170;y++){for(int x=0;x<320;x++){uint16_t c=frame.readPixel(x,y);row[x*3]=((c>>11)&31)*255/31;row[x*3+1]=((c>>5)&63)*255/63;row[x*3+2]=(c&31)*255/31;}Serial.write(row,sizeof(row));delay(1);}Serial.println("FRAME_END");
  }
 }
 static uint32_t lastDraw=0,lastTouch=0,pressStart=0,wifiDown=millis(),autoAt=millis();static bool held=false,longDone=false,bootHeld=false;
 if(rebootAt&&(int32_t)(millis()-rebootAt)>=0)ESP.restart();
 if(portalActive){dns.processNextRequest();server.handleClient();if(millis()-apStarted>600000&&WiFi.status()==WL_CONNECTED){WiFi.softAPdisconnect(true);portalActive=false;}}
 if(WiFi.status()==WL_CONNECTED)wifiDown=millis();else if(millis()-wifiDown>30000)beginPortal();
 bool down=digitalRead(14)==LOW;
 if(down&&!held){held=true;pressStart=millis();longDone=false;}
 if(held&&down&&!longDone&&millis()-pressStart>=3000){beginPortal();longDone=true;lastAction=millis();}
 if(!down&&held){if(!longDone&&millis()-pressStart>30){page++;lastAction=millis();}held=false;}
 bool boot=digitalRead(0)==LOW;if(boot&&!bootHeld){direction=1-direction;page=0;lastAction=millis();autoAt=millis();}bootHeld=boot;
 int x,y;if(touchPoint(x,y)&&millis()-lastTouch>250){lastTouch=millis();lastAction=millis();autoAt=millis();if(!portalActive){if(y>=138&&x>=74&&x<166)beginPortal();else if(y>=138&&x>=170){int north=(cfg.station.toInt()>=6000?1:0)^(cfg.flip?1:0);int left=cfg.ns?north:0;direction=x<242?left:1-left;page=0;}else if((y>=27&&y<116)||(y>=116&&y<138&&x>=270))page++;}}
 if(!portalActive&&cfg.automatic&&millis()-autoAt>15000&&millis()-lastAction>15000){direction=1-direction;page=0;autoAt=millis();}
 if(millis()-lastDraw>1000){draw();lastDraw=millis();}
 delay(5);
}
