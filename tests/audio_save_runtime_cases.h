#pragma once
// Synthetic host tests: completion events are simulated, not decoder evidence.
namespace audio_save_cases {
static std::string extra(const mahoyo::save::Document& d){return {d.hostExtra.begin(),d.hostExtra.end()};}
static void run(){
    Program p;p.sound190(0,0,"once.hw",80,12);p.sound190(1,1,"loop.hw",60,112);p.sound190(2,0,"ended.hw",70,99);
    p.push(0);p.string("voice");p.op(0x1a0);p.dialogue(0);p.op(0xf4);
    Fixture f(std::move(p),false,"","","loop.hw 48000 48000 2 0 2400 24000\n");
    f.extraFiles.push_back(f.root/"savedata/slot3.whs");
    f.command("textSpeed","0");auto state=f.command("action","start");check(boolean(state,"canSave"),"audio does not block stable input save");
    state=f.command("audioEnded",("se2 "+std::to_string(uint64_t(number(audioChannel(state,"se2"),"id")))).c_str());
    const auto oldVoice=uint64_t(number(audioChannel(state,"voice"),"id"));
    f.command("save","2");const auto good=mahoyo::save::read(f.root/"savedata/slot2.whs");
    check(good.audio.size()==3,"save active voice, one-shot and loop, exclude completed request");
    const auto text=extra(good);check(text.rfind("WOHHOST23\n",0)==0,"audio descriptor schema21");
    state=f.command("load","2");
    for(const auto& item:std::vector<std::tuple<std::string,double,double,bool>>{{"se0",12,.8,false},{"se1",112,.6,true},{"voice",64,1,false}}){
        auto a=audioChannel(state,std::get<0>(item));check(number(a,"pan")==std::get<1>(item)&&number(a,"volume")==std::get<2>(item)&&boolean(a,"loop")==std::get<3>(item),"load restores active channel pan/gain/loop");
    }
    const auto voice=audioChannel(state,"voice");state=f.command("audioEnded",("voice "+std::to_string(oldVoice)).c_str());check(audioChannel(state,"voice")==voice,"pre-load completion cannot end restored voice");
    woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");
    check(audioChannel(state,"voice").find("voice.hw")!=std::string::npos&&number(audioChannel(state,"se1"),"pan")==112,"fresh runtime restores nonloop descriptor and stereo");
    const auto stableVoice=audioChannel(state,"voice"),stableLoop=audioChannel(state,"se1");const auto instructions=number(state,"instructions");
    const auto first=text.find("AUDIOPAN1 "),end=text.find("EFFECTREGISTRY1 ",first);check(first!=std::string::npos&&end!=std::string::npos,"bounded audio footer");
    for(const auto& bad:std::vector<std::string>{"AUDIOPAN1 2\n", "AUDIOPAN1 3\n\"se0\" 12\n\"se0\" 112\n\"voice\" 64\n", "AUDIOPAN1 3\n\"unknown\" 12\n\"se1\" 112\n\"voice\" 64\n", "AUDIOPAN1 3\n\"se0\" -1\n\"se1\" 112\n\"voice\" 64\n", "AUDIOPAN1 3\n\"se0\" 129\n\"se1\" 112\n\"voice\" 64\n", "AUDIOPAN1 3\n\"se0\" nan\n\"se1\" 112\n\"voice\" 64\n", ""}){
        auto invalid=good;auto changed=text.substr(0,first)+bad+text.substr(end);invalid.hostExtra.assign(changed.begin(),changed.end());mahoyo::save::writeAtomic(f.root/"savedata/slot3.whs",invalid);
        state=f.command("load","3");check(state.find("\"notice\":\"Loaded\"")==std::string::npos&&number(state,"instructions")==instructions&&audioChannel(state,"voice")==stableVoice&&audioChannel(state,"se1")==stableLoop,"bad21 rejects before live audio or VM mutation");
    }
    auto legacy=good;auto old=text.substr(0,first)+text.substr(end);old.replace(0,9,"WOHHOST20");old.resize(old.find("LANGUAGETEXT1 "));{auto at=old.find("EXACTLAYOUT1 ");check(at!=std::string::npos,"22 origin marker exists");old.erase(at,old.find('\n',at)-at+1);}legacy.hostExtra.assign(old.begin(),old.end());
    legacy.audio.erase(std::remove_if(legacy.audio.begin(),legacy.audio.end(),[](const auto& a){return !a.loop;}),legacy.audio.end());
    mahoyo::save::writeAtomic(f.root/"savedata/slot3.whs",legacy);state=f.command("load","3");
    check(state.find("\"notice\":\"Loaded\"")!=std::string::npos&&number(audioChannel(state,"se1"),"pan")==64,"legacy20 keeps documented center pan default");
    state=f.command("load","2");for(const auto& channel:{"voice","se0"})state=f.command("audioEnded",(std::string(channel)+" "+std::to_string(uint64_t(number(audioChannel(state,channel),"id")))).c_str());
    f.command("save","3");auto ended=mahoyo::save::read(f.root/"savedata/slot3.whs");check(ended.audio.size()==1&&ended.audio[0].channel=="se1","completed one-shots are not resurrected by next save");
    check(extra(mahoyo::save::read(f.root/"savedata/slot2.whs"))==text,"baseline slot preserved");f.passed=true;
}
}
