#ifndef SCRIPT_STRUCTS
#define SCRIPT_STRUCTS

# include "../lib/placed.hpp"

# include <cstring>

# include <vector>
# include <string>

// WARNING: most of this header is basically internal information.

// this is the only truly public class
struct ArgRec
{
    const char *name;
    union
    {
        int32_t i;
        const char *s;
    };

    ArgRec(const char *n, int32_t v) : name(n), i(v) {}
    ArgRec(const char *n, const char *v) : name(n), s(v) {}
};

// enum used for both code and value types
enum class Script : uint8_t
{
    NOP = 0,

    /// Types
    // an index into the current script, i.e. a label
    POS = 1,
    INT = 2,
    PARAM = 3,
    FUNC = 4,
    // a dynamically allocated string
    STR = 5,
    // a string literal
    UNUSED_CONSTSTR = 6,
    ARG = 7,
    NAME = 8,
    EOL = 9,
    RETINFO = 10,

    /// Operators
    // ||
    LOR = 11,
    // &&
    LAND = 12,
    // <=
    LE = 13,
    // <
    LT = 14,
    // >=
    GE = 15,
    // >
    GT = 16,
    // ==
    EQ = 17,
    // !=
    NE = 18,
    // ^
    XOR = 19,
    // |
    OR = 20,
    // &
    AND = 21,
    // +
    ADD = 22,
    // -
    SUB = 23,
    // *
    MUL = 24,
    // /
    DIV = 25,
    // %
    MOD = 26,
    // -
    NEG = 27,
    // !
    LNOT = 28,
    // ~
    NOT = 29,
    // >>
    SH_R = 30,
    // <<
    SH_L = 31,

    // not a values, just a bound
    COUNT = 32,
};

// contains a typedef corresponding to something in script_data' union
template<Script Type>
struct ScriptStorageType;
#define SCRIPT_TYPE(TYPE, native_type)  \
template<>                              \
struct ScriptStorageType<Script::TYPE>  \
{                                       \
    typedef native_type type;           \
}

// these are the only types that ever occur on the stack
// Script::PARAM is always a constant, and appears on the stack as Script::NAME
SCRIPT_TYPE(INT, int32_t);
SCRIPT_TYPE(POS, int32_t);
SCRIPT_TYPE(NAME, int32_t);
SCRIPT_TYPE(ARG, int32_t);
SCRIPT_TYPE(RETINFO, const Script *);
SCRIPT_TYPE(STR, std::string);
#undef SCRIPT_TYPE

// not const-correct - though, you should never have a const one of these
struct script_data
{
private:
    Script type;
    AnyPlaced<int32_t, std::string, const Script *> impl;
    script_data& operator =(const script_data&) = delete;
public:
    Script get_type() { return type; }
    script_data(const script_data&) = delete;
    script_data() : type(Script::NOP) {}
    ~script_data()
    {
        if (type == Script::STR)
            impl.destroy<std::string>();
    }
    script_data(script_data&& rhs) : type(rhs.type)
    {
        if (type == Script::STR)
        {
            impl.construct<std::string>(std::move(rhs.impl.ref<std::string>()));
            // the following would get called anyway when rhs is destructed...
            rhs.impl.destroy<std::string>();
            type = Script::NOP;
        }
        else
        {
            memcpy(&impl, &rhs.impl, sizeof(impl));
        }
    }
    script_data& operator =(script_data&& rhs)
    {
        // needed due to a bug in libstdc++
        // (though if we use insert() or erase() we need it anyway)
        if (type == Script::STR)
            impl.destroy<std::string>();
        type = rhs.type;
        if (type == Script::STR)
        {
            impl.construct<std::string>(std::move(rhs.impl.ref<std::string>()));
            // the following would get called anyway when rhs is destructed...
            rhs.impl.destroy<std::string>();
            type = Script::NOP;
        }
        else
        {
            memcpy(&impl, &rhs.impl, sizeof(impl));
        }
        return *this;
    }
    template<Script Type>
    void set(typename ScriptStorageType<Type>::type val)
    {
        switch(type)
        {
        case Script::STR:
            impl.destroy<std::string>();
            break;
        }
        type = Type;
        impl.construct<typename ScriptStorageType<Type>::type>(val);
    }
    template<Script Type>
    typename ScriptStorageType<Type>::type get()
    {
        return impl.ref<typename ScriptStorageType<Type>::type>();
    }
    friend class ScriptState;
    // friend void ScriptState::push_copy_of(size_t);
};

/// Run state
enum class ScriptExecutionState : uint8_t
{
    ZERO,
    STOP,
    END,
    RERUNLINE,
    GOTO,
    RETFUNC,
};


class ScriptState
{
    // in the old struct, sp was size(), sp_max was capacity()
    std::vector<script_data> stack;
public:
    void pop(int32_t start, int32_t end);
    template<Script Type>
    void push(typename ScriptStorageType<Type>::type value);
    template<Script Type>
    typename ScriptStorageType<Type>::type get_as(size_t idx);
    void push_copy_of(size_t idx);
    void resolve(size_t idx);
    Script type_at(size_t idx) __attribute__((pure));
    script_data& raw_at(size_t idx) __attribute__((pure, deprecated("Use what you need, or become a friend")));
    bool has_at(size_t idx) __attribute__((pure));
    std::string to_string(size_t idx);
    int32_t to_int(size_t idx);
    template<Script Op>
    void op();
    void run_func();
    friend void run_script_main(const Script *script, int32_t pos,
                                ScriptState *st, const Script *rootscript);
    friend int32_t run_script_l(const std::vector<Script>& script, int32_t pos,
                                int32_t rid, int32_t oid,
                                int32_t args_nr, ArgRec *args);

    // arguments
    int32_t start, end;
    // script offset
    int32_t pos;
    // used for stuff like GOTO and END
    ScriptExecutionState state;
    // beings
    int32_t rid, oid;
    // what is actually executed
    const Script *script;
    // something to do with function calls
    int32_t defsp, new_pos, new_defsp;
};

#define INSTANTIATE_LATER(Type) \
extern template typename ScriptStorageType<Script::Type>::type ScriptState::get_as<Script::Type>(size_t idx);   \
extern template void ScriptState::push<Script::Type>(typename ScriptStorageType<Script::Type>::type value)
INSTANTIATE_LATER(INT);
INSTANTIATE_LATER(POS);
INSTANTIATE_LATER(NAME);
INSTANTIATE_LATER(ARG);
INSTANTIATE_LATER(STR);
INSTANTIATE_LATER(RETINFO);
#undef INSTANTIATE_LATER

struct builtin_function_t
{
    void (*func)(ScriptState *);
    const char *name;
    const char *arg;
};

enum class SP : uint16_t;
/// Data about a word
// fields used, by type:
// all: int32_t next
// all: std::string str;
// Script::INT: int32_t val
// Script::PARAM: SP val;
// Script::NOP: int32_t backpatch
// Script::POS: int32_t label, unset int32_t backpatch
// Script::FUNC: int32_t val is the index, deprecated but needed for the args
// Script::FUNC: fptr func is the actually implementation
// cleanup: Script::{POS,NAME->NOP} clears backpatch, label
// Script::NAME uses label (why?), unsets backpatch
struct str_data_t
{
    Script type;
    std::string str;
    // this acts as a linked list of uses of a label
    int32_t backpatch;
    // this is the offset within the script where the label points
    // (only if type == Script::POS)
    int32_t label;
    // command or function to invoke (only if type == Script::FUNC)
    void (*func)(ScriptState *);
    int32_t val;
    SP param;
    // linked list of words in str_data with the same hash
    uint32_t next;
};

extern std::vector<str_data_t> str_data;

#endif //SCRIPT_STRUCTS
