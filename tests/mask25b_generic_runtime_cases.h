// Synthetic RGBA headers only. No original pixels, decoder, GPU or user saves.
#include "../runtime/alpha_rule.h"
namespace generic_mask_cases {
static std::string ack(Fixture& f,const std::string& state){
    auto at=state.find("\"alphaRulePrepareAck\":\"");check(at!=std::string::npos,"generic mask has prepared identity");at+=23;
    auto token=state.substr(at,state.find('"',at)-at);return f.command("transitionReady",token.c_str());
}
static void run(){
    for(int rule:{12,8,14}){
        const bool red=rule==12;const int id=red?39:2,duration=red?300:rule==8?400:650;
        const std::string image=red?"img0461":rule==8?"img2372":"synthetic_rgba";
        Program p;p.image(1000,image,id);p.alpha(id,0);p.blend(id,red?16:0,0);
        if(red){p.push(0xff1313);p.push(1);p.push(id);p.op(0x253);p.axisScale(id,1000,2200);p.rotate(id,3888);}
        p.dialogue(0);for(int n:{256,0,rule,id})p.push(n);p.op(0x25b);
        for(int n:{0,duration,255,INT32_MIN,id})p.push(n);p.op(0x245);
        for(int n:{0,1,duration})p.push(n);p.op(0x110);
        for(int n:{256,0,-1,id})p.push(n);p.op(0x25b);p.dialogue(1);p.op(0xf4);
        Fixture f(std::move(p),true);f.imageHeader(image,2552,red?764:1272);f.command("textSpeed","0");
        auto state=f.command("action","start");check(waiting(state,"input"),"generic mask starts from stable material");
        state=f.command("advance");check(waiting(state,"transitionReady")&&!boolean(state,"canSave"),"generic mask uses existing ready and save barrier");
        auto layer=layerObject(state,id);check(number(layer,"blend")==int(red?16:0)&&number(layer,"colorMode")==int(red?1:0)&&number(layer,"alpha")==1&&number(layer,"logicalAlpha")==0,"mask preserves blend/color and independent live alpha");
        check(state.find(rule<10?"rule_08.cbg":rule==12?"rule_12.cbg":"rule_14.cbg")!=std::string::npos,"mask rule resource follows Native index");
        f.command("tick","900000");state=ack(f,woh_snapshot(f.handle));check(waiting(state,"frame"),"actual prepared acknowledgement starts frame wait");
        f.command("tick","1000");state=f.command("tick",std::to_string(1000+duration/2-16).c_str());
        check(state.find("\"P\":128")!=std::string::npos&&number(layerObject(state,id),"alpha")==1,"generic duration reaches half threshold without double object fade");
        state=f.command("tick",std::to_string(1000+duration-16).c_str());check(waiting(state,"input")&&boolean(state,"canSave")&&state.find("\"alphaRule\"")==std::string::npos,"generic endpoint and clear restore ordinary stable state");
        check(number(layerObject(state,id),"blend")==int(red?16:0)&&number(layerObject(state,id),"colorMode")==int(red?1:0)&&number(layerObject(state,id),"alpha")==1,"completed mask preserves source material and live alpha");f.passed=true;
    }
    using namespace mahoyo::alpha_rule;
    Context base{"arbitrary_rgba.mzp",2552,764,-1,16,0,1,0,0,0xff1313,false,false,false,false,false};
    validate(base);
    for(int kind=0;kind<15;++kind){auto bad=base;switch(kind){case 0:bad.sourceFormat=1;break;case 1:bad.parent=0;break;case 2:bad.window=true;break;case 3:bad.crop=true;break;case 4:bad.wave=true;break;case 5:bad.noise=true;break;case 6:bad.children=true;break;case 7:bad.blend=17;break;case 8:bad.flag=1;break;case 9:bad.color=2;break;case 10:bad.contrast=1;break;case 11:bad.brightness=1;break;case 12:bad.width=8193;break;case 13:bad.height=0;break;case 14:bad.rgb=0x1000000;break;}bool rejected=false;try{validate(bad);}catch(const std::runtime_error&){rejected=true;}check(rejected,"generic mask material remains inside proven RGBA class");}
    for(int rule:{0,15}){Controller c;bool rejected=false;try{c.request(0,rule,0,256,base);}catch(const std::runtime_error&){rejected=true;}check(rejected,"generic mask rejects unknown Native rule index");}
    for(int strength:{-1,257}){Controller c;bool rejected=false;try{c.request(0,12,0,strength,base);}catch(const std::runtime_error&){rejected=true;}check(rejected,"generic mask rejects unsupported strength");}
}
}
