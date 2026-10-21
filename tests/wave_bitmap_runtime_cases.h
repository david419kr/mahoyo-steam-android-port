// Synthetic intrinsic-bitmap guard checks for reviewed local12/13/14/15/16/17/18/19.
static void nativeWaveTwelveThirteenBitmapBounds(){
    for(int preset:{12,13,14,15,16,17,18,19}){
        const int cellHeight=preset==13||preset==14||preset==18?64:256;
        for(auto dims:std::vector<std::array<int,2>>{{13824,37*cellHeight},{12800,40*cellHeight},{16385,cellHeight}}){
            Program p;p.image(1,"grid",24);if(preset==19)p.blend(24,16,0);p.wave(24,preset);p.commit();p.dialogue(0);p.op(0xf4);Fixture f(std::move(p));f.imageHeader("grid",dims[0],dims[1]);f.command("textSpeed","0");auto state=f.command("action","start");check(waiting(state,"transitionReady"),"12/13/14/15/16/17/18/19 bitmap validation waits for prepared dimensions");auto transition=state.substr(state.find("\"transition\":"));state=woh_command(f.handle,"transitionReady",std::to_string(uint64_t(number(transition,"id"))).c_str());
            if(dims[0]==13824){check(hasWave(state,24)&&state.find("\"error\":\"\"")!=std::string::npos,"999 native cells12/13/14/15/16/17/18/19 accepted despite tiny logical display");if(waiting(state,"frame"))state=f.command("tick","1000");check(waiting(state,"input"),"999cell fixture finishes its initial commit before save");f.command("save","2");check(fs::exists(f.root/"savedata/slot2.whs"),"valid999cell checkpoint written");f.imageHeader("grid",12800,40*cellHeight);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=woh_command(f.handle,"load","2");check(state.find("Unsupported wave"+std::to_string(preset)+" bitmap")!=std::string::npos&&state.find("\"notice\":\"Loaded\"")==std::string::npos,"fresh restore rejects1000cell12/13/14/15/16/17/18/19 before installing checkpoint");}
            else check(state.find("Unsupported wave"+std::to_string(preset)+" bitmap")!=std::string::npos,"12/13/14/15/16/17/18/19 rejects1000 cells or extent above16384");f.passed=true;
        }
    }
}
