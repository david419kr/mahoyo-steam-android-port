#pragma once
// Authored fixtures only; rewind reconstruction never reads or writes a user slot.
namespace history_rewind_cases {
static Program story(){
    Program p;p.image();historyVoice(p,"rewind_first");fontText(p,"FIRST",1,1);
    p.position(0,100,70);historyVoice(p,"rewind_second");fontText(p,"SECOND",1,1);
    p.position(0,200,90);historyVoice(p,"rewind_third");fontText(p,"THIRD",1,1);p.op(0xf4);return p;
}
static std::string loops(){return "rewind_first.hw 48000 48000 1 0 0 0\nrewind_second.hw 48000 48000 1 0 0 0\nrewind_third.hw 48000 48000 1 0 0 0\n";}
static std::string select(const std::string& s,size_t start,size_t count=1){return historyTokenValue(s,"historyRevision")+" "+std::to_string(start)+" "+std::to_string(count);}
static std::string visibleText(const std::string& s){auto a=s.find("\"text\":[");auto b=s.find(",\"actions\":",a);return s.substr(a,b-a);}
static void retroactiveSaveAndVisualState(){
    Fixture f(story(),true,"","",loops());f.command("textSpeed","0");auto first=f.firstComplete();
    auto second=f.command("advance");auto third=f.command("advance");auto oldAudioId=number(audioChannel(third,"voice"),"id");
    f.command("save","2");const auto path=f.root/"savedata/slot2.whs";auto doc=mahoyo::save::read(path);check(std::string(doc.hostExtra.begin(),doc.hostExtra.end()).find("WOHHOST23")!=std::string::npos,"pre-existing save format has source history without rewind checkpoints");
    std::ifstream in(path,std::ios::binary);std::string before{std::istreambuf_iterator<char>(in),{}};in.close();
    f.command("title");f.command("load","2");auto history=f.command("history");
    auto common=f.root/"savedata/shared.whp";std::ifstream commonBeforeFile(common,std::ios::binary);std::string commonBefore{std::istreambuf_iterator<char>(commonBeforeFile),{}};commonBeforeFile.close();
    auto result=f.command("historyRewind",select(history,0).c_str());
    check(result.find("\"scene\":\"play\"")!=std::string::npos&&waiting(result,"input"),"loaded legacy source history can reconstruct an earlier scene");
    check(visibleLayers(result)==visibleLayers(first),"rewind restores complete prior layer geometry rather than replacing dialogue only");
    check(visibleText(result).find("FIRST")!=std::string::npos&&visibleText(result).find("SECOND")==std::string::npos,"rewind drops later displayed text");
    check(audioChannel(result,"voice").find("rewind_first.hw")!=std::string::npos&&number(audioChannel(result,"voice"),"id")>oldAudioId,"rewind restarts the correct voice with a fresh callback identity");
    std::ifstream commonAfterFile(common,std::ios::binary);std::string commonAfter{std::istreambuf_iterator<char>(commonAfterFile),{}};check(commonBefore==commonAfter,"successful rewind preserves common progress files too");
    auto audio=audioChannel(result,"voice");result=f.command("audioEnded",("voice "+std::to_string(uint64_t(oldAudioId))).c_str());check(audioChannel(result,"voice")==audio,"stale pre-rewind completion is ignored");
    result=f.command("advance");check(visibleLayers(result)==visibleLayers(second)&&audioChannel(result,"voice").find("rewind_second.hw")!=std::string::npos,"execution resumes along the original script after rewind");
    std::ifstream afterFile(path,std::ios::binary);std::string after{std::istreambuf_iterator<char>(afterFile),{}};check(before==after,"history rewind never overwrites manual save slots");f.passed=true;
}
static void invalidRequestsRemainAtomic(){
    Fixture f(story(),true,"","",loops());f.command("textSpeed","0");f.firstComplete();f.command("advance");auto h=f.command("history");auto layers=visibleLayers(h),text=visibleText(h),audio=historyAudio(h);
    for(const auto& payload:{std::string("0 0 1"),select(h,999),select(h,0,0)}){
        auto result=f.command("historyRewind",payload.c_str());check(result.find("Unable to restore this history scene.")!=std::string::npos,"invalid rewind returns a nonfatal localized notice");
        check(visibleLayers(result)==layers&&visibleText(result)==text&&historyAudio(result)==audio&&result.find("\"scene\":\"history\"")!=std::string::npos,"failed rewind preserves the live scene and audio");
    }f.passed=true;
}
static void trimmedHistoryAndDuplicateLines(){
    Program p;auto loop=uint32_t(p.words.size()*4);fontText(p,"SAME",1,1);p.op(1);p.op(loop);p.op(0x18);Fixture f(std::move(p));f.command("textSpeed","0");auto s=f.command("action","start");for(int i=1;i<600;++i)s=f.command("advance");
    check(number(s,"textLine")==600,"synthetic story reached its repeated current line");s=f.command("history");s=f.command("historyRewind",select(s,20).c_str());
    check(s.find("\"scene\":\"play\"")!=std::string::npos&&number(s,"textLine")==109,"execution-point verification disambiguates identical lines after 512-row history trimming");
    s=f.command("advance");check(number(s,"textLine")==110,"trimmed rewind preserves exact subsequent script position");f.passed=true;
}
static void replayFailureDoesNotPersistProgress(){
    Fixture f(story(),true,"","",loops());f.command("textSpeed","0");f.firstComplete();f.command("advance");auto h=f.command("history");auto layers=visibleLayers(h),audio=historyAudio(h);
    auto script=f.game/"scripts/1_0.chs";auto altered=story().build();const std::string a="SECOND",b="BROKEN";auto at=std::search(altered.begin(),altered.end(),a.begin(),a.end());check(at!=altered.end(),"synthetic script pool contains replacement fixture");std::copy(b.begin(),b.end(),at);{std::ofstream out(script,std::ios::binary);out.write(reinterpret_cast<const char*>(altered.data()),altered.size());}
    auto common=f.root/"savedata/shared.whp";std::ifstream beforeFile(common,std::ios::binary);std::string before{std::istreambuf_iterator<char>(beforeFile),{}};beforeFile.close();
    auto failed=f.command("historyRewind",select(h,0).c_str());check(failed.find("Unable to restore this history scene.")!=std::string::npos&&visibleLayers(failed)==layers&&historyAudio(failed)==audio,"diverged replay leaves original runtime intact");
    std::ifstream afterFile(common,std::ios::binary);std::string after{std::istreambuf_iterator<char>(afterFile),{}};check(before==after,"isolated replay cannot mutate shared progress even on failure");f.passed=true;
}
static void run(){retroactiveSaveAndVisualState();invalidRequestsRemainAtomic();trimmedHistoryAndDuplicateLines();replayFailureDoesNotPersistProgress();}
}
