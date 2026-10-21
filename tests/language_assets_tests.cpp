#include "../runtime/language_text.h"
#include "language_assets.h"
#include "runtime_api.h"
#include <chrono>
#include <iostream>
#include <set>
#include <vector>
namespace fs=std::filesystem;
static int checks=0;
static void check(bool good){++checks;if(!good)throw std::runtime_error("Language check "+std::to_string(checks));}
static void write(const fs::path& path,const std::string& value){fs::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);f<<value;check(bool(f));}
static std::string program(bool image=false){
    std::vector<uint32_t> code;
    auto push=[&](uint32_t value){code.push_back(0);code.push_back(value);};
    std::vector<std::pair<size_t,std::string>> strings;
    auto string=[&](const std::string& value){code.push_back(3);strings.push_back({code.size(),value});code.push_back(0);};
    push(0);code.push_back(0x153);
    if(image){for(int value:{0,0,0,0,0,0,-1,0,0})push(value);string("CC");for(int value:{0,0,255,1000,50,50})push(value);string("fake");push(0);code.push_back(0x240);}
    for(int n:{1,1,0,0})push(n);
    code.push_back(3);size_t ptr=code.size();code.push_back(0);code.push_back(0x140);code.push_back(0x1b);
    size_t suffix=0;for(const auto& item:strings){code[item.first]=uint32_t(code.size()*4+suffix);suffix+=item.second.size()+1;}
    code[ptr]=uint32_t(code.size()*4+suffix);
    std::string out="HunexCompiledScriptVer1.00";out+='\0';
    auto word=[&](uint32_t value){for(unsigned i=0;i<4;i++)out+=char(value>>(8*i));};
    word(12);word(0);word(0);for(auto value:code)word(value);for(const auto& item:strings){out+=item.second;out+='\0';}out+="$000000";out+='\0';return out;
}
int main(){
    const auto owned=fs::temp_directory_path()/("mahoyo-languages-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(fs::create_directory(owned));const auto root=owned/"game";
    try{
        {
            mahoyo::language::TextState source{"vanilla","ja",{{"","$000000"},{"\n","$000001"}},{{"","$000000"},{"legacy literal", ""}}};
            std::ostringstream out;mahoyo::language::writeTextState(out,source);std::istringstream input(out.str());
            auto saved=mahoyo::language::readTextState(input,2);check(saved.edition=="vanilla"&&saved.language=="ja");
            check(mahoyo::language::renderSources(saved.paragraph,{"first","second"})=="first\nsecond");check(saved.history[1].prefix=="legacy literal");
            bool rejected=false;std::istringstream bad("LANGUAGETEXT1 vanilla ja 0 90000");try{mahoyo::language::readTextState(bad,2);}catch(const std::exception&){rejected=true;}check(rejected);
            mahoyo::text::Reveal reveal;reveal.speed(10,0);reveal.reset("abcdefghij",0);reveal.tick(500);reveal.remap("abcdefghijklmnopqrst",500);check(reveal.visible()==10);reveal.finish();reveal.remap("short",500);check(reveal.visible()==5&&!reveal.active());
        }
        write(root/"text/script_text_ja.ctd","JAPANESE\n");write(root/"text/script_text_en.ctd","ENGLISH\n");
        write(root/"text/script_text_zc.ctd","SIMPLIFIED\n");write(root/"text/script_text_zt.ctd","TRADITIONAL\n");
        write(root/"scripts/1_0.chs",program());
        for(const auto& suffix:{"ja","en","zc","zt"})write(root/(std::string("images/title_titlelogo_")+suffix+".cbg.png"),"fixture");
        for(auto pair:std::vector<std::pair<std::string,std::string>>{{"ja","JAPANESE"},{"en","ENGLISH"},{"zh-Hans","SIMPLIFIED"},{"zh-Hant","TRADITIONAL"}}){
            auto handle=woh_create_configured(root.string().c_str(),"vanilla",pair.first.c_str());
            auto state=std::string(woh_snapshot(handle));check(state.find("\"language\":\""+pair.first+"\"")!=std::string::npos);
            check(state.find("title_titlelogo_"+mahoyo::language::suffix(pair.first)+".cbg")!=std::string::npos);
            state=woh_command(handle,"action","start");check(state.find("\"error\":\"\"")!=std::string::npos);check(state.find(pair.second)!=std::string::npos);woh_destroy(handle);
        }
        {
            auto handle=woh_create_configured(root.string().c_str(),"vanilla","ja");woh_command(handle,"textSpeed","0");
            auto before=std::string(woh_command(handle,"action","start"));auto after=std::string(woh_command(handle,"language","en"));
            check(after.find("ENGLISH")!=std::string::npos);check(after.find("JAPANESE")==std::string::npos);
            auto field=[](const std::string& json,const std::string& key){auto at=json.find("\""+key+"\":");check(at!=std::string::npos);return json.substr(at,json.find(',',at)-at);};
            for(const auto& key:{"instructions","modulePc","visibleCharacters","textLine"})if(std::string(key)!="visibleCharacters")check(field(before,key)==field(after,key));
            check(after.find("\"revealing\":false")!=std::string::npos);
            auto saved=std::string(woh_command(handle,"save","2"));check(saved.find("\"notice\":\"Saved\"")!=std::string::npos);
            woh_command(handle,"language","zh-Hant");auto loaded=std::string(woh_command(handle,"load","2"));
            check(loaded.find("TRADITIONAL")!=std::string::npos);check(loaded.find("\"language\":\"zh-Hant\"")!=std::string::npos);
            woh_destroy(handle);
        }
        write(root/"meta/game-edition.txt","WOHEDITION1\nkorean\n");
        write(root/"meta/language-assets.tsv","WOHLANGASSETS1\nko\tscript_text_en.ctd\ttext/ko/script_text_en.ctd\t0\t0\nko\t1_0.chs\tscripts/ko/1_0.chs\t0\t0\nko\ttitle_titlelogo_en.cbg\timages/ko/title_titlelogo_en.cbg.png\t952\t303\n");
        write(root/"text/ko/script_text_en.ctd","KOREAN\n");write(root/"scripts/ko/1_0.chs",program());
        auto handle=woh_create_configured(root.string().c_str(),"korean","ko");auto state=std::string(woh_command(handle,"action","start"));check(state.find("KOREAN")!=std::string::npos);woh_destroy(handle);
        handle=woh_create_configured(root.string().c_str(),"korean","ja");state=woh_command(handle,"action","start");check(state.find("JAPANESE")!=std::string::npos);woh_destroy(handle);
        handle=woh_create_configured(root.string().c_str(),"vanilla","en");state=woh_snapshot(handle);check(state.find("edition mismatch")!=std::string::npos);woh_destroy(handle);
        mahoyo::language::Assets assets;assets.open(root,"korean","ko");check(assets.tablePath()=="text/ko/script_text_en.ctd");
        auto exists=[](const std::string&){return true;};check(assets.localize("title_menu_ja.cbg",exists)=="title_menu_en.cbg");assets.select("zh-Hant");check(assets.localize("title_menu_ja.cbg",exists)=="title_menu_zt.cbg");
        check(assets.localize("img0100.mzp",exists)=="img0100.mzp");check(assets.path("img0100.mzp")=="images/img0100.mzp.png");
        bool rejected=false;try{assets.select("en");}catch(const std::exception&){rejected=true;}check(rejected);
        {
            write(root/"meta/language-assets.tsv","WOHLANGASSETS1\nko script_text_en.ctd text/ko/script_text_en.ctd 0 0\nko fake.mzp images/ko/fake.mzp.png 200 120\n");
            write(root/"meta/image-dimensions.tsv","WOH_IMAGE_DIMENSIONS1\nfake.mzp 100 80\n");
            write(root/"images/fake.mzp.png","fixture");write(root/"images/ko/fake.mzp.png","fixture");write(root/"scripts/1_0.chs",program(true));
            auto probe=woh_create_configured(root.string().c_str(),"korean","ja");woh_command(probe,"textSpeed","0");
            auto before=std::string(woh_command(probe,"action","start"));
            auto value=[](const std::string& json,const std::string& key){auto at=json.find("\""+key+"\":");if(at==std::string::npos)throw std::runtime_error("Missing geometry field");return std::stod(json.substr(at+key.size()+3));};
            check(before.find("\"error\":\"\"")!=std::string::npos);check(value(before,"w")==100&&value(before,"h")==80);
            auto korean=std::string(woh_command(probe,"language","ko"));
            if(korean.find("\"error\":\"\"")==std::string::npos)throw std::runtime_error(korean);
            if(value(korean,"w")!=200||value(korean,"h")!=120)throw std::runtime_error("Live language geometry was not rebound");
            check(true);
            auto anchor=[&](const std::string& json,int width,int height){return std::array<double,2>{value(json,"x")+(value(json,"w")-width)*value(json,"pivotX"),value(json,"y")+(value(json,"h")-height)*value(json,"pivotY")};};
            check(anchor(before,100,80)==anchor(korean,200,120));
            woh_command(probe,"save","7");
            auto japanese=std::string(woh_command(probe,"language","ja"));check(value(japanese,"w")==100&&value(japanese,"h")==80);
            auto loaded=std::string(woh_command(probe,"load","7"));check(loaded.find("\"error\":\"\"")!=std::string::npos);check(value(loaded,"w")==100&&value(loaded,"h")==80);
            woh_destroy(probe);
        }
        write(root/"meta/language-assets.tsv","WOHLANGASSETS1\nko script_text_en.ctd text/ko/script_text_en.ctd 0 0\nko face.mzp images/ko/face.mzp.composite.png 758 1272\n");
        assets.open(root,"korean","ko");check(assets.path("face.mzp")=="images/ko/face.mzp.composite.png");check(assets.overlay("face.mzp")->size[0]==758);
        assets.select("ja");check(assets.path("face.mzp")=="images/face.mzp.png");
        for(const std::string& bad:{"ko\t../escape.cbg\timages/ko/escape.cbg.png\t1\t1", "ko\tx.cbg\timages/ja/x.cbg.png\t1\t1", "ko\tx.cbg\timages/ko/y.cbg.png\t1\t1", "ko\tx.cbg\timages/ko/x.cbg.png\t0\t1", "ko x.mzp images/ko/y.mzp.composite.png 1 1", "ko x.cbg images/ko/x.cbg.composite.png 1 1", "ko x.mzp images/ko/x.mzp.composite.png 0 0"}){
            write(root/"meta/language-assets.tsv","WOHLANGASSETS1\n"+bad+"\n");rejected=false;try{assets.open(root,"korean","ko");}catch(const std::exception&){rejected=true;}check(rejected);
        }
        // Only remove this newly allocated fixture, after checking its absolute ownership boundary.
        auto target=fs::weakly_canonical(owned);check(target.parent_path()==fs::weakly_canonical(fs::temp_directory_path())&&target.filename().string().rfind("mahoyo-languages-",0)==0&&!fs::is_symlink(owned));
        std::error_code ignored;fs::remove_all(target,ignored);
        std::cout<<checks<<" native language resource and actual Runtime API checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"; fixture retained at "<<root<<'\n';return 1;}
}
