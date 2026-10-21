#include "../runtime/vm/machine.h"
#include <cstring>
#include <iostream>
#include <map>
using namespace mahoyo::vm;
static void check(bool condition,const char* what){if(!condition)throw std::runtime_error(what);}
static void append(std::vector<uint8_t>& v,uint32_t n){for(unsigned k=0;k<4;++k)v.push_back(uint8_t(n>>(k*8)));}
static std::vector<uint8_t> chs(const std::vector<uint32_t>& code,const std::string& suffix={}){
    std::vector<uint8_t> b(48);const char* signature="HunexCompiledScriptVer1.00";std::memcpy(b.data(),signature,std::strlen(signature));b[27]=21;
    for(auto n:code)append(b,n);b.insert(b.end(),suffix.begin(),suffix.end());b.push_back(0);return b;
}
struct TestHost:Host{
    std::map<std::string,std::vector<uint8_t>> modules;unsigned calls=0;
    std::vector<uint8_t> loadModule(const std::string& n)override{auto it=modules.find(n);if(it==modules.end())throw Error("No synthetic module");return it->second;}
    Flow invoke(uint32_t op,Machine& m)override{if(op!=0x300)throw Error("Unknown synthetic host opcode");++calls;m.setVariable(0,m.pop());return Flow::Yield;}
};
static void runAll(){
    {
        TestHost h;h.modules["test.chs"]=chs({0,7,0,3,0x21,0,5,0x22,0xf4});Machine m(h);m.start("test");check(m.run()==Flow::Halt,"math halt");check(m.pop()==20,"left operand subtraction/multiplication");
    }
    {
        // Zero branch target is module code + 40, bypasses push99.
        TestHost h;h.modules["test.chs"]=chs({0,0,1,40,0x19,0,0,99,0xf4,0x7b,0,42,0xf4,0,0});Machine m(h);m.start("test");m.run();check(m.pop()==42,"conditional branch relocation");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({1,16,0x1a,0xf4,0,8,0x1b});Machine m(h);m.start("test");m.run();check(m.pop()==8,"intra module call and return");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({0,27,0,4,0xe0,0,4,0xe1,0,600,0,2,0xea,0xf4});Machine m(h);m.start("test");m.setVariable(0,9,true);m.run();check(m.pop()==9&&m.pop()==27,"global remap and variables");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({0,123,0x300,0,124,0x300,0xf4});Machine m(h);m.start("test");check(m.run()==Flow::Yield&&h.calls==1,"initial host yield");check(m.run()==Flow::Yield&&h.calls==1,"blocked instruction not repeated");auto state=m.saveState();Machine restored(h);restored.loadState(state);check(restored.flow()==Flow::Yield,"yield state preserved");restored.resume();restored.run();check(h.calls==2&&restored.variable(0)==124,"resume consumes command once");
        auto before=restored.pc();state.back()^=1;bool error=false;try{restored.loadState(state);}catch(const Error&){error=true;}check(error&&before==restored.pc(),"corrupt save preserves live state");
    }
    {
        TestHost h;h.modules["child.chs"]=chs({0,77,0x300,0x1b});h.modules["test.chs"]=chs({1,16,0xf0,0xf4},"child");Machine m(h);m.start("test");check(m.run()==Flow::Yield,"module call yield");auto state=m.saveState();Machine r(h);r.loadState(state);check(r.moduleName()=="child.chs","module state restored");r.resume();check(r.run()==Flow::Halt&&r.moduleName()=="test.chs","module return after restored yield");
    }
    {
        TestHost h;std::vector<uint32_t> code;for(unsigned i=0;i<100;++i){code.push_back(1);code.push_back(1204);code.push_back(0xf0);}code.push_back(0xf4);h.modules["test.chs"]=chs(code,"child");h.modules["child.chs"]=chs({0x1b});Machine m(h);m.start("test");check(m.run()==Flow::Halt,"module call unload avoids64 module accumulation");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({0x7f,0,12,0xdead});Machine m(h);m.start("test");bool error=false;try{m.run();}catch(const Error& e){std::string s=e.what();error=s.find("test.chs +0xc op 0xdead line 12")!=std::string::npos;}check(error,"unknown opcode carries exact source position");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({1,0,0x18});Machine m(h);m.start("test");check(m.run(17)==Flow::Continue&&m.instructionsExecuted()==17,"bounded instruction budget");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({0x20});Machine m(h);m.start("test");bool error=false;try{m.run();}catch(const Error&){error=true;}check(error,"underflow fails explicitly");
    }
    {
        // Local byte sign extension and dword stores use tagged addresses.
        TestHost h;h.modules["test.chs"]=chs({0,0x10000000,0,255,9,0,0,0x10000000,8,0,0xf4});Machine m(h);m.start("test");m.run();check(m.pop()==-1,"signed byte load from local address space");
    }
    {
        // Copy an actual CHS string into the engine's16x256 string pool.
        TestHost h;h.modules["test.chs"]=chs({1,44,0,2,0xe8,0x90,0,2,0xe8,0x94,0xf4},"ruby");Machine m(h);m.start("test");m.run();check(m.pop()==4&&m.stringAt(0x20000200)=="ruby","string pool pointer and length");
    }
    {
        TestHost h;h.modules["test.chs"]={1,2,3};Machine m(h);bool error=false;try{m.start("test");}catch(const Error&){error=true;}check(error,"malformed CHS rejected");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({0,42,0,1,0x1e,0,5,0xe0,0x1f,0xf4});Machine m(h);m.start("test");m.run();check(m.pop()==42&&m.variable(5)==42,"scope restores native stack cells after argument pops");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({0,0,0x95,0xf4});Machine m(h);m.start("test");check(m.stringAt(0).empty(),"null tagged string is empty");m.push(0);m.run();check(m.pop()==1,"two null strings compare equal");
    }
    {
        TestHost h;h.modules["test.chs"]=chs({0xf4});Machine m(h);m.start("test");m.setFlag(0,true,true);m.setFlag(16383,true,true);m.setFlag(2047,true);m.setFlag(3,false,true);
        Machine restored(h);restored.loadState(m.saveState());check(restored.flag(0,true)&&restored.flag(16383,true)&&restored.flag(2047)&&!restored.flag(3,true),"public flags preserve existing VM save format");
        restored.start("test");check(restored.flag(0,true)&&!restored.flag(2047),"shared global flag persists across fresh module start");
        for(const auto global:{false,true}){const size_t limit=global?16384:2048;bool readError=false,writeError=false;try{restored.flag(limit,global);}catch(const Error&){readError=true;}try{restored.setFlag(limit,true,global);}catch(const Error&){writeError=true;}check(readError&&writeError,"public flag range checks");}
    }
    {
        // The real1_0 tail clears the function name and replaces its own module
        // through f3. Variables/flags are shared across this chapter boundary.
        TestHost h;h.modules["test.chs"]=chs({0,37,0,4,0xe0,0,1,0,3,0xe4,0,0,0xf6,3,64,0xf3},"next");
        h.modules["next.chs"]=chs({0,4,0xe1,0,3,0xe5,0x20,0x300,0xf4});Machine m(h);m.start("test");m.setFlag(0,true,true);
        check(m.run()==Flow::Yield&&m.moduleName()=="next.chs"&&m.variable(0)==38&&m.flag(0,true),"chapter replacement preserves variables and flags");
        Machine restored(h);restored.loadState(m.saveState());restored.resume();check(restored.run()==Flow::Halt&&restored.moduleName()=="next.chs","replacement chapter persists across save and load");
    }
}
int main(){try{runAll();std::cout<<"17 VM behavioral checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
