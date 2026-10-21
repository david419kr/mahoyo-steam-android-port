#include "vm/machine.h"
#include <iostream>
using mahoyo::vm::Machine;
using mahoyo::vm::Flow;
struct Program {std::vector<uint8_t> bytes;size_t prefix;};
static Program program(bool korean,bool reference=false){
    std::vector<uint32_t> code{3,0,0x100,3,0,0x100,0x1b};
    std::string a=korean?std::string("$000001\0\0\0",10):std::string("@x$000001\0",10);
    std::string b=korean?std::string("$000002\0\0\0",10):std::string("@x$000002\0",10);
    code[1]=uint32_t(code.size()*4);code[4]=code[1]+uint32_t(a.size());
    Program out;const std::string signature="HunexCompiledScriptVer1.00";out.bytes.assign(signature.begin(),signature.end());out.bytes.push_back(0);
    auto word=[&](uint32_t value){for(unsigned n=0;n<4;n++)out.bytes.push_back(uint8_t(value>>(n*8)));};
    word(reference?22:12);word(reference?1:0);if(reference){std::string child="child.chs";out.bytes.insert(out.bytes.end(),child.begin(),child.end());out.bytes.push_back(0);}word(0);
    for(auto value:code)word(value);out.prefix=out.bytes.size();out.bytes.insert(out.bytes.end(),a.begin(),a.end());out.bytes.insert(out.bytes.end(),b.begin(),b.end());return out;
}
struct Host:mahoyo::vm::Host {
    std::map<std::string,std::vector<uint8_t>> sources;std::vector<std::string> spoken;
    std::vector<uint8_t> loadModule(const std::string& name)override{return sources.at(name);}
    Flow invoke(uint32_t op,Machine& machine)override{if(op!=0x100)throw std::runtime_error("unexpected fixture opcode");spoken.push_back(machine.popString());return Flow::Yield;}
};
static int checks=0;
static void check(bool value){if(!value)throw std::runtime_error("String rebind check "+std::to_string(checks));checks++;}
int main(){try{
    auto original=program(false),korean=program(true);Host host;host.sources["main.chs"]=original.bytes;Machine vm(host);vm.start("main.chs");vm.setVariable(3,77);vm.run();
    auto pc=vm.pc();auto instructions=vm.instructionsExecuted();auto stack=vm.stack();host.sources["main.chs"]=korean.bytes;vm.rebindModuleStrings({{"main.chs",original.prefix}});
    check(vm.pc()==pc);check(vm.instructionsExecuted()==instructions);check(vm.stack()==stack);check(vm.variable(3)==77);check(host.spoken[0]=="@x$000001");
    vm.resume();vm.run();check(host.spoken[1]=="$000002");
    auto saved=vm.saveState();host.sources["main.chs"][original.prefix-4]^=1;
    bool rejected=false;try{vm.rebindModuleStrings({{"main.chs",original.prefix}});}catch(const mahoyo::vm::Error&){rejected=true;}check(rejected);check(vm.saveState()==saved);
    host.sources["main.chs"]=original.bytes;host.sources["main.chs"].push_back(0);rejected=false;try{vm.rebindModuleStrings({{"main.chs",original.prefix}});}catch(const mahoyo::vm::Error&){rejected=true;}check(rejected);check(vm.saveState()==saved);
    auto parent=program(false,true),next=program(true,true);host.sources["main.chs"]=parent.bytes;host.sources["child.chs"]=original.bytes;vm.start("main.chs");vm.run();saved=vm.saveState();
    host.sources["main.chs"]=next.bytes;host.sources["child.chs"]=korean.bytes;host.sources["child.chs"][original.prefix-4]^=1;
    rejected=false;try{vm.rebindModuleStrings({{"main.chs",parent.prefix},{"child.chs",original.prefix}});}catch(const mahoyo::vm::Error&){rejected=true;}check(rejected);check(vm.saveState()==saved);
    std::cout<<checks<<" VM locale rebind checks: PC/variables/stack and all-module atomic rejection passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
