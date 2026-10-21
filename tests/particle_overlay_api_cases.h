#pragma once
// Synthetic API fixtures only; shared Program/Fixture helpers own all files.
namespace particle_overlay_api_cases {
static std::string object(const std::string& json,const char* key){
    auto at=json.find(std::string("\"")+key+"\":{");check(at!=std::string::npos,"particle family JSON");
    at=json.find('{',at);int depth=0;for(size_t end=at;end<json.size();++end){if(json[end]=='{')++depth;else if(json[end]=='}'&&!--depth)return json.substr(at,end-at+1);}
    throw std::runtime_error("Unterminated particle fixture JSON");
}
static std::string revision(const std::string& family){const auto at=family.find("\"revision\":\"");check(at!=std::string::npos,"particle revision string");auto begin=at+12;return family.substr(begin,family.find('"',begin)-begin);}
static void images(Fixture& f){f.imageHeader("fallnote0",13512,187);f.imageHeader("fallnote1",4500,112);f.imageHeader("fallnote2",3600,150);f.imageHeader("focusline",960,2);}
static std::string particleAck(Fixture& f,const std::string& state){
    const auto fall=object(state,"fallNote"),focus=object(state,"focusLine");
    const auto payload=std::to_string(uint64_t(number(state,"version")))+' '+(boolean(fall,"pendingResource")?revision(fall):"0")+' '+(boolean(focus,"pendingResource")?revision(focus):"0");
    return f.command("particlesReady",payload.c_str());
}
static std::string finishParticlePrepare(Fixture& f,std::string state){
    if(boolean(object(state,"fallNote"),"pendingResource")||boolean(object(state,"focusLine"),"pendingResource"))state=particleAck(f,state);
    if(waiting(state,"transitionReady")){auto at=state.find("\"transition\":{");check(at!=std::string::npos,"particle prepare transition");state=f.command("transitionReady",std::to_string(uint64_t(number(state.substr(at),"id"))).c_str());}
    return state;
}
static void readyTickStopAndPause(){
    Program p;p.op(0x390);for(int n:{13,-4,7,0,99})p.push(n);p.op(0x3a0);p.commit();p.dialogue(0);p.op(0x392);p.op(0x3a1);p.commit();p.dialogue(1);p.op(0xf4);
    Fixture f(p);images(f);f.command("textSpeed","0");auto s=f.command("action","start");
    check(waiting(s,"transitionReady")&&!boolean(s,"canSave")&&number(s,"nextTickMs")==0,"pending particles wait without polling");
    check(boolean(object(s,"fallNote"),"pendingResource")&&boolean(object(s,"focusLine"),"pendingResource"),"two Native prepared resource families");
    const auto original=s;auto ignored=f.command("particlesReady","-1 1 2");check(number(ignored,"version")==number(original,"version")&&object(ignored,"fallNote")==object(original,"fallNote"),"malformed ACK no version or packet mutation");
    s=particleAck(f,s);check(number(s,"version")==number(original,"version")&&waiting(s,"transitionReady"),"particle ACK does not invalidate following transition ACK");
    const auto acked=s;s=particleAck(f,s);check(number(s,"version")==number(acked,"version")&&object(s,"focusLine")==object(acked,"focusLine"),"duplicate zero/past ACK has no restart");
    s=finishParticlePrepare(f,s);s=f.command("tick","1000");
    check(waiting(s,"input")&&number(s,"nextTickMs")==16&&boolean(s,"canSave"),"stable active pools animate in otherwise idle input");
    check(object(s,"focusLine").find("\"priority\":22577")!=std::string::npos&&object(s,"fallNote").find("\"priority\":22576")!=std::string::npos,"natural tick creates sprites after ACK");
    auto before=object(s,"focusLine");s=f.command("pause");check(number(s,"nextTickMs")==0,"Core pause stops particle scheduling");s=f.command("tick","1000000");check(object(s,"focusLine")==before,"paused tick does not redraw or consume pool state");
    f.command("resume");s=f.command("advance");s=finishParticlePrepare(f,s);s=f.command("tick","1000016");
    for(int n=1;n<=22;++n)s=f.command("tick",std::to_string(1000016+n*50).c_str());
    check(waiting(s,"input")&&number(s,"nextTickMs")==0&&!boolean(object(s,"fallNote"),"active")&&!boolean(object(s,"focusLine"),"active"),"800/400ms fades restore true idle");
    s=f.command("title");check(!boolean(object(s,"fallNote"),"pendingResource")&&object(s,"focusLine").find("FocusLine.mzp")==std::string::npos,"Title clears logical and prepared particle state");f.passed=true;
}
static void handlerBoundsAndMissingResources(){
    for(bool mode391:{false,true}){Program p;if(mode391){p.push(5);p.op(0x391);}else{for(int i=0;i<4;++i)p.push(i);p.op(0x3a0);}p.commit();p.dialogue(0);p.op(0xf4);Fixture f(p);images(f);std::string s=woh_command(f.handle,"action","start");
        if(mode391)check(waiting(s,"transitionReady")&&boolean(object(s,"fallNote"),"pendingResource"),"391 consumes its raw scalar then prepares");
        else check(waiting(s,"error")&&s.find("underflow")!=std::string::npos,"3A0 consumes exactly five stack values");f.passed=true;}
    {Program p;p.op(0x390);p.commit();p.dialogue(0);Fixture f(p);std::string s=woh_command(f.handle,"action","start");check(waiting(s,"error")&&s.find("fallnote0.mzp")!=std::string::npos,"unprepared particle assets fail strictly");f.passed=true;}
    {Program p;p.op(0x390);p.op(0x1c0);Fixture f(p);std::string s=woh_command(f.handle,"action","start");check(waiting(s,"error")&&s.find("particles/movie")!=std::string::npos,"unreviewed movie combination is explicit");f.passed=true;}
}
static void run(){readyTickStopAndPause();handlerBoundsAndMissingResources();}
} // namespace particle_overlay_api_cases
