#pragma once
// Synthetic dialogue and audio names only. Completion events model Android playback.
namespace voice_continuation_cases {
static Program program(const std::string& suffix="@k@e",bool timed=false,bool three=false){
    Program p;historyVoice(p,"continuation_first");fontText(p,"FIRST"+suffix,1,0);
    if(timed){for(int n:{0,1,500})p.push(n);p.op(0x110);for(int n:{0,256,0,0,300,0})p.push(n);p.op(0x200);}
    historyVoice(p,"continuation_second");fontText(p,"SECOND",three?0:1,three?0:1);
    if(three){historyVoice(p,"continuation_third");fontText(p,"THIRD",1,1);}
    p.op(0xf4);return p;
}
static std::string step(Fixture& f,std::string s,uint64_t& time){
    if(waiting(s,"transitionReady"))return acknowledge(f,s);
    time+=50;return f.command("tick",std::to_string(time).c_str());
}
static std::string reachVoice(Fixture& f,std::string s,uint64_t& time){
    for(int i=0;i<120&&!waiting(s,"voice");++i)s=step(f,s,time);
    check(waiting(s,"voice"),"continued utterance reaches real playback completion wait");return s;
}
static std::string end(Fixture& f,std::string s,uint64_t& time){
    auto id=std::to_string(uint64_t(number(audioChannel(s,"voice"),"id")));
    s=f.command("audioEnded",("voice "+id).c_str());return step(f,s,time);
}
static void noInputAndCompletionOrder(){
    Fixture f(program("@k@e",true));uint64_t time=0;auto s=reachVoice(f,f.command("action","start"),time);
    auto id=number(audioChannel(s,"voice"),"id");check(audioChannel(s,"voice").find("continuation_first.hw")!=std::string::npos,"timed graphics cannot replace an unfinished voice");
    for(int i=0;i<80;++i)s=step(f,s,time);
    check(waiting(s,"voice")&&number(audioChannel(s,"voice"),"id")==id,"elapsed script time never substitutes for audio completion");
    check(number(s,"nextTickMs")==0,"idle voice completion wait does not busy-poll");
    s=f.command("audioEnded",("voice "+std::to_string(uint64_t(id+900))).c_str());s=step(f,s,time);
    check(waiting(s,"voice"),"stale completion cannot release a continued voice");
    s=f.command("audioEnded","se0 0");s=step(f,s,time);check(waiting(s,"voice"),"other channels cannot release speech wait");
    s=end(f,s,time);check(waiting(s,"input")&&audioChannel(s,"voice").find("continuation_second.hw")!=std::string::npos,"next clip starts after matching completion");
    auto second=audioChannel(s,"voice");s=step(f,s,time);check(audioChannel(s,"voice")==second,"completion starts the next clip exactly once");f.passed=true;
}
static void userAdvanceAndSkipRemainImmediate(){
    for(bool skip:{false,true}){
        Fixture f(program());uint64_t time=0;f.command("textSpeed","0");auto s=reachVoice(f,f.command("action","start"),time);
        s=f.command(skip?"skip":"advance",skip?"1":"");if(skip)s=step(f,s,time);
        check(audioChannel(s,"voice").find("continuation_second.hw")!=std::string::npos,"explicit advance or skip may interrupt continued speech");f.passed=true;
    }
    Fixture f(program(""));f.command("textSpeed","0");auto s=f.command("action","start");check(waiting(s,"input"),"ordinary dialogue keeps its input boundary");s=f.command("advance");
    check(waiting(s,"input")&&audioChannel(s,"voice").find("continuation_second.hw")!=std::string::npos,"ordinary next-line advance still replaces the previous voice");f.passed=true;
    Fixture noMarker(program("@e"));s=noMarker.command("action","start");check(waiting(s,"input")&&audioChannel(s,"voice").find("continuation_second.hw")!=std::string::npos,"unmarked no-wait scripts retain their original scheduling");noMarker.passed=true;
}
static void multipleClipsPauseAndHistory(){
    Fixture f(program("@k@e",false,true));uint64_t time=0;f.command("textSpeed","0");auto s=reachVoice(f,f.command("action","start"),time);
    s=f.command("pause");auto id=std::to_string(uint64_t(number(audioChannel(s,"voice"),"id")));s=f.command("audioEnded",("voice "+id).c_str());s=step(f,s,time);
    check(waiting(s,"voice")&&audioChannel(s,"voice").find("continuation_second.hw")==std::string::npos,"late completion while paused cannot launch a clip");
    s=f.command("resume");s=step(f,s,time);check(waiting(s,"voice")&&audioChannel(s,"voice").find("continuation_second.hw")!=std::string::npos,"resume starts the second clip and protects the third");
    s=f.command("history");s=end(f,s,time);check(s.find("\"scene\":\"history\"")!=std::string::npos&&audioChannel(s,"voice").find("continuation_third.hw")==std::string::npos,"history never launches a pending normal clip");
    s=f.command("history");s=step(f,s,time);check(waiting(s,"input")&&audioChannel(s,"voice").find("continuation_third.hw")!=std::string::npos,"history close releases only the completed predecessor");f.passed=true;
}
static void completedVoiceAndTitleCancel(){
    Fixture f(program("@k@e",true));uint64_t time=0;auto s=f.command("action","start");
    s=f.command("audioEnded",("voice "+std::to_string(uint64_t(number(audioChannel(s,"voice"),"id")))).c_str());
    for(int i=0;i<100&&!waiting(s,"input");++i)s=step(f,s,time);
    check(waiting(s,"input")&&audioChannel(s,"voice").find("continuation_second.hw")!=std::string::npos,"already completed speech adds no new wait");f.passed=true;
    Fixture cancel(program());time=0;s=reachVoice(cancel,cancel.command("action","start"),time);s=cancel.command("title");
    check(s.find("\"scene\":\"title\"")!=std::string::npos,"title cancels the suspended voice request");s=cancel.command("action","start");
    check(waiting(s,"voice")&&audioChannel(s,"voice").find("continuation_first.hw")!=std::string::npos,"new game never starts a stale deferred clip");cancel.passed=true;
}
static void manualRevealAndLoadReset(){
    Fixture f(program());uint64_t time=0;auto s=reachVoice(f,f.command("action","start"),time);
    check(boolean(s,"revealing")&&!boolean(s,"canSave"),"pending voice operation is not serialized as an incomplete save");
    f.command("save","2");check(!fs::exists(f.root/"savedata/slot2.whs"),"save during deferred voice leaves no partial slot");
    s=f.command("advance");check(waiting(s,"voice")&&!boolean(s,"revealing")&&audioChannel(s,"voice").find("continuation_first.hw")!=std::string::npos,"first tap finishes text without cutting its voice");
    s=f.command("advance");check(waiting(s,"input")&&audioChannel(s,"voice").find("continuation_second.hw")!=std::string::npos,"second tap explicitly interrupts the voice wait");
    f.command("textSpeed","0");f.command("save","2");s=f.command("action","start");check(waiting(s,"voice"),"restart reaches another deferred request");
    s=f.command("load","2");check(waiting(s,"input")&&audioChannel(s,"voice").find("continuation_second.hw")!=std::string::npos,"load discards pending operation before restoring saved voice");
    s=end(f,s,time);check(waiting(s,"input")&&audioChannel(s,"voice").find("continuation_first.hw")==std::string::npos,"loaded completion never resurrects a deferred clip");f.passed=true;
}
static void run(){noInputAndCompletionOrder();userAdvanceAndSkipRemainImmediate();multipleClipsPauseAndHistory();completedVoiceAndTitleCancel();manualRevealAndLoadReset();}
}
