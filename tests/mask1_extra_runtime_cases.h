// Exact7_EX rule1/256/alpha0->255/1000ms; retain C10 and strict negatives.
static void mask1Rule(Program& p,int id,int rule=1,int strength=256,int middle=0){for(int n:{strength,middle,rule,id})p.push(n);p.op(0x25b);}
static void mask1Fade(Program& p,int id,int duration=1000){for(int n:{0,duration,255,INT32_MIN,id})p.push(n);p.op(0x245);}
static std::string mask1Ack(Fixture& f,const std::string& s){auto at=s.find("\"alphaRulePrepareAck\":\"");check(at!=std::string::npos,"mask1 prepared acknowledgement token exists");at+=23;auto token=s.substr(at,s.find('"',at)-at);return f.command("transitionReady",token.c_str());}
static void nativeMask1ExtraAndNoisePrepare110(){
    for(bool clear:{true,false}){
        Program p;p.image(1000,"img0158",1);p.alpha(1,0);p.dialogue(0);mask1Rule(p,1);mask1Fade(p,1);for(int n:{1,8,1})p.push(n);p.op(0x27f);if(clear)mask1Rule(p,1,-1);p.dialogue(1);p.op(0xf4);
        Fixture f(std::move(p),true);f.imageHeader("img0158",2552,2034);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"input"),"rule1 starts from stable input");
        state=f.command("advance");check(waiting(state,"transitionReady")&&state.find("\"rule\":1")!=std::string::npos&&state.find("rule_01.cbg")!=std::string::npos&&state.find("\"S\":256")!=std::string::npos&&!boolean(state,"canSave"),"exactrule1 dynamic DTO and pending save-busy");
        f.command("tick","900000");state=mask1Ack(f,woh_snapshot(f.handle));check(waiting(state,"animation"),"27f mask8 waits for mask1 Native job");
        f.command("tick","1000");state=f.command("tick","1484");check(waiting(state,"animation")&&state.find("\"P\":128")!=std::string::npos&&state.find("\"logicalAlpha\":0")!=std::string::npos&&number(layerObject(state,1),"alpha")==1,"rule1 linearP128 at500ms with independent forced live alpha");
        state=f.command("tick","1984");check(waiting(state,"input")&&(clear?state.find("\"alphaRule\"")==std::string::npos:state.find("\"alphaRule\":null")!=std::string::npos),"rule1 endpoint releases visible mask before27f resumes");
        check(boolean(state,"canSave")==clear&&number(layerObject(state,1),"alpha")==1,"positive rawrule remains save-busy until explicit clear");
        if(clear){f.command("save","2");f.command("title");state=f.command("load","2");check(waiting(state,"input")&&boolean(state,"canSave")&&number(layerObject(state,1),"alpha")==1,"cleared rule1 keeps ordinary WH19 stable restore");}
        f.passed=true;
    }
    for(auto request:std::vector<std::pair<int,int>>{{15,256},{1,257},{0,256}}){
        Program p;p.image(1000,"img0158",1);p.alpha(1,0);mask1Rule(p,1,request.first,request.second);mask1Fade(p,1);p.dialogue(0);
        Fixture f(std::move(p),true);f.imageHeader("img0158",2552,2034);auto state=std::string(woh_command(f.handle,"action","start"));check(waiting(state,"error")&&state.find("Unreviewed transient alpha-rule request")!=std::string::npos,"generic mask still rejects out-of-domain rule/strength");f.passed=true;
    }
    for(bool capture:{false,true}){
        Program p;p.image(1000,"img0158",1);p.alpha(1,0);mask1Rule(p,1);mask1Fade(p,1);if(capture){p.push(1);p.push(1);p.op(0x279);}else p.clone(2,1,"");p.dialogue(0);
        Fixture f(std::move(p),true);f.imageHeader("img0158",2552,2034);auto state=std::string(woh_command(f.handle,"action","start"));check(waiting(state,"error")&&state.find("Transient alpha-rule save/clone/capture is unsupported")!=std::string::npos,"newrule1 keeps clone/capture strict rejection");f.passed=true;
    }
    // GPU Screen1 failure originated in an ordinary direct257 before110;
    // it must prepare state, rather than removing the renderer's guard.
    {Program p;p.image(1000,"fake",21);p.dialogue(0);p.noise(21,1);for(int n:{0,1,600})p.push(n);p.op(0x110);p.dialogue(1);p.op(0xf4);
     Fixture f(std::move(p),true);f.imageHeader("noise0",64,64);f.command("textSpeed","0");f.command("action","start");auto state=f.command("advance");auto l=layerObject(state,21);
     check(waiting(state,"transitionReady")&&l.find("\"noise\":{")!=std::string::npos&&number(l,"decimalMode")==1&&number(l,"periodMs")==50,"direct257 before110 exports validated noise state at prepare");
     auto at=state.find("\"transition\":{");state=f.command("transitionReady",std::to_string(uint64_t(number(state.substr(at),"id"))).c_str());check(waiting(state,"frame"),"directnoise110 preserves wait-clock flow");f.command("tick","1000");state=f.command("tick","1584");check(waiting(state,"input")&&layerObject(state,21).find("\"noise\":{")!=std::string::npos,"prepared noise survives110 resume without bypassing material checks");f.passed=true;}
}
