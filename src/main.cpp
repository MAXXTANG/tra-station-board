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
#include "ods_ca.h"
#include "portal.h"
#include "display_orientation.h"
#include "board_profile.h"
#include "frame_buffer.h"
#include "ods_stream.h"
#include "touch_calibration.h"
#include "cyd_touch.h"
#include "cache_store.h"

struct Config {String ssid,password,station="7210"; bool flip=false,ns=true,automatic=false,rotate180=false; String recent[6]; display::Calibration calibration;} cfg;
TFT_eSPI tft; BoardFrame frame(&tft);
#ifdef TRA_BOARD_CYD
CydTouch cydTouch;
#endif
Preferences prefs;WebServer server(80);DNSServer dns;
SemaphoreHandle_t modelMutex;
std::vector<rail::Train> trains;
String apiMessage="等待連線",scheduleDate,catalogRevision;
constexpr uint32_t REFRESH_SECONDS=120;
constexpr uint32_t REFRESH_MS=REFRESH_SECONDS*1000;
time_t lastChecked=0;CacheStore cache;bool cachedFromDisk=false;
struct DrawState {time_t now=0,updated=0;bool clock=false;String message,date;std::vector<rail::Train> rows;} drawing;
bool portalActive=false;
uint8_t touchAddress=0;volatile bool touchIRQ=false;
String apName,apPassword,csrf;
uint32_t apStarted=0,rebootAt=0,lastAction=0;
int direction=1,page=0;
enum class View {Board,Recent,Regions,Stations,Switching,Calibrate};
View view=View::Board;int pickerPage=0,lastRegionPage=0;String pickerRegion,switchingStation;
constexpr uint16_t BG=0x0841,WHITE=0xf7be,AMBER=0xf5ad,GRAY=0x8410,RED=0xf986;
#if !defined(TRA_BOARD_CYD)
struct PsramAllocator {void* allocate(size_t n){return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}void deallocate(void*p){heap_caps_free(p);}void* reallocate(void*p,size_t n){return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}};
using Document=BasicJsonDocument<PsramAllocator>;
#endif
const Station* stationInfo(){for(const auto &s:STATIONS)if(cfg.station==s.id)return &s;return &STATIONS[0];}
String dateNow(){time_t now=time(nullptr);struct tm t;localtime_r(&now,&t);char b[12];strftime(b,sizeof(b),"%Y-%m-%d",&t);return b;}
void setMessage(const String&s){xSemaphoreTake(modelMutex,portMAX_DELAY);apiMessage=s;xSemaphoreGive(modelMutex);Serial.println(s);}
String htmlEscape(String s){s.replace("&","&amp;");s.replace("<","&lt;");s.replace(">","&gt;");s.replace("\"","&quot;");s.replace("'","&#39;");return s;}
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
const Station *findStation(const String &id){for(const auto &s:STATIONS)if(id==s.id)return &s;return nullptr;}
void normalizeRecent(Config &value){
 String ordered[6];int count=0;
 auto add=[&](const String &id){if(count>=6 || !findStation(id))return;for(int i=0;i<count;i++)if(ordered[i]==id)return;ordered[count++]=id;};
 add(value.station);for(const auto &id:value.recent)add(id);
 for(const char *name:{"礁溪","宜蘭","羅東","臺北","樹林","花蓮"})for(const auto &s:STATIONS)if(strcmp(name,s.name)==0)add(s.id);
 for(int i=0;i<6;i++)value.recent[i]=ordered[i];
}
bool saveConfig(const Config &value){
 DynamicJsonDocument d(1024);d["ssid"]=value.ssid;d["password"]=value.password;d["station"]=value.station;
 d["ns"]=value.ns;d["flip"]=value.flip;d["auto"]=value.automatic;d["rotation"]=value.rotate180?180:0;
 JsonArray recent=d.createNestedArray("recent");for(const auto &id:value.recent)if(id.length())recent.add(id);
 if(value.calibration.valid){JsonArray cal=d.createNestedArray("calibration");for(float v:{value.calibration.a,value.calibration.b,value.calibration.c,value.calibration.d,value.calibration.e,value.calibration.f})cal.add(v);}
 d["interval"]=REFRESH_SECONDS;d["source"]="ods";String blob;serializeJson(d,blob);return prefs.putString("config",blob)>0;
}
void loadConfig(){
 prefs.begin("tra-board",false);DynamicJsonDocument d(2048);deserializeJson(d,prefs.getString("config","{}"));
 cfg.ssid=d["ssid"] | "";cfg.password=d["password"] | "";cfg.station=d["station"] | "7210";
 cfg.flip=d["flip"] | false;cfg.ns=d["ns"] | true;cfg.automatic=d["auto"] | false;
 cfg.rotate180=(d["rotation"] | 0)==180;
 bool valid=false;for(auto&s:STATIONS)if(cfg.station==s.id)valid=true;if(!valid)cfg.station="7210";
 int recentIndex=0;for(JsonVariant id:d["recent"].as<JsonArray>())if(recentIndex<6)cfg.recent[recentIndex++]=id.as<String>();
 normalizeRecent(cfg);
 JsonArray cal=d["calibration"].as<JsonArray>();if(cal.size()==6){float *v[]={&cfg.calibration.a,&cfg.calibration.b,&cfg.calibration.c,&cfg.calibration.d,&cfg.calibration.e,&cfg.calibration.f};cfg.calibration.valid=true;for(int i=0;i<6;i++){*v[i]=cal[i].as<float>();if(!std::isfinite(*v[i]))cfg.calibration.valid=false;}}
 // Migrate old paid-service settings without changing Wi-Fi, station or orientation.
 if(d.containsKey("client") || d.containsKey("secret") || (d["interval"] | 0)!=REFRESH_SECONDS)saveConfig(cfg);
}
void setupWeb(){
 server.on("/",HTTP_GET,[]{
  if(!portalActive){server.send(403,"text/plain","Open setup using the device button.");return;}
  server.sendHeader("Cache-Control","no-store");server.setContentLength(CONTENT_LENGTH_UNKNOWN);server.send(200,"text/html; charset=utf-8",PORTAL_HEAD);
  server.sendContent("<input type='hidden' name='csrf' value='"+csrf+"'><label>Wi-Fi 名稱</label><input name='ssid' required maxlength='32' value='"+htmlEscape(cfg.ssid)+"' autocomplete='off'>");
  server.sendContent(PORTAL_MIDDLE);
  String chunk;chunk.reserve(1600);for(auto&s:STATIONS){chunk+="<option value='"+String(s.id)+"' data-region='"+htmlEscape(s.region)+"'"+(cfg.station==s.id?" selected":"")+">"+htmlEscape(s.name)+" · "+s.id+"</option>";if(chunk.length()>1400){server.sendContent(chunk);chunk="";}}server.sendContent(chunk);
  server.sendContent("</select><script type='application/json' id='current'>{\"labels\":\""+String(cfg.ns?"ns":"cw")+"\",\"flip\":\""+String(cfg.flip?"1":"0")+"\",\"rotation\":\""+String(cfg.rotate180?180:0)+"\",\"auto\":"+String(cfg.automatic?"true":"false")+"}</script>");
  if(board::CYD)server.sendContent("<label><input name=\"calibrate\" type=\"checkbox\" style=\"width:auto\"> 重新校正觸控（儲存後依序點擊四個十字）</label>");
  server.sendContent(PORTAL_TAIL);server.sendContent("");
 });
 server.on("/save",HTTP_POST,[]{
  if(!portalActive||server.arg("csrf")!=csrf){server.send(403,"text/plain","Invalid setup session");return;}
  String ssid=server.arg("ssid"),pw=server.arg("password"),station=server.arg("station");
  bool valid=false;for(auto&s:STATIONS)if(station==s.id)valid=true;
  bool isOpen=server.hasArg("open");
  if(ssid.length()==0||ssid.length()>32||pw.length()>63||(!pw.isEmpty()&&pw.length()<8)||!valid){server.send(400,"text/plain; charset=utf-8","請檢查 Wi-Fi 名稱、密碼（8–63 字元）與車站。");return;}
  if(!isOpen&&pw.isEmpty()&&cfg.password.isEmpty()){server.send(400,"text/plain; charset=utf-8","請輸入 Wi-Fi 密碼，或勾選無密碼 Wi-Fi。");return;}
  if(!isOpen&&pw.isEmpty()&&ssid!=cfg.ssid){server.send(400,"text/plain; charset=utf-8","切換 Wi-Fi 時請輸入新密碼。");return;}
  if(server.hasArg("rotation")&&server.arg("rotation")!="0"&&server.arg("rotation")!="180"){server.send(400,"text/plain; charset=utf-8","螢幕方向僅支援正常或旋轉 180 度。");return;}
  Config nextCfg=cfg;
  if(server.hasArg("rotation"))nextCfg.rotate180=server.arg("rotation")=="180";
  nextCfg.ssid=ssid;if(isOpen)nextCfg.password="";else if(!pw.isEmpty())nextCfg.password=pw;
  nextCfg.station=station;normalizeRecent(nextCfg);if(server.hasArg("calibrate"))nextCfg.calibration.valid=false;
  nextCfg.ns=server.arg("labels")!="cw";nextCfg.flip=server.arg("flip")=="1";nextCfg.automatic=server.hasArg("auto");
  if(!saveConfig(nextCfg)){server.send(500,"text/plain","Storage failed");return;}
  server.send(200,"text/html; charset=utf-8","<meta name='viewport' content='width=device-width'><h2>已儲存，正在重新連線</h2><p>請查看看板。若 30 秒內無法連線，設定熱點會重新開啟。</p>");rebootAt=millis()+1500;
 });
 server.onNotFound([]{if(portalActive){server.sendHeader("Location","http://192.168.4.1/");server.send(302,"text/plain","");}else server.send(404,"text/plain","Not found");});server.begin();
}
const char *stationName(const char *id){for(const auto &s:STATIONS)if(strcmp(id,s.id)==0)return s.name;return "";}
bool beginPublic(HTTPClient &http,WiFiClientSecure &tls,const String &path){
 tls.setCACert(ODS_CA);tls.setTimeout(20);http.setConnectTimeout(12000);http.setTimeout(20000);http.useHTTP10(true);
 if(!http.begin(tls,"https://ods.railway.gov.tw"+path))return false;
 http.addHeader("Accept-Encoding","identity");http.setUserAgent("TRAStationBoard/0.2.1");return true;
}
class HttpBodyReader {
 WiFiClient &client;int remaining;size_t total=0;uint8_t buffer[2048];size_t pos=0,count=0;
public:
 HttpBodyReader(WiFiClient &c,int size):client(c),remaining(size){}
 int read(){
  if(pos==count){
   if(remaining==0 || total>=8*1024*1024)return -1;
   uint32_t started=millis();int available=client.available();
   while(available<=0){
    if(!client.connected() || millis()-started>=20000)return -1;
    vTaskDelay(1);available=client.available();
   }
   size_t wanted=std::min((size_t)available,sizeof(buffer));
   if(remaining>0)wanted=std::min(wanted,(size_t)remaining);
   int received=client.read(buffer,wanted);if(received<=0)return -1;
   count=(size_t)received;pos=0;
   if(remaining>0)remaining-=count;total+=count;
  }
  return buffer[pos++];
 }
};
bool fetchCatalog(rail::DailyFile &file,const String &today){
 WiFiClientSecure tls;HTTPClient http;
 if(!beginPublic(http,tls,"/tra-ods-web/ods/download/dataResource/railway_schedule/JSON/list"))return false;
 int code=http.GET();if(code!=200 || http.getSize()>256*1024){http.end();setMessage("時刻表目錄失敗 "+String(code));return false;}
 std::string html;html.reserve(20000);HttpBodyReader body(http.getStream(),http.getSize());int c;
 while((c=body.read())>=0 && html.size()<256*1024)html+=(char)c;
 http.end();return rail::findDailyFile(html,today.c_str(),file);
}
bool fetchTimetable(const rail::DailyFile &file,const String &today,std::vector<rail::Train> &updated){
 WiFiClientSecure tls;HTTPClient http;if(!beginPublic(http,tls,file.path.c_str()))return false;
 int code=http.GET();if(code!=200 || http.getSize()>8*1024*1024){http.end();setMessage("時刻表下載失敗 "+String(code));return false;}
#ifndef TRA_BOARD_CYD
 Document doc(98304);DynamicJsonDocument filter(1024);
 filter["Train"]=true;filter["LineDir"]=true;filter["CarClass"]=true;
 JsonObject stop=filter["TimeInfos"].to<JsonArray>().createNestedObject();stop["Station"]=true;stop["Order"]=true;stop["ARRTime"]=true;stop["DEPTime"]=true;
 #endif
 HttpBodyReader body(http.getStream(),http.getSize());std::string source;
#ifdef TRA_BOARD_CYD
 rail::ParseStats stats;bool ok=rail::parseOdsCompact(body,cfg.station.c_str(),today.c_str(),stationName,updated,source,&stats,[]{vTaskDelay(1);});
 Serial.printf("ODS compact trains=%u maxStops=%u maxValue=%u\n",(unsigned)stats.trains,(unsigned)stats.maxStops,(unsigned)stats.maxValueBytes);
#else
 bool ok=rail::parseOdsStream(body,doc,filter.as<JsonVariantConst>(),cfg.station.c_str(),today.c_str(),stationName,updated,source,[]{vTaskDelay(1);});
 #endif
 http.end();if(ok)Serial.printf("ODS downloaded date=%s rows=%u source=%s\n",today.c_str(),(unsigned)updated.size(),source.c_str());return ok;
}
void networkTask(void*){
 uint32_t nextFetch=0;
 for(;;){
  if(rebootAt || WiFi.status()!=WL_CONNECTED){vTaskDelay(pdMS_TO_TICKS(250));continue;}
  if(time(nullptr)<1735689600){setMessage("正在校時");vTaskDelay(pdMS_TO_TICKS(3000));continue;}
  if((int32_t)(millis()-nextFetch)<0){vTaskDelay(pdMS_TO_TICKS(250));continue;}
  nextFetch=millis()+REFRESH_MS;String today=dateNow();rail::DailyFile file;
  setMessage("檢查時刻表");
  if(!fetchCatalog(file,today)){setMessage("目錄更新失敗，2 分鐘後重試");continue;}
  if(today==scheduleDate && file.revision==catalogRevision.c_str()){
   xSemaphoreTake(modelMutex,portMAX_DELAY);lastChecked=time(nullptr);apiMessage="時刻表已更新";xSemaphoreGive(modelMutex);
   Serial.println("ODS checked: cached revision unchanged");continue;
  }
  setMessage("下載時刻表");std::vector<rail::Train> updated;
  if(!fetchTimetable(file,today,updated)){setMessage("時刻表更新失敗，保留快取");continue;}
  xSemaphoreTake(modelMutex,portMAX_DELAY);
  if(scheduleDate!=today){
   time_t now=time(nullptr),cutoff=rail::midnightTaipei(today.c_str())+3600;
   for(const auto &t:trains)if(t.departure>=now-45 && t.departure<cutoff)updated.push_back(t);
  }
  trains.swap(updated);scheduleDate=today;catalogRevision=file.revision.c_str();lastChecked=time(nullptr);apiMessage="時刻表已更新";
  cachedFromDisk=false;xSemaphoreGive(modelMutex);
  if(board::CYD){rail::CacheMeta meta;meta.station=cfg.station.c_str();meta.date=today.c_str();meta.checked=lastChecked;Serial.printf("Cache save: %s\n",cache.save(meta,trains)?"ok":"unavailable");}
 }
}
void IRAM_ATTR onTouch(){touchIRQ=true;}
void setupTouch(){
#ifdef TRA_BOARD_CYD
 cydTouch.begin();Serial.println("Touch: XPT2046, calibration required on first use");
#else
 pinMode(21,OUTPUT);digitalWrite(21,LOW);delay(30);digitalWrite(21,HIGH);delay(150);Wire.begin(18,17);Wire.setTimeOut(30);
 for(uint8_t a:{0x1a,0x15}){Wire.beginTransmission(a);if(Wire.endTransmission()==0){touchAddress=a;break;}}
 if(touchAddress==0x1a){Wire.beginTransmission(0x1a);Wire.write(0xd1);Wire.write(0x09);Wire.endTransmission();}
 if(touchAddress==0x15){Wire.beginTransmission(0x15);Wire.write(0xfe);Wire.write(1);Wire.endTransmission();}
 pinMode(16,INPUT_PULLUP);attachInterrupt(16,onTouch,FALLING);Serial.printf("Touch address: 0x%02x\n",touchAddress);
#endif
}
bool touchPoint(int&x,int&y){
#ifdef TRA_BOARD_CYD
 int rx,ry;if(!cydTouch.raw(rx,ry))return false;return cfg.calibration.map(rx,ry,cfg.rotate180,x,y);
#else
 if(!touchAddress||!touchIRQ)return false;touchIRQ=false;uint8_t b[7]={};Wire.beginTransmission(touchAddress);
 if(touchAddress==0x1a){Wire.write(0xd0);Wire.write(0);}else Wire.write(0);
 if(Wire.endTransmission(false)!=0)return false;
 if(Wire.requestFrom((int)touchAddress,7)!=7)return false;for(int i=0;i<7;i++)b[i]=Wire.read();
 int px,py;if(touchAddress==0x1a){Wire.beginTransmission(touchAddress);Wire.write(0xd0);Wire.write(0);Wire.write(0xab);Wire.endTransmission();if(!(b[5]&15)||(b[0]&15)!=6)return false;px=(b[1]<<4)|(b[3]>>4);py=(b[2]<<4)|(b[3]&15);}
 else{if(!(b[2]&15)||(b[3]>>6)==1)return false;px=((b[3]&15)<<8)|b[4];py=((b[5]&15)<<8)|b[6];}
 if(!display::mapTouch(px,py,cfg.rotate180,x,y))return false;
 Serial.printf("Touch x=%d y=%d\n",x,y);return true;
#endif
}

#ifdef TRA_BOARD_CYD
struct CalibrationSession {display::Point raw[3],target[3];display::Calibration fitted;int step=0;bool armed=false;uint32_t releasedAt=0;} calibrationSession;
const display::Point calibrationTargets[]={{20,60},{300,60},{20,220},{300,220}};
String calibrationNotice;
bool tickCalibration(){
 auto &c=calibrationSession;
 if(cydTouch.released()){if(!c.armed)c.releasedAt=millis();c.armed=true;return false;}
 if(!c.armed || millis()-c.releasedAt<200)return false;
 int rx,ry;if(!cydTouch.raw(rx,ry))return false;c.armed=false;
 display::Point target=calibrationTargets[c.step];
 if(cfg.rotate180){target.x=board::WIDTH-1-target.x;target.y=board::HEIGHT-1-target.y;}
 if(c.step<3){
  c.raw[c.step]={(float)rx,(float)ry};c.target[c.step]=target;c.step++;
  if(c.step==3 && !display::solveCalibration(c.raw,c.target,c.fitted)){c.step=0;calibrationNotice="校正失敗，請重新點擊";}
 }else{
  int x,y;bool valid=c.fitted.map(rx,ry,false,x,y) && std::abs(x-(int)target.x)<=18 && std::abs(y-(int)target.y)<=18;
  if(valid){Config nextCfg=cfg;nextCfg.calibration=c.fitted;if(saveConfig(nextCfg)){cfg=nextCfg;view=View::Board;Serial.println("Touch calibration saved");}else calibrationNotice="儲存失敗，請重新點擊";}
  else calibrationNotice="校正失敗，請重新點擊";
  c.step=0;
 }
 return true;
}
void drawCalibration(){
 auto &c=calibrationSession;auto target=calibrationTargets[c.step];
 label("觸控校正",96,5,AMBER);label("依序點擊十字 "+String(c.step+1)+"/4",56,29);
 label("請放開後再點下一點",64,104,GRAY);label(calibrationNotice,56,132,AMBER,240);
 frame.drawFastHLine(target.x-8,target.y,17,AMBER);for(int y=target.y-8;y<=target.y+8;y++)frame.drawPixel(target.x,y,AMBER);
}
#else
bool tickCalibration(){return false;}
void drawCalibration(){}
#endif

std::vector<String> pickerRegions(){
 std::vector<String> names;for(const auto &s:STATIONS){bool found=false;for(const auto &name:names)if(name==s.region)found=true;if(!found && strlen(s.region))names.push_back(s.region);}return names;
}
std::vector<const Station*> pickerStations(){
 std::vector<const Station*> result;
 if(view==View::Recent){for(const auto &id:cfg.recent){const Station *s=findStation(id);if(s)result.push_back(s);}}
 else for(const auto &s:STATIONS)if(pickerRegion==s.region)result.push_back(&s);
 return result;
}
void drawPicker(){
 if(view==View::Switching){label("正在切換車站",64,43,AMBER);label(switchingStation,104,72);label("自動連線並載入表定時刻",48,106,GRAY);frame.pushSprite(0,0);return;}
 bool regions=view==View::Regions;auto counties=pickerRegions();auto stations=pickerStations();
 int count=regions?(int)counties.size():(int)stations.size(),pages=std::max(1,(count+7)/8);if(pickerPage>=pages)pickerPage=0;
 label(view==View::Recent?"常用車站":(regions?"選擇縣市":pickerRegion),8,5,AMBER,192);
 if(view==View::Recent){frame.fillRoundRect(208,1,52,27,3,0x2124);label("全部",218,6);}
 frame.fillRoundRect(264,1,52,27,3,0x2124);label("返回",274,6);
 for(int i=0;i<8;i++){
  int n=pickerPage*8+i;if(n>=count)break;int x=6+(i%2)*158,y=32+(i/2)*board::PICKER_STEP;
  bool selected=!regions && cfg.station==stations[n]->id;
  frame.fillRoundRect(x,y,150,board::PICKER_STEP-3,3,selected?AMBER:0x2124);
  label(regions?counties[n]:String(stations[n]->name),x+7,y+3,selected?BG:WHITE,136);
 }
 frame.fillRect(0,board::PICKER_BOTTOM,320,30,0x10e3);label("上一頁",16,board::PICKER_TEXT_Y);label(String(pickerPage+1)+"/"+String(pages),144,board::PICKER_TEXT_Y,GRAY,40);label("下一頁",256,board::PICKER_TEXT_Y);frame.pushSprite(0,0);
}
void pickerTap(int x,int y){
 if(view==View::Switching)return;
 if(y<29){
  if(x>=264){if(view==View::Stations){view=View::Regions;pickerPage=lastRegionPage;}else if(view==View::Regions){view=View::Recent;pickerPage=0;}else view=View::Board;}
  else if(view==View::Recent && x>=208){view=View::Regions;pickerPage=0;}
  return;
 }
 bool regions=view==View::Regions;auto counties=pickerRegions();auto stations=pickerStations();
 int count=regions?(int)counties.size():(int)stations.size(),pages=std::max(1,(count+7)/8);
 if(y>=board::PICKER_BOTTOM){if(x<100)pickerPage=(pickerPage+pages-1)%pages;else if(x>=220)pickerPage=(pickerPage+1)%pages;return;}
 if(y<32 || y>=32+4*board::PICKER_STEP)return;int column=x<160?0:1,row=(y-32)/board::PICKER_STEP;
 if((y-32)%board::PICKER_STEP>=board::PICKER_STEP-3 || x<6 || (column==0 && x>=156) || (column==1 && (x<164 || x>=314)))return;
 int index=pickerPage*8+row*2+column;if(index>=count)return;
 if(regions){lastRegionPage=pickerPage;pickerRegion=counties[index];view=View::Stations;pickerPage=0;return;}
 const Station *selected=stations[index];if(cfg.station==selected->id){view=View::Board;return;}
 Config nextCfg=cfg;nextCfg.station=selected->id;normalizeRecent(nextCfg);
 if(!saveConfig(nextCfg)){setMessage("站點儲存失敗");view=View::Board;return;}
 switchingStation=selected->name;view=View::Switching;rebootAt=millis()+1200;
}
void drawPass(){
 frame.fillSprite(BG);time_t now=drawing.now;bool clock=drawing.clock;
 if(portalActive){
  label("臺鐵看板設定",8,5,AMBER);label("手機連接 Wi-Fi",8,30);label(apName,8,51,WHITE);
  label("密碼："+apPassword,8,74,AMBER);label("瀏覽器開啟",8,99);label("192.168.4.1",8,122,WHITE);
  label(WiFi.status()==WL_CONNECTED?"Wi-Fi 已連線，儲存後返回看板":"完成配網、選站後開始顯示",8,149,GRAY);frame.pushSprite(0,0);return;
 }
 if(view==View::Calibrate){drawCalibration();frame.pushSprite(0,0);return;}
 if(view!=View::Board){drawPicker();return;}
 const Station*s=stationInfo();
 int north=(cfg.station.toInt()>=6000?1:0)^(cfg.flip?1:0);
 const char*left=cfg.ns?"北上":"順行";const char*right=cfg.ns?"南下":"逆行";int leftDir=cfg.ns?north:0;
 frame.fillRect(0,0,320,26,WHITE);label("時間",16,4,BG);label("開往",98,4,BG);label("車種",190,4,BG);label("車次",268,4,BG);
 const auto &rows=drawing.rows;const String &msg=drawing.message;time_t updated=drawing.updated;
 int pages=std::max(1,((int)rows.size()+board::ROWS-1)/board::ROWS);if(page>=pages)page=0;
 if(rows.empty()){label(clock?(updated?"今日此方向已無班次":"等待時刻表"):"等待 Wi-Fi 連線與校時",40,56,AMBER);label(msg,8,86,GRAY,304);}
 else for(int i=0;i<board::ROWS;i++){
  int n=page*board::ROWS+i,y=board::TABLE_Y+i*board::ROW_HEIGHT;if(n>=(int)rows.size())break;auto&t=rows[n];
  frame.drawFastHLine(0,y+board::ROW_HEIGHT-3,320,0x2945);struct tm local;localtime_r(&t.departure,&local);char tm[8];strftime(tm,sizeof(tm),"%H:%M",&local);
  label(tm,8,y+4,WHITE,56);label(t.destination.c_str(),70,y+4,WHITE,90);
  uint16_t color=(t.code=="6"||t.code=="10")?0x1278:(t.code=="4"?0xc9e0:0xc986);if(t.type.find("3000")!=std::string::npos)color=0xc8c3;
  frame.fillRect(164,y+1,80,22,color);label(rail::shortType(t),174,y+4,WHITE,64);label(t.no.c_str(),260,y+4,WHITE,56);
 }
 frame.fillRect(0,board::NAV_Y,320,32,0x10e3);char b[12]="--:--:--";if(clock){struct tm local;localtime_r(&now,&local);strftime(b,sizeof(b),"%H:%M:%S",&local);}label(b,4,board::NAV_TEXT_Y,AMBER);
 label(String(s->name)+"站",78,board::NAV_TEXT_Y,WHITE,84);
 frame.fillRoundRect(170,board::NAV_Y,70,32,3,direction==leftDir?AMBER:0x2124);frame.fillRoundRect(244,board::NAV_Y,70,32,3,direction!=leftDir?AMBER:0x2124);
 label(left,189,board::NAV_TEXT_Y,direction==leftDir?BG:WHITE);label(right,263,board::NAV_TEXT_Y,direction!=leftDir?BG:WHITE);
 String info;
 if(!clock && board::CYD && drawing.date.length())info="未校時 · 快取 "+drawing.date;
 else if(WiFi.status()!=WL_CONNECTED)info="離線 · 快取時刻";
 else if(!updated)info=msg;
 else if(now-updated>2*REFRESH_SECONDS)info="更新失敗 · 保留表定時刻";
 else {struct tm checked;localtime_r(&updated,&checked);char stamp[8];strftime(stamp,sizeof(stamp),"%H:%M",&checked);info="更新時間 "+String(stamp);}
 label(info,4,board::STATUS_Y,GRAY,264);label(String(page+1)+"/"+String(pages),276,board::STATUS_Y,GRAY,44);frame.pushSprite(0,0);
}
void prepareDraw(){
 drawing.now=time(nullptr);drawing.clock=drawing.now>1735689600;
 xSemaphoreTake(modelMutex,portMAX_DELAY);
 if(view==View::Board && !portalActive){
  if(drawing.clock)drawing.rows=rail::upcoming(trains,direction,drawing.now);
  else{drawing.rows.clear();if(board::CYD && cachedFromDisk){for(const auto&t:trains)if(t.direction==direction)drawing.rows.push_back(t);std::sort(drawing.rows.begin(),drawing.rows.end(),[](const rail::Train&a,const rail::Train&b){return a.departure<b.departure;});}}
 }
 drawing.message=apiMessage;drawing.date=scheduleDate;drawing.updated=lastChecked;xSemaphoreGive(modelMutex);
}
void draw(){prepareDraw();for(int band=0;band<board::HEIGHT;band+=BoardFrame::BAND_HEIGHT){frame.band(band);drawPass();}}
void setup(){
 if(!board::CYD){pinMode(15,OUTPUT);digitalWrite(15,HIGH);pinMode(14,INPUT_PULLUP);}
 pinMode(0,INPUT_PULLUP);
 Serial.begin(115200);delay(200);Serial.printf("TRA Board 0.3.0-dev / %s\n",board::NAME);
 modelMutex=xSemaphoreCreateMutex();loadConfig();direction=cfg.ns?((cfg.station.toInt()>=6000?1:0)^(cfg.flip?1:0)):0;
 tft.init();tft.setRotation(display::landscapeRotation(cfg.rotate180));tft.fillScreen(BG);pinMode(board::BACKLIGHT,OUTPUT);digitalWrite(board::BACKLIGHT,HIGH);
 if(!frame.begin()){Serial.println("Framebuffer allocation failed");while(true)delay(1000);}
 cache.begin();rail::CacheMeta meta;if(cache.load(cfg.station,meta,trains)){scheduleDate=meta.date.c_str();lastChecked=meta.checked;cachedFromDisk=true;Serial.printf("Cache loaded rows=%u date=%s\n",(unsigned)trains.size(),meta.date.c_str());}
 setupTouch();
 if(board::CYD && !cfg.calibration.valid && cfg.ssid.length())view=View::Calibrate;
 uint64_t mac=ESP.getEfuseMac();char suffix[7];snprintf(suffix,sizeof(suffix),"%06X",(unsigned)(mac&0xffffff));apName="TRA-"+String(suffix);
 char secret[9];snprintf(secret,sizeof(secret),"%08X",(unsigned)esp_random());apPassword=secret;csrf=String(esp_random(),HEX)+String(esp_random(),HEX)+String(esp_random(),HEX);
 WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);if(cfg.ssid.length())WiFi.begin(cfg.ssid.c_str(),cfg.password.c_str());else beginPortal();
 configTzTime("CST-8","time.cloudflare.com","time.google.com","pool.ntp.org");setupWeb();xTaskCreatePinnedToCore(networkTask,"ods",16384,nullptr,1,nullptr,0);lastAction=millis();draw();
}
void loop(){
 if(Serial.available()) {
  char cmd=Serial.read();
  if(cmd=='?')Serial.printf("INFO station=%s wifi=%d touch=0x%02x psram=%u heap=%u portal=%d rotation=%d source=ods interval=%u version=0.3.0-dev\n",cfg.station.c_str(),(int)WiFi.status(),touchAddress,ESP.getPsramSize(),ESP.getFreeHeap(),portalActive,cfg.rotate180?180:0,(unsigned)REFRESH_SECONDS);
  if(cmd=='b'){direction=1-direction;page=0;draw();Serial.printf("DIRECTION %d\n",direction);}
  if(cmd=='p'){beginPortal();draw();}
  if(cmd=='s'){
   prepareDraw();Serial.printf("FRAME_RGB888 %d %d\n",board::WIDTH,board::HEIGHT);
   uint8_t row[board::WIDTH*3];
   for(int band=0;band<board::HEIGHT;band+=BoardFrame::BAND_HEIGHT){frame.band(band);drawPass();
    for(int y=band;y<std::min(band+BoardFrame::BAND_HEIGHT,board::HEIGHT);y++){
     for(int x=0;x<board::WIDTH;x++){uint16_t c=frame.readPixel(x,y);row[x*3]=((c>>11)&31)*255/31;row[x*3+1]=((c>>5)&63)*255/63;row[x*3+2]=(c&31)*255/31;}
     Serial.write(row,sizeof(row));delay(1);
    }
   }Serial.println("FRAME_END");
  }
 }
 static uint32_t lastDraw=0,lastTouch=0,pressStart=0,wifiDown=millis(),autoAt=millis();static bool held=false,longDone=false,bootHeld=false;
 if(rebootAt&&(int32_t)(millis()-rebootAt)>=0)ESP.restart();
 if(portalActive){dns.processNextRequest();server.handleClient();if(millis()-apStarted>600000&&WiFi.status()==WL_CONNECTED){WiFi.softAPdisconnect(true);portalActive=false;}}
 if(WiFi.status()==WL_CONNECTED)wifiDown=millis();else if(millis()-wifiDown>30000)beginPortal();
 bool down=digitalRead(board::BUTTON)==LOW;
 if(down&&!held){held=true;pressStart=millis();longDone=false;}
 if(held&&down&&!longDone&&millis()-pressStart>=3000){beginPortal();longDone=true;lastAction=millis();}
 if(!down&&held){if(!longDone&&millis()-pressStart>30){if(view==View::Board)page++;else pickerPage++;lastAction=millis();draw();}held=false;}
 bool boot=!board::CYD && digitalRead(0)==LOW;if(boot&&!bootHeld){if(view==View::Board){direction=1-direction;page=0;}else view=View::Board;lastAction=millis();autoAt=millis();draw();}bootHeld=boot;
 if(!portalActive && view==View::Calibrate && tickCalibration())draw();
 int x,y;if(view!=View::Calibrate && touchPoint(x,y)&&millis()-lastTouch>250){lastTouch=millis();lastAction=millis();autoAt=millis();if(!portalActive){if(view!=View::Board)pickerTap(x,y);else if(y>=board::NAV_Y&&x>=74&&x<166){view=View::Recent;pickerPage=0;}else if(y>=board::NAV_Y&&x>=170){int north=(cfg.station.toInt()>=6000?1:0)^(cfg.flip?1:0);int left=cfg.ns?north:0;direction=x<242?left:1-left;page=0;}else if((y>=27&&y<board::TABLE_BOTTOM)||(y>=board::TABLE_BOTTOM&&y<board::NAV_Y&&x>=270))page++;}draw();}
 if(!portalActive&&view==View::Board&&cfg.automatic&&millis()-autoAt>15000&&millis()-lastAction>15000){direction=1-direction;page=0;autoAt=millis();}
 if(millis()-lastDraw>1000){draw();lastDraw=millis();}
 delay(5);
}
