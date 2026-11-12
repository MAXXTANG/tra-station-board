#pragma once
#include "rail_cache.h"
#ifdef TRA_BOARD_CYD
#include <LittleFS.h>
#endif

class CacheStore {
    bool ready=false;
public:
    void begin(){
#ifdef TRA_BOARD_CYD
        // Never format existing storage implicitly. A fresh install packages a
        // separate LittleFS image; an unavailable cache does not block Wi-Fi.
        ready=LittleFS.begin(false);
        Serial.printf("Timetable cache: %s\n",ready?"mounted":"not mounted (no formatting)");
#endif
    }
    bool load(const String &station,rail::CacheMeta &meta,std::vector<rail::Train> &trains){
#ifdef TRA_BOARD_CYD
        if(!ready)return false;File file=LittleFS.open("/timetable.jsonl","r");
        if(!file || file.size()>256*1024)return false;
        return rail::readCache(file,station.c_str(),meta,trains);
#else
        return false;
#endif
    }
    bool save(const rail::CacheMeta &meta,const std::vector<rail::Train> &trains){
#ifdef TRA_BOARD_CYD
        if(!ready)return false;File file=LittleFS.open("/timetable.tmp","w");if(!file)return false;
        bool ok=rail::writeCache(file,meta,trains);file.flush();file.close();
        // LittleFS rename is atomic; preserve the old file on any failure.
        if(ok)ok=LittleFS.rename("/timetable.tmp","/timetable.jsonl");
        if(!ok)LittleFS.remove("/timetable.tmp");return ok;
#else
        return false;
#endif
    }
};
