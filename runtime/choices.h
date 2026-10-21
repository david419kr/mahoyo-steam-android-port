#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mahoyo::choices {
struct Request {int kind=2,index=0,arg2=2,arg3=0;};
struct Item {int32_t value=0;std::string raw;};
struct Layout {int slot=0;std::string mono,color;int x=0,y=0,width=0,height=0,priority=0,enabled=1,hitMode=1,anchorX=0,anchorY=0,sourceWidth=0,sourceHeight=0;};
enum class Profile { Text161, Wik164 };
struct Option {int ordinal=0;int32_t result=0;std::string label;bool enabled=true;std::optional<Layout> layout;};
struct State {uint64_t revision=0;Profile profile=Profile::Text161;Request request;bool ready=false;std::vector<Option> options;};
struct Completion {int kind,index;int32_t value;uint64_t revision;};

//Pure pending choice state. No VM, VFS, UI clocks or persistent save fields.
class Controller {
public:
    void append(int32_t value,std::string raw);
    const std::vector<Item>& queued()const{return queue_;}
    const std::optional<State>& state()const{return pending_;}
    bool active()const{return pending_.has_value();}
    bool safe()const{return queue_.empty()&&!pending_;}
    static void validateRequest(const Request& request);
    void beginText(const Request& request,const std::vector<std::string>& labels);
    void beginWik(const Request& request,const std::vector<Layout>& layout);
    void prepared();
    std::optional<Completion> select(const std::string& revision,int ordinal,bool inputAllowed)const;
    //Call only after the destination write succeeds, before resuming the VM.
    void complete(uint64_t revision);
    //Scene discard, not a Native normal -1 completion or user cancel.
    void reset()noexcept;
private:
    std::vector<Item> queue_;
    std::optional<State> pending_;
};
std::vector<Layout> parseWikLayout(const std::string& text);
bool validUtf8(const std::string& text);
} //namespace mahoyo::choices
