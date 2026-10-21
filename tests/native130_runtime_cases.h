// Native130 reads replay mode; original stack and existing WH18 context survive.
static std::vector<int32_t> native130SavedStack(Fixture& f){
    f.command("save","2");auto doc=mahoyo::save::read(f.root/"savedata/slot2.whs");
    check(currentHostSchema(doc),"130 retains WH18");InspectHost host;mahoyo::vm::Machine machine(host);machine.loadState(doc.vm);return machine.stack();
}
static Program native130Program(const std::vector<int32_t>& values,int story=0){
    Program p;if(story){p.push(story);p.push(26);p.op(0xe0);}
    for(auto value:values)p.push(value);p.op(0x130);p.dialogue(0);p.op(0x130);p.dialogue(1);p.op(0xf4);return p;
}
static void native130MainStackAndFreshRestore(){
    for(const auto& values:std::vector<std::vector<int32_t>>{{},{1,1,0,0,0,0,0,1,1,0,1,1,0,1,0,0,0,1,0,0,1,0,1},{INT32_MIN,-17,INT32_MAX}}){
        Fixture f(native130Program(values));f.command("textSpeed","0");auto state=f.command("action","start");
        check(waiting(state,"input")&&number(state,"nextTickMs")==0,"130 is immediate, no timer");
        auto expected=values;expected.push_back(0);check(native130SavedStack(f)==expected,"normal130 preserves existing operands and appends replay mode0");
        woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");
        check(waiting(state,"input"),"normal130 fresh restore input");state=f.command("advance");expected.push_back(0);
        check(waiting(state,"input")&&native130SavedStack(f)==expected,"restored normal context still returns0");f.passed=true;
    }
}
static void native130ArchiveModeAndTitleReset(){
    for(int story:{1,23}){
        Fixture f(native130Program({-91}));prepareArchive(f);woh_destroy(f.handle);f.handle=0;
        auto common=mahoyo::shared::read(f.root/"savedata/shared.whp");check(bool(common),"130 Archive common fixture exists");common->flags[1900+story]=1;++common->generation;mahoyo::shared::write(f.root/"savedata/shared.whp",*common);
        archiveFixtureFile(f,"scripts/replay.chs",native130Program({-91},story).build());f.handle=woh_create(f.game.string().c_str());f.command("textSpeed","0");
        auto state=f.command("archiveOpen");auto gen=uint64_t(number(archiveObject(state),"id"));state=f.command("archivePlay",(std::to_string(gen)+" "+std::to_string(story)).c_str());
        check(waiting(state,"input")&&native130SavedStack(f)==std::vector<int32_t>({-91,1}),"Archive130 returns mode1 independent of StoryId1 or23");
        woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");state=f.command("advance");
        check(waiting(state,"input")&&native130SavedStack(f)==std::vector<int32_t>({-91,1,1}),"fresh restored Archive context returns1 again");
        f.command("title");state=f.command("action","start");check(waiting(state,"input")&&native130SavedStack(f)==std::vector<int32_t>({-91,0}),"Title/new main clears replay mode");f.passed=true;
    }
}
