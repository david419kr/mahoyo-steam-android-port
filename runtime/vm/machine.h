#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace mahoyo::vm {
enum class Flow { Continue, Yield, Halt };
class Machine;
class Host {
public:
    virtual ~Host() = default;
    virtual std::vector<uint8_t> loadModule(const std::string& name) = 0;
    // The opcode word has already been consumed. Arguments remain on the stack.
    // A yielding call must consume its arguments exactly once; resume continues
    // at the next opcode, never invokes the same command again.
    virtual Flow invoke(uint32_t opcode, Machine& machine) = 0;
    virtual void trace(const Machine&, uint32_t) {}
    virtual void diagnostic(const std::string&) {}
    virtual void variableChanged(size_t, int32_t, bool) {}
};

class Error : public std::runtime_error { using std::runtime_error::runtime_error; };
class Machine {
public:
    explicit Machine(Host& host);
    void start(const std::string& module, uint32_t entryOffset = 0);
    Flow run(size_t instructionBudget = 20000);
    void resume();
    int32_t pop();
    void push(int32_t value);
    int32_t peek(size_t depth = 0) const;
    std::string popString();
    std::string stringAt(uint32_t address) const;
    uint32_t readOperand();
    int32_t variable(size_t index, bool global = false) const;
    void setVariable(size_t index, int32_t value, bool global = false);
    bool flag(size_t index, bool global = false) const;
    void setFlag(size_t index, bool value, bool global = false);
    const std::vector<int32_t>& sharedVariables() const { return globals_; }
    const std::vector<uint8_t>& sharedFlags() const { return globalFlags_; }
    //Replace only the shared manager; local VM/PC/stack remain unchanged.
    //Validation is atomic and accepts the native smaller shared-array layout.
    void restoreShared(const std::vector<int32_t>& variables,const std::vector<uint8_t>& flags);
    const std::vector<int32_t>& stack() const { return stack_; }
    uint32_t pc() const { return pc_; }
    uint32_t modulePc() const;
    const std::string& moduleName() const;
    uint32_t sourceLine() const { return sourceLine_; }
    const std::string& debugMessage() const { return debugMessage_; }
    Flow flow() const { return flow_; }
    uint64_t instructionsExecuted() const { return instructions_; }
    void jump(uint32_t address);
    void validateExternalTarget(uint32_t address) const;
    uint32_t externalJump(uint32_t address);
    void callExport(const std::string& name);
    std::vector<uint8_t> saveState() const;
    void loadState(const std::vector<uint8_t>& bytes);
    // Locale variants may replace pooled literals only; all code/exports and addresses stay fixed.
    void rebindModuleStrings(const std::map<std::string,size_t>& immutablePrefixes);

private:
    struct Module { std::string name; uint32_t begin, end, code; };
    struct Frame { uint32_t pc, module, localTop; bool unload; size_t moduleCount; };
    struct Scope { size_t stackSize; uint32_t localTop; };
    struct Export { uint32_t module, offset; };
    Host& host_;
    std::vector<uint8_t> arena_, locals_, strings_, scratch_;
    std::vector<int32_t> stack_, stackCells_, variables_, globals_;
    std::vector<uint8_t> flags_, globalFlags_;
    std::vector<Module> modules_;
    std::vector<Frame> frames_;
    std::vector<Scope> scopes_;
    std::map<std::string, Export> exports_;
    uint32_t pc_ = 0, currentModule_ = 0, localTop_ = 0, sourceLine_ = 0;
    uint64_t instructions_ = 0;
    int32_t returnValue_ = 0;
    bool returnValueValid_ = false;
    std::string functionName_, debugMessage_;
    Flow flow_ = Flow::Halt;
    uint32_t load(const std::string& name, bool references);
    Flow execute(uint32_t opcode);
    void returnFromCall();
    void pushFrame(bool unload);
    std::vector<uint8_t>& memory(uint32_t address, size_t bytes, size_t& offset);
    const std::vector<uint8_t>& memory(uint32_t address, size_t bytes, size_t& offset) const;
    uint32_t readMemory(uint32_t address, unsigned width) const;
    void writeMemory(uint32_t address, uint32_t value, unsigned width);
    static uint32_t word(const std::vector<uint8_t>& bytes, size_t at);
};
} // namespace mahoyo::vm
