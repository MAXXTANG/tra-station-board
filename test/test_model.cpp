#include <cassert>
#include <iostream>
#include "rail_model.h"
int main(){
 auto now=rail::isoTime("2026-11-10T12:00:00+08:00");assert(now==rail::isoTime("2026-11-10T04:00:00Z"));assert(rail::isoTime("")==0);
 DynamicJsonDocument d(10000);
 auto err=deserializeJson(d,R"({"TrainDate":"2026-11-10","StationTimetables":[{"StationID":"7210","Direction":1,"TimeTables":[{"TrainNo":"4171","DestinationStationName":{"Zh_tw":"樹林"},"TrainTypeCode":"6","DepartureTime":"11:55:00","SuspendedFlag":0},{"TrainNo":"217","DestinationStationName":{"Zh_tw":"臺北"},"TrainTypeCode":"3","DepartureTime":"12:20:00","SuspendedFlag":1}]},{"StationID":"7210","Direction":0,"TimeTables":[{"TrainNo":"4180","DestinationStationName":{"Zh_tw":"蘇澳"},"DepartureTime":"12:10:00"}]}]})");assert(!err);
 std::vector<rail::Train> t;assert(rail::parseSchedule(d.as<JsonVariantConst>(),"7210","2026-11-10",t));assert(t.size()==3);assert(!rail::parseSchedule(d.as<JsonVariantConst>(),"1000","2026-11-10",t));assert(t.size()==3);assert(rail::upcoming(t,1,now).size()==1);
 assert(rail::remark(t[0],now,true)=="待更新");assert(rail::remark(t[1],now,true)=="停駛");
 DynamicJsonDocument live(4000);deserializeJson(live,R"({"StationLiveBoards":[{"StationID":"7210","Direction":1,"TrainNo":"4171","DelayTime":10,"RunningStatus":1,"UpdateTime":"2026-11-10T11:59:00+08:00"}]})");
 assert(rail::mergeLive(live.as<JsonVariantConst>(),"7210","2026-11-10",now,t));assert(rail::upcoming(t,1,now).size()==2);assert(rail::remark(t[0],now,true)=="晚10分");assert(rail::remark(t[0],now,false)=="待更新");assert(rail::remark(t[0],now+400,true)=="待更新");
 live["StationLiveBoards"][0]["DelayTime"]=0;live["StationLiveBoards"][0]["RunningStatus"]=0;assert(rail::mergeLive(live.as<JsonVariantConst>(),"7210","2026-11-10",now,t));assert(rail::remark(t[0],now,true)=="準點");
 live["StationLiveBoards"][0]["RunningStatus"]=2;rail::mergeLive(live.as<JsonVariantConst>(),"7210","2026-11-10",now,t);assert(rail::remark(t[0],now,true)=="取消");
 live["StationLiveBoards"][0]["StationID"]="1000";rail::mergeLive(live.as<JsonVariantConst>(),"7210","2026-11-10",now,t);assert(rail::remark(t[0],now,true)=="待更新");
 assert(!rail::parseSchedule(d.as<JsonVariantConst>(),"7210","2026-11-11",t));assert(t.size()==3);
 deserializeJson(live,R"({"StationLiveBoards":[{"StationID":"7210","Direction":1,"TrainNo":"99","ScheduleDepartureTime":"23:55:00","DelayTime":20,"RunningStatus":1,"UpdateTime":"2026-11-11T00:04:00+08:00"}]})");auto midnight=rail::isoTime("2026-11-11T00:05:00+08:00");std::vector<rail::Train> night;rail::mergeLive(live.as<JsonVariantConst>(),"7210","2026-11-11",midnight,night);assert(night.size()==1);assert(rail::upcoming(night,1,midnight).size()==1);assert(night[0].departure==rail::isoTime("2026-11-10T23:55:00+08:00"));
 rail::Train later;later.no="99";later.direction=1;later.departure=rail::isoTime("2026-11-11T23:55:00+08:00");night={later};rail::mergeLive(live.as<JsonVariantConst>(),"7210","2026-11-11",midnight,night);assert(night.size()==2);assert(night[0].liveAt==0);assert(night[1].liveAt>0);
 std::cout<<"PASS: timezone, directions, stale/missing/live delays, cancellation, wrong station/date, midnight delayed train\n";
}
