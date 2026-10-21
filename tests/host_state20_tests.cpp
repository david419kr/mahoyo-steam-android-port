// Codec/POD-only tests: no actual Runtime, original assets, saves or device.
#include "../runtime/host_state20.h"
#include <iostream>
#include <algorithm>
#include <functional>
using namespace mahoyo;
static unsigned checks=0;static void check(bool yes,const char* m){++checks;if(!yes)throw std::runtime_error(m);}
static std::string encode(const host_state20::State& s,const std::vector<int>& ids={1,9,63}){std::ostringstream out;host_state20::write(out,s,ids,{1,9,32,63});return out.str();}
static host_state20::State decode(const std::string& s,const std::vector<int>& ids={1,9,63}){std::istringstream in(s);auto value=host_state20::read(in,ids,{1,9,32,63});std::string trailing;if(in>>trailing)throw save::Error("Unexpected saved effect data");return value;}
static std::vector<std::string> lines(const std::string& s){std::istringstream in(s);std::vector<std::string> rows;std::string row;while(std::getline(in,row))rows.push_back(row);return rows;}
static std::string text(const std::vector<std::string>& rows){std::string s;for(const auto& row:rows)s+=row+'\n';return s;}
static std::string field(const std::string& row,size_t at,const std::string& value){std::istringstream in(row);std::vector<std::string> parts;std::string p;while(in>>p)parts.push_back(p);check(at<parts.size(),"fixture field index");parts[at]=value;std::string out;for(auto& part:parts){if(!out.empty())out+=' ';out+=part;}return out;}
static host_state20::State active(bool fade){host_state20::State s;s.registeredEffects={1,9,32,63};s.pausedEffects={1,63};s.randomState=0;s.fallNote.mode=1;s.fallNote.phase=fade?fall_note::Phase::FadingOut:fall_note::Phase::Running;s.fallNote.opacity16=65536;s.fallNote.slots[0]={0,7,-0.0f,23.f,.05f,-.2f,.000001f,-.0000001f};s.fallNote.slots[49]={2,750,700.f,-60.f,-.44f,4.f,.00001f,.0001f};if(fade){s.fallNote.fadeDurationMs=800;s.fallNote.fadeElapsedMs=123;s.fallNote.opacity16-=uint32_t((uint64_t(123)<<16)/800);}s.focusLine.phase=fade?3:2;s.focusLine.substage=fade?1:2;s.focusLine.canvasWidth=1920;s.focusLine.canvasHeight=1080;s.focusLine.fade0=fade?int32_t((uint64_t(123)<<16)/400):0;s.focusLine.durationMs=fade?400:0;s.focusLine.fadeElapsedMs=fade?123:0;s.focusLine.slots[0]={0,79,960.f,540.f,327.67f,6553};s.focusLine.slots[19]={0,0,800.f,600.f,0.f,32768};return s;}
int main(){try{
 host_state20::State off;auto legacy=encode(off,{});check(legacy==encode(decode(legacy,{}),{}),"legacy default is valid off, RNG1, no paused IDs");
 for(bool fade:{false,true})for(uint32_t seed:{0u,1u,UINT32_MAX}){auto s=active(fade);s.randomState=seed;auto bytes=encode(s);auto restored=decode(bytes);check(bytes==encode(restored),"all stable pool fields + fade + exact RNG + paused IDs roundtrip");check(host_state20::detail::bits(restored.fallNote.slots[0].x)==0x80000000u,"negative-zero float bits retained");}
 auto running50=active(false);running50.focusLine.fadeElapsedMs=50;check(encode(decode(encode(running50)))==encode(running50),"Native running steady phase retains its bounded pause elapsed");
 auto current=active(true);auto old=encode(current);unsigned negatives=0;auto reject=[&](const std::string& bad,const std::vector<int>& ids=std::vector<int>{1,9,63}){bool rejected=false;try{auto next=decode(bad,ids);current=std::move(next);}catch(const std::exception&){rejected=true;}check(rejected,"malformed footer rejected");check(encode(current)==old,"failed temporary parse never changed current state/RNG/pools");++negatives;};
 auto rows=lines(old);const size_t fall=3,firstFall=4,focus=54,firstFocus=55;check(rows.size()==75,"fixed50+20 counted rows");
 auto bad=[&](size_t row,size_t col,const char* value){auto changed=rows;changed[row]=field(changed[row],col,value);reject(text(changed));};
 bad(0,0,"EFFECTREGISTRY2");bad(0,1,"65");bad(0,1,"-1");bad(0,3,"1");bad(0,4,"30");bad(0,5,"64");bad(1,0,"EFFECTPAUSES2");bad(1,1,"65");bad(1,1,"-1");bad(1,3,"1");bad(1,2,"63");bad(1,2,"0");bad(1,3,"64");
 for(auto value:{"-1","+1","4294967296","1x","nan"})bad(2,1,value);
 bad(fall,0,"FALLNOTE2");bad(fall,2,"0");bad(fall,3,"4");bad(fall,7,"49");bad(fall,5,"800");bad(firstFall,1,"-1");bad(firstFall,2,"2143289344");bad(firstFall,3,"2139095040");bad(firstFall,4,"2139095039");bad(firstFall,1,"4294967296");
 auto invalid51=running50;invalid51.focusLine.fadeElapsedMs=51;bool rangeRejected=false;try{encode(invalid51);}catch(const std::exception&){rangeRejected=true;}check(rangeRejected,"running steady elapsed51 rejects");
 bad(focus,0,"FOCUSLINE2");bad(focus,2,"1");bad(focus,7,"0");bad(focus,9,"21");bad(firstFocus,4,"2143289344");bad(firstFocus,5,"42");bad(firstFocus,0,"-1");
 reject(old+"EFFECTPAUSES1 0\n");reject(old,{1,1,63});reject(old,{1,9});
 std::istringstream words(old);std::vector<std::string> tokens;std::string word;while(words>>word)tokens.push_back(word);std::string truncated;for(size_t n=0;n<tokens.size();++n){reject(truncated);truncated+=tokens[n]+' ';}
 // Exact same helper lifecycle is used by future temp Runtime controllers.
 unsigned draws=0;auto rng=[&](){++draws;return 1u;};fall_note::Controller fallController;fallController.restore(current.fallNote);focus_line::Controller focusController;focusController.restore(current.focusLine,987654);fallController.step(999999,false,false,rng);focusController.sample(999999,false,false,rng);check(draws==0&&focusController.resourcesPending(),"restored pools wait fresh resource ACK and never draw RNG during restore/loading");check(encode(current)==old,"validation/restore did not change source POD");check(current.registeredEffects.count(32)&&!current.pausedEffects.count(32),"completed registry ID survives without an active packet");
 std::ostringstream failed;failed.setstate(std::ios::badbit);bool badStream=false;try{host_state20::write(failed,current,{1,9,63},{1,9,32,63});}catch(const save::Error&){badStream=true;}check(badStream,"stream encode failure does not return a partial valid footer");
 std::cout<<"WH20 codec checks="<<checks<<" negative="<<negatives<<" RNGrestoreDraws="<<draws<<" CoreIntegration=false\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
