// Synthetic profile/clock/save fixtures; no game pixels or user save slots.
static Program nativeWaveBatchProgram(int preset,int wrong=0){
    const int blends[]={0,0,0,0,0,0,0,11,0,19,0,0,0},colours[]={39,39,39,39,0,39,39,70,52,0,27,0,0};
    const int axisX[]={1000,1000,1000,-1000,1000,1000,1000,1000,1200,1000,700,2500,1000};
    const int axisY[]={1000,1000,1000,1000,1000,1000,1000,1000,1200,1400,700,2500,1000};
    const int rotation[]={-2000,2800,-1800,1800,0,-2000,2800,0,-4436,12300,-4700,0,0};
    check(preset>=24&&preset<=36,"batch test preset bounds");int i=preset-24;Program p;
    if(preset==35)p.image(1000,"grid",53,{{0,0,1920,1080,0,0}});
    p.image(1000,"grid",45,{},preset==35?53:-1);p.position(45,320,180);
    p.alpha(45,preset==31?0:preset==36?128:255);
    for(int n:{wrong==3?1:0,wrong==2?(colours[i]==0?1:0):colours[i],45})p.push(n);p.op(0x253);
    p.blend(45,wrong==1?(blends[i]==0?1:0):blends[i],0);
    if(preset==33||wrong==4){p.push(wrong==4?1:89);p.push(45);p.op(0x254);}
    p.axisScale(45,axisX[i],axisY[i]);p.rotate(45,rotation[i]);p.wave(45,preset);p.waitWave(45);p.dialogue(0);p.op(0xf4);return p;
}
static void nativeWaveBatchProfilesRestoreAndClock(){
    const int widths[]={1022,1022,1532,1532,2552,1022,512,2552,3572,3572,1532,2042,2042};
    const int heights[]={1526,1526,1526,1526,1526,1526,1526,1272,2042,2042,1526,2034,5082};
    for(int preset=24;preset<=36;++preset){
        Fixture f(nativeWaveBatchProgram(preset),false,"","grid 100 100\n");f.imageHeader("grid",widths[preset-24],heights[preset-24]);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));
        check(hasWave(state,45)&&number(waveObject(state,45),"preset")==preset,"batch exact material profile accepts arbitrary actor45");
        woh_wave_clock(f.handle,1000);woh_snapshot(f.handle);woh_wave_clock(f.handle,1250);state=woh_snapshot(f.handle);
        check(number(waveObject(state,45),"elapsedMs")==250,"batch ready source clock advances atalpha0 and128 too");
        auto before=visibleLayers(state);f.command("save","2");woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");
        check(visibleLayers(state)==before&&number(waveObject(state,45),"elapsedMs")==250,"batch fresh restore retains material/geometry/phase");
        woh_wave_clock(f.handle,1000000);woh_snapshot(f.handle);woh_wave_clock(f.handle,1000016);state=woh_snapshot(f.handle);
        check(number(waveObject(state,45),"elapsedMs")==266,"batch fresh platform clock has no old debt");f.passed=true;
    }
}
static void nativeWaveBatchMode0BoundsAndProfiles(){
    for(int preset:{31,36}){
        const int w=preset==31?2552:2042,h=preset==31?1272:5082;
        Fixture f(nativeWaveBatchProgram(preset),true,"","grid 100 100\n");f.imageHeader("grid",w,h);f.command("textSpeed","0");auto state=acknowledge(f,f.command("action","start"));check(hasWave(state,45),"reviewed1590/5084 mode0 bitmap accepted");
        f.command("save","2");f.imageHeader("grid",preset==31?2560:2560,preset==31?1280:4068);woh_destroy(f.handle);f.handle=woh_create(f.game.string().c_str());state=f.command("load","2");
        check(state.find("Unsupported wave"+std::to_string(preset)+" bitmap")!=std::string::npos,"freshrestore rejects above reviewed1590/5084 cells");f.passed=true;
    }
    for(int preset:{24,31,33,35,36})for(int wrong=1;wrong<=4;++wrong){
        Fixture f(nativeWaveBatchProgram(preset,wrong),true,"","grid 100 100\n");f.imageHeader("grid",1022,764);auto state=std::string(woh_command(f.handle,"action","start"));
        if(waiting(state,"transitionReady")){auto transition=state.substr(state.find("\"transition\":"));state=woh_command(f.handle,"transitionReady",std::to_string(uint64_t(number(transition,"id"))).c_str());}
        check(state.find("Unsupported wave24..36 Native context")!=std::string::npos,"batch wrong blend/colour/rgb/contrast fails closed");f.passed=true;
    }
}
