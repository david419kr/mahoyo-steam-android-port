// Synthetic Archive metadata/CHS only. Included after the Runtime test helpers.
static std::string syntheticArchive() {
    std::ostringstream out;out<<"WOHARCHIVE1 1a9bdcf53a9e6ff181c9312d1c9186cf97bee2db873e39f29ad6b81c1b8b540b 73 20\n";
    for(int n=0;n<20;++n)out<<"G "<<n<<" \"group_"<<n<<".cbg\"\n";
    for(int n=1;n<=73;++n)out<<"E "<<n<<' '<<(n-1)%20<<' '<<1000+n<<" 10 "<<(n==1?"10,11":std::to_string(1900+n))<<" \"Synthetic "<<n<<"\" \"preview_"<<n<<".mzp\" \"replay.chs\" 1 0 "<<n<<'\n';
    return out.str();
}
static void archiveParserAndGates() {
    auto raw=syntheticArchive();std::istringstream input(raw);auto c=mahoyo::archive::Catalog::parse(input);check(c.entries.size()==73&&c.headers.size()==20,"Archive parser preserves complete native row counts");
    std::vector<uint8_t> flags(2048);flags[10]=1;check(!c.eligible(*c.find(1),flags)&&c.read(*c.find(1),flags),"required flags are AND and separate from read state");flags[11]=1;check(c.eligible(*c.find(1),flags)&&!c.eligible(*c.find(2),flags),"all required flags expose only eligible entries");check(c.eligible(*c.find(1),{}),"native short flag-array skips out-of-range tests");
    for(auto text:{raw.substr(0,raw.rfind("E 73 ")),raw+"G 0 \"bad.cbg\"\n",raw+"X unknown\n",std::string("WOHARCHIVE1 wrong 73 20\n")}){bool rejected=false;try{std::istringstream bad(text);mahoyo::archive::Catalog::parse(bad);}catch(...){rejected=true;}check(rejected,"truncated/duplicate/unknown/unpinned Archive metadata rejects atomically");}
    auto unsafe=raw;auto at=unsafe.find("preview_1.mzp");unsafe.replace(at,13,"../bad.mzp");bool rejected=false;try{std::istringstream bad(unsafe);mahoyo::archive::Catalog::parse(bad);}catch(...){rejected=true;}check(rejected,"Archive resource paths cannot escape the package");
}
static void archiveFixtureFile(Fixture& f,const std::string& relative,const std::vector<uint8_t>& bytes){auto path=f.game/relative;fs::create_directories(path.parent_path());f.extraFiles.push_back(path);std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));check(bool(file),"write synthetic Archive file");}
static void prepareArchive(Fixture& f,bool leaf12c=false) {
    woh_destroy(f.handle);f.handle=0;auto raw=syntheticArchive();archiveFixtureFile(f,"meta/archive-catalog.tsv",{raw.begin(),raw.end()});
    Program boot(false);boot.push(1);boot.push(500);boot.op(0xe0);boot.push(9);boot.op(0xe3);boot.push(459);boot.op(0xe0);boot.push(1);boot.push(26);boot.op(0xe0);boot.string("replay.chs");boot.op(0xf3);archiveFixtureFile(f,"scripts/archive.chs",boot.build());
    Program replay;replay.image(1000,"BLACK");replay.push(8);replay.op(0xe3);replay.push(0);replay.op(0x251);replay.dialogue(0);replay.op(leaf12c?0x12c:0xf4);replay.op(0xdead);archiveFixtureFile(f,"scripts/replay.chs",replay.build());
    mahoyo::shared::State common;common.generation=20;common.variables.resize(4096);common.flags.resize(16384);common.flags[10]=common.flags[11]=1;mahoyo::shared::write(f.root/"savedata/shared.whp",common);f.handle=woh_create(f.game.string().c_str());
}
static std::string archiveObject(const std::string& state){auto begin=state.find("\"archive\":");check(begin!=std::string::npos,"Archive snapshot field exists");begin+=10;return state.substr(begin,state.find(",\"modal\":",begin)-begin);}
static void nativeArchiveLifecycle() {
    for(bool leaf:{false,true}){
        Fixture f(teatimeProgram(3));prepareArchive(f,leaf);f.command("textSpeed","0");auto title=f.command("title"),state=f.command("archiveOpen");auto catalog=archiveObject(state);auto gen=uint64_t(number(catalog,"id"));
        check(waiting(state,"archive")&&state.find("Synthetic 1\"")!=std::string::npos&&state.find("Synthetic 2\"")==std::string::npos&&number(state,"instructions")==0&&!boolean(state,"canSave"),"catalog hides locked rows, has no gameplay VM and cannot save");
        state=f.command("archivePlay",(std::to_string(gen+1)+" 1").c_str());check(waiting(state,"archive"),"stale Archive generations cannot start a replay");
        state=f.command("archivePlay",(std::to_string(gen)+" 1").c_str());check(waiting(state,"input")&&number(state,"z")==1001&&state.find("\"module\":\"replay.chs\"")!=std::string::npos,"Archive runs its CHS bootstrap with shared8 ChapterType before the original F3 replacement");
        f.command("save","2");auto document=mahoyo::save::read(f.root/"savedata/slot2.whs");InspectHost inspector;mahoyo::vm::Machine vm(inspector);vm.loadState(document.vm);check(vm.variable(500)==1&&vm.variable(26)==1&&vm.variable(459)==0&&vm.variable(8,true)==1001,"replay saves retain the original VM context without another host schema");
        f.command("title");state=f.command("load","2");check(waiting(state,"input"),"replay slot loads at its original input boundary");state=f.command("advance");check(waiting(state,"archive")&&number(archiveObject(state),"selected")==1&&number(archiveObject(state),"id")>gen,"F4 and12c reopen the same selected catalog entry after save/load");
        auto payload=std::to_string(uint64_t(number(archiveObject(state),"id")));state=f.command("archiveBack",payload.c_str());check(state.find("\"scene\":\"title\"")!=std::string::npos&&number(state,"instructions")==0,"closing Archive returns to title, never a destroyed replay VM");
        state=readyTeatime(f);auto before=number(state,"instructions");state=modalCommand(f,state,"archive");check(waiting(state,"archive")&&number(state,"instructions")==0&&number(archiveObject(state),"checkpoint")==3,"TEATIME Archive destroys the suspended play VM and preserves checkpoint selector");
        gen=uint64_t(number(archiveObject(state),"id"));state=f.command("archivePlay",(std::to_string(gen)+" 1").c_str());f.command("save","2");document=mahoyo::save::read(f.root/"savedata/slot2.whs");vm.loadState(document.vm);check(vm.variable(459)==3&&vm.variable(9,true)==3&&before>0,"checkpoint context is shared9/local459, not StoryId");f.passed=true;
    }
}
static void nativeMusicUnlockAndTitleCue() {
    for(int id:{-1,0,1,64,65}){
        Program p;p.bgm(id);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));woh_destroy(f.handle);f.handle=0;auto path=f.game/"meta/music-map.tsv";fs::create_directories(path.parent_path());f.extraFiles.push_back(path);{std::ofstream file(path);file<<id<<"\tfake.hw\n";}f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");f.command("action","start");f.command("pause");auto common=mahoyo::shared::read(f.root/"savedata/shared.whp");check(common.has_value(),"180 music record persists on pause");for(int flag=256;flag<=321;++flag)check(bool(common->flags[size_t(flag)])==(id>=0&&id<65&&flag==256+id),"180 records exactlyu32ID<65 includingstopID0");f.passed=true;
    }
    Fixture f(appendProgram(),false,{}, {},"x_m64.hw 11664000 48000 2 1 648000 5400000\nm42.hw 5396032 48000 2 1 0 5264828\n");auto state=f.command("title"),cue=audioChannel(state,"bgm");check(cue.find("x_m64.hw")!=std::string::npos&&boolean(cue,"loop")&&number(cue,"volume")==1&&number(cue,"startFrame")==648000&&number(cue,"endFrame")==5400000,"uncompleted title plays Native x_M64 with its intro/partial loop");auto id=number(cue,"id");state=f.command("title");check(number(audioChannel(state,"bgm"),"id")==id,"unchanged title cue does not restart its decoder");check(!fs::exists(f.root/"savedata/shared.whp"),"title alone cannot manufacture a legacy-import base");f.command("action","start");auto common=mahoyo::shared::read(f.root/"savedata/shared.whp");check(common&&common->flags[321],"first real VM persists heard title321 despite180ID65 exclusion");woh_destroy(f.handle);f.handle=0;for(int flag:{128,65,73,72,206,129,130,131,132,133,134})common->flags[size_t(flag)]=1;++common->generation;mahoyo::shared::write(f.root/"savedata/shared.whp",*common);f.handle=woh_create(f.game.string().c_str());state=f.command("title");cue=audioChannel(state,"bgm");check(cue.find("m42.hw")!=std::string::npos&&number(cue,"endFrame")==5264828,"all eleven completion flags select Native M42 title track");common=mahoyo::shared::read(f.root/"savedata/shared.whp");check(common&&common->flags[298],"completed title records Native flag298");f.passed=true;
}
static mahoyo::save::Document emptySceneLegacy(mahoyo::save::Document document,int schema) {
    check(document.layers.empty()&&schema>=3&&schema<=16,"bounded empty-scene legacy fixture");
    auto original=std::string(document.hostExtra.begin(),document.hostExtra.end());auto at=original.find("COMMON1 ");check(at!=std::string::npos,"source fixture has its real common generation");uint64_t epoch=std::stoull(original.substr(at+8));
    std::ostringstream data;data<<"WOHHOST"<<schema<<"\n0\n0\n";
    if(schema>=7)data<<"PRELOADS1 0\n0\n0\n0\n0\n";
    if(schema>=8)data<<"WINDOWFADE2 1 65536 0\n";
    if(schema>=9)data<<"EFFECTFRAMES1 0\n";
    if(schema>=10)data<<"TEATIME1 0\nCOMMON1 "<<epoch<<'\n';
    if(schema>=12)data<<"GEOMETRYMODES1 0\n";
    if(schema>=13)data<<"WAVES1 0\n";
    if(schema>=14)data<<"LOCAL270CACHE1 0 0\n";
    if(schema>=15)data<<"FONTCARRY1 0\n";
    if(schema>=16)data<<"SCREENWAVE1 0 0\n";
    auto text=data.str();document.hostExtra.assign(text.begin(),text.end());return document;
}
static void titleOnlyNeverMasksLegacyImport() {
    for(int schema=3;schema<=16;++schema)for(bool restart:{false,true})for(bool code24Record:{false,true}){
        Program p;p.push(99);p.push(17);p.op(0xe2);p.push(1);p.push(9);p.op(0xe6);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");auto originalState=f.command("action","start");f.command("save","2");auto source=mahoyo::save::read(f.root/"savedata/slot2.whs");auto old=emptySceneLegacy(source,schema);InspectHost h;mahoyo::vm::Machine sourceVm(h);sourceVm.loadState(source.vm);
        woh_destroy(f.handle);f.handle=0;mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",old);check(fs::remove(f.root/"savedata/shared.whp"),"remove only private common fixture");
        fs::create_directories(f.game/"meta");{std::ofstream file(f.game/"meta/audio-loop-map.tsv");file<<"x_m64.hw 11664000 48000 2 1 648000 5400000\nm42.hw 5396032 48000 2 1 0 5264828\n";}
        if(code24Record){mahoyo::shared::State title;title.generation=1;title.variables.resize(4096);title.flags.resize(16384);title.flags[321]=1;mahoyo::shared::write(f.root/"savedata/shared.whp",title);}
        f.handle=woh_create(f.game.string().c_str());if(!code24Record)check(!fs::exists(f.root/"savedata/shared.whp"),"no-common title cannot persist an empty migration base");
        if(restart){woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());if(!code24Record)check(!fs::exists(f.root/"savedata/shared.whp"),"title-only exit then fresh title still leaves legacy base unmasked");}
        auto state=f.command("load","2");check(waiting(state,"input")&&state.find("\"notice\":\"Loaded\"")!=std::string::npos&&number(state,"instructions")==number(originalState,"instructions")&&number(state,"modulePc")==number(originalState,"modulePc"),"each WH3..16 import restores the precise VM input boundary");f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");mahoyo::vm::Machine loaded(h);loaded.loadState(saved.vm);
        auto expected=sourceVm.sharedFlags();expected[321]=1;check(loaded.sharedVariables()==sourceVm.sharedVariables()&&loaded.sharedFlags()==expected,"legacy numbers/flags import exactly with only heard-title321 added");
        check(loaded.pc()==sourceVm.pc()&&loaded.stack()==sourceVm.stack(),"legacy PC and operand stack survive title-only migration");for(size_t i=0;i<1280;++i)check(loaded.variable(i)==sourceVm.variable(i),"legacy local variables survive title-only migration");
        auto common=mahoyo::shared::read(f.root/"savedata/shared.whp");check(common&&common->variables==sourceVm.sharedVariables()&&common->flags==expected&&common->generation>0,"imported common persists exact numeric/flag contents");f.passed=true;
    }
    for(int kind=0;kind<3;++kind){
        Program p;p.push(99);p.push(17);p.op(0xe2);p.push(1);p.push(9);p.op(0xe6);p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.command("textSpeed","0");f.command("action","start");f.command("save","2");auto old=emptySceneLegacy(mahoyo::save::read(f.root/"savedata/slot2.whs"),3);woh_destroy(f.handle);f.handle=0;mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",old);
        mahoyo::shared::State common;common.generation=kind==2?2:1;common.variables.resize(4096);common.flags.resize(16384);common.flags[321]=1;if(kind==0)common.variables[17]=777;if(kind==1)common.flags[10]=1;mahoyo::shared::write(f.root/"savedata/shared.whp",common);
        {std::ofstream file(f.game/"meta/audio-loop-map.tsv");file<<"x_m64.hw 11664000 48000 2 1 648000 5400000\n";}
        f.handle=woh_create(f.game.string().c_str());f.command("load","2");f.command("save","2");InspectHost h;mahoyo::vm::Machine loaded(h);loaded.loadState(mahoyo::save::read(f.root/"savedata/slot2.whs").vm);auto persisted=mahoyo::shared::read(f.root/"savedata/shared.whp");check(loaded.sharedVariables()==common.variables&&loaded.sharedFlags()==common.flags&&persisted&&persisted->generation==common.generation,"any other numeric/flag or generation preserves authoritative common precedence without merging slot progress");f.passed=true;
    }
}
static void titleCuePreservesLegacyCheckpoint() {
    Program program;program.image(1000,"BLACK");program.push(1);program.push(500);program.op(0xe0);program.bgm(1,65);program.dialogue(0);program.op(0xf4);
    Fixture f(std::move(program),false,{}, {},"old.hw 48000 48000 2 1 0 0\n");
    woh_destroy(f.handle);f.handle=0;auto music=f.game/"meta/music-map.tsv";f.extraFiles.push_back(music);{std::ofstream file(music);file<<"1\told.hw\n";}f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");auto before=f.command("action","start");f.command("save","2");auto original=mahoyo::save::read(f.root/"savedata/slot2.whs");
    InspectHost h;mahoyo::vm::Machine originalVm(h);originalVm.loadState(original.vm);check(!originalVm.flag(321,true)&&originalVm.variable(500)==1&&originalVm.variable(26)==0,"legacy fixture models ordinary1_5 marker without Archive StoryId or new title flag");
    auto legacy=host15Fixture(original);mahoyo::save::writeAtomic(f.root/"savedata/slot2.whs",legacy);woh_destroy(f.handle);f.handle=0;
    auto commonBefore=mahoyo::shared::read(f.root/"savedata/shared.whp");check(commonBefore.has_value(),"legacy checkpoint has an existing independent shared manager");
    {std::ofstream file(f.game/"meta/audio-loop-map.tsv",std::ios::app);file<<"x_m64.hw 11664000 48000 2 1 648000 5400000\nm42.hw 5396032 48000 2 1 0 5264828\n";}
    f.handle=woh_create(f.game.string().c_str());auto commonTitle=mahoyo::shared::read(f.root/"savedata/shared.whp");check(commonTitle&&commonTitle->generation==commonBefore->generation+1&&commonTitle->flags[321],"fresh title records exactly one new common-progress generation");
    auto expectedFlags=commonBefore->flags;expectedFlags[321]=1;check(commonTitle->flags==expectedFlags&&commonTitle->variables==commonBefore->variables,"only Native title music flag321 changes; unrelated flags and global numbers are exact");
    auto restored=f.command("load","2");check(waiting(restored,"input")&&restored.find("\"notice\":\"Loaded\"")!=std::string::npos&&visibleLayers(restored)==visibleLayers(before)&&number(restored,"instructions")==number(before,"instructions")&&number(restored,"modulePc")==number(before,"modulePc"),"WH15 slot restores exact original PC/locals/layers rather than the transient title state");
    auto audio=audioChannel(restored,"bgm");check(audio.find("old.hw")!=std::string::npos&&boolean(audio,"loop")&&std::abs(number(audio,"volume")-.65)<1e-9,"saved BGM replaces new title cue with original gain and loop");
    f.command("save","2");auto saved=mahoyo::save::read(f.root/"savedata/slot2.whs");mahoyo::vm::Machine loaded(h);loaded.loadState(saved.vm);
    for(size_t n=0;n<1280;++n)check(loaded.variable(n)==originalVm.variable(n),"all legacy VM locals are preserved");
    check(loaded.pc()==originalVm.pc()&&loaded.stack()==originalVm.stack()&&loaded.instructionsExecuted()==originalVm.instructionsExecuted()&&loaded.sharedVariables()==originalVm.sharedVariables(),"legacy VM execution and shared numeric state are unchanged");
    auto expected=originalVm.sharedFlags();expected[321]=1;check(loaded.sharedFlags()==expected,"loaded VM adopts precise newer common flag321 without weakening its other shared-flag checks");check(currentHostSchema(saved),"resaved legacy checkpoint uses the current WH16 container");f.passed=true;
}
