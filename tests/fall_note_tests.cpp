#include "../runtime/fall_note.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mahoyo::fall_note;
static int checks;
static uint32_t floatBits(float value){uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));return bits;}
static void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>static void reject(F f){++checks;try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("invalid FallNote state accepted");}
static void nativeVectors(){
// Whole original14F950 CPU outputs; explicit rand15 provider/render recorders.
// All90 private vectors/36180 scalar comparisons are retained in the receipt.
struct Gold{uint32_t seed,delta,type,age,x,y,vx,vy,ax,ay,seedAfter;};
const Gold gold[]={{99u,0,0u,1119u,1140391936u,3265265664u,3199918408u,1082004603u,3086723202u,981668463u,438956706u},
{99u,16,0u,1135u,1140380453u,3264766002u,3199919374u,1082008630u,3086723202u,981668463u,438956706u},
{99u,50,0u,1169u,1140356046u,3263703417u,3199921428u,1082017186u,3086723202u,981668463u,438956706u},
{5u,0,1u,255u,1135181824u,3257663488u,3201428357u,1082109460u,3096628470u,3134306071u,1800435196u},
{5u,16,1u,271u,1135168924u,3256658500u,3201430612u,1082103018u,3096628470u,3134306071u,1800435196u},
{5u,50,1u,305u,1135141499u,3254271030u,3201435403u,1082089327u,3096628470u,3134306071u,1800435196u},
{1u,0,2u,734u,3243245568u,3259498496u,3200421724u,1082088489u,947628162u,3155391676u,1766988168u},
{1u,16,2u,750u,3243628031u,3258496554u,3200419791u,1082052250u,947628162u,3155391676u,1766988168u},
{1u,50,2u,784u,3244440378u,3256381866u,3200415684u,1081975243u,947628162u,3155391676u,1766988168u}};
for(const auto& g:gold){Controller c;c.request(0);c.commit();c.resourcesReady(1000);uint32_t seed=g.seed;int draws=0;c.step(1000+g.delta,true,false,[&](){seed=seed*214013u+2531011u;++draws;return(seed>>16)&32767u;});auto p=c.state().slots[0];uint32_t actual[]={uint32_t(p.type),p.ageMs,floatBits(p.x),floatBits(p.y),floatBits(p.vx),floatBits(p.vy),floatBits(p.ax),floatBits(p.ay)};const uint32_t expected[]={g.type,g.age,g.x,g.y,g.vx,g.vy,g.ax,g.ay};for(int i=0;i<8;++i)check(actual[i]==expected[i],"Native slot scalar mismatch");check(seed==g.seedAfter&&draws==9,"shared RNG draw order");auto sprites=c.sprites();check(sprites.size()==1&&sprites[0].priority==22576&&sprites[0].frame==int(p.ageMs/50),"source cell and native overlay order");}
}
int main(){nativeVectors();Controller c;validate(State{});check(c.safe()&&!c.active(),"default legacy state off");int draws=0;auto noBirth=[&](){++draws;return 32767u;};c.request(0);check(!c.safe(),"pending command requires prepare");c.commit();check(!c.safe()&&c.pendingResource()&&!c.active(),"pending resources require existing ready barrier");c.step(1000,false,false,noBirth);check(draws==0&&c.sprites().empty(),"resource wait has no RNG or pool advancement");c.resourcesReady(1000);check(c.safe()&&c.needsTick(),"prepared pool supports stable saves");c.step(1016,true,true,noBirth);check(draws==0,"pause leaves RNG alone");c.step(1032,true,false,noBirth);check(draws==1,"natural capped frame samples RNG once");
Controller threshold;threshold.request(1);threshold.commit();threshold.resourcesReady(0);int sampled=0;auto middle=[&](){++sampled;return 2000u;};threshold.step(16,true,false,middle);check(sampled==1&&threshold.sprites().empty(),"Native mode1 probability excludes2000/32767");threshold.request(2);threshold.commit();sampled=0;threshold.step(32,true,false,middle);check(sampled==9&&threshold.sprites().size()==1,"Native mode2 probability includes2000/32767");
State s=c.snapshot();s.slots[0]={0,1190,100.f,100.f,-.4f,4.f,0.f,0.f};c.restore(s);c.rebase(2000);c.step(10000,true,false,noBirth);check(c.state().slots[0].ageMs==40,"large wall gap capped50 and atlas cycle wraps");auto p=c.state().slots[0];c.request(1);c.commit();check(c.state().slots[0].x==p.x&&c.state().slots[0].ageMs==p.ageMs,"active mode change preserves pool");
c.request(-1);c.commit();c.step(11000,true,false,noBirth);for(int i=1;i<=8;++i)c.step(11000+i*50,true,false,noBirth);check(c.state().fadeElapsedMs==400&&c.state().opacity16==32768&&c.safe(),"native800ms stop reaches half fade and is saveable");auto half=c.snapshot();int before=draws;Controller restored;restored.restore(half);check(draws==before,"restore consumes no shared RNG");restored.rebase(50000);restored.step(50016,false,false,noBirth);check(restored.state().fadeElapsedMs==400&&draws==before,"restored assets require fresh ready before advancement");restored.step(50032,true,true,noBirth);check(restored.state().fadeElapsedMs==400&&draws==before,"pause freezes restored fade");for(int i=1;i<=8;++i)restored.step(50032+i*50,true,false,noBirth);check(!restored.active()&&restored.sprites().empty(),"stop completion destroys logical pool");restored.request(2);restored.commit();check(restored.state().slots[0].type==-1,"completed stop restarts with empty pool");
State bad=half;bad.slots[0].x=std::numeric_limits<float>::quiet_NaN();reject([&](){restored.restore(bad);});check(restored.state().phase==Phase::WaitingResources,"malformed restore atomic");bad=half;bad.slots[0].ageMs=1200;reject([&](){validate(bad);});bad=half;bad.opacity16=999;reject([&](){validate(bad);});reject([&](){restored.request(3);});check(opacity8(65536)==255&&opacity8(32768)==127&&opacity8(65536,32768,32768)==63&&opacity8(65536,65536)==0,"whole Native06F230 opacity8 vectors");
std::cout<<"FallNote focused lifecycle/native vectors passed "<<checks<<" checks\n";
}
