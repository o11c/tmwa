#include "script.hpp"

#include <cmath>

#include "../common/db.hpp"
#include "../common/lock.hpp"
#include "../common/utils.hpp"
#include "../common/socket.hpp"
#include "../common/timer.hpp"

#include "map.hpp"
#include "npc.hpp"
#include "pc.hpp"

// indices into str_data
constexpr uint32_t LABEL_NEXTLINE = 1;
constexpr uint32_t WORD_START = 2;

std::vector<Script> script_buf;

// the first couple of entries are special
// str_data[0] is unused to allow checks against 0
// str_data[1 = LABEL_NEXTLINE] is the special label '-'
std::vector<str_data_t> str_data(WORD_START);

// indices into str_data of the first word of the appropriate hash value
int32_t str_hash[256];

static DMap<int32_t, int32_t> mapreg_db;
static DMap<int32_t, std::string> mapregstr_db;
// The OLD way was:
// -1: not initialized
// 0: saved
// 1: dirty
// the only time not initialized is not handled is in do_final_script
// otherwise, not initialized is treated as dirty
// The NEW way is:
// false: not initialized, or saved (this should be safe)
// true: needs saved
static bool mapreg_dirty = false;
const char mapreg_txt[] = "save/mapreg.txt";
#define MAPREG_AUTOSAVE_INTERVAL        (10*1000)

static struct dbt *scriptlabel_db = NULL;
static struct dbt *userfunc_db = NULL;

struct dbt *script_get_label_db(void)
{
    return scriptlabel_db;
}

struct dbt *script_get_userfunc_db(void)
{
    if (!userfunc_db)
        userfunc_db = strdb_init();
    return userfunc_db;
}

static int32_t parse_cmd_if = 0;
static int32_t parse_cmd;

static uint8_t calc_hash2(const uint8_t *p, const uint8_t *e)
{
    int32_t h = 0;
    while (p != e)
    {
        h = (h << 1) + (h >> 3) + (h >> 5) + (h >> 8);
        h += *p++;
    }
    return h & 0xFF;
}
static uint8_t calc_hash(const uint8_t *p) __attribute__((pure));
static uint8_t calc_hash(const uint8_t *p)
{
    int32_t h = 0;
    while (*p)
    {
        h = (h << 1) + (h >> 3) + (h >> 5) + (h >> 8);
        h += *p++;
    }
    return h & 0xFF;
}

// Search for the index within str_data, or -1
static int32_t search_str(const char *p) __attribute__((pure));
static int32_t search_str(const char *p)
{
    int32_t i = str_hash[calc_hash(sign_cast<const uint8_t *>(p))];
    while (i)
    {
        if (str_data[i].str == p)
        {
            return i;
        }
        i = str_data[i].next;
    }
    return -1;
}

// Register a name in str_data
int32_t add_str(const char *p, size_t len)
{
    int32_t i = calc_hash2(sign_cast<const uint8_t *>(p),
                           sign_cast<const uint8_t *>(p + len));
    if (str_hash[i] == 0)
    {
        // if there was previously no index for the hash,
        // it will become the index of what we add
        str_hash[i] = str_data.size();
    }
    else
    {
        i = str_hash[i];
        for (;;)
        {
            const std::string& str = str_data[i].str;
            if (str.size() == len && strncmp(str.data(), p, len) == 0)
                return i;
            if (!str_data[i].next)
                break;
            i = str_data[i].next;
        }
        // the next will be the one we add
        str_data[i].next = str_data.size();
    }

    str_data_t new_str_data;
    new_str_data.type = Script::NOP;
    new_str_data.str = std::string(p, len);
    new_str_data.next = 0;
    new_str_data.func = NULL;
    new_str_data.backpatch = -1;
    new_str_data.label = -1;

    // right now str_data_t is trivial, but some day I may be glad of this std::move
    str_data.push_back(std::move(new_str_data));
    return str_data.size() - 1;
}

/// write a byte directly into the script buffer
static void add_scriptb(uint8_t a)
{
    script_buf.push_back(static_cast<Script>(a));
}

/// always called with a value from the enum
static void add_scriptc(Script a)
{
    if (a >= Script::COUNT)
        // there used to be a strange loop similar to add_scripti while >= 0x40
        // but all callers are only in range
        abort();
    add_scriptb(static_cast<uint8_t>(a));
}

/// encode an integer
// just *try* to figure out how get_num does the opposite of this
static void add_scripti(uint32_t a)
{
    while (a >= 0x40)
    {
        // add the lower 6 bits
        add_scriptb(a | 0xc0);
        //was: a = (a - 0x40) >> 6;
        a = (a >> 6) - 1;
    }
    add_scriptb(a | 0x80);
}

/// append a function or variable name or a label
static void add_scriptl(int32_t l)
{
    int32_t backpatch = str_data[l].backpatch;

    switch (str_data[l].type)
    {
    case Script::POS:
        add_scriptc(Script::POS);
        add_scriptb(str_data[l].label);
        add_scriptb(str_data[l].label >> 8);
        add_scriptb(str_data[l].label >> 16);
        break;
    case Script::NOP:
        add_scriptc(Script::NAME);
        // this forms a linked list of backpatches
        // For labels: during label resolution,
        // Script::NAME will get overwritten with Script::POS
        str_data[l].backpatch = script_buf.size();
        add_scriptb(backpatch);
        add_scriptb(backpatch >> 8);
        add_scriptb(backpatch >> 16);
        break;
    case Script::INT:
        // type 0 from db/const.txt
        add_scripti(str_data[l].val);
        break;
    default:
        add_scriptc(Script::NAME);
        add_scriptb(l);
        add_scriptb(l >> 8);
        add_scriptb(l >> 16);
        break;
    }
}

/// Resolve ("backpatch") labels
// this is called as a label is encountered
// it is also called every time a line ends, for the special '-' label
static void set_label(int32_t l)
{
    size_t pos = script_buf.size();
    str_data[l].type = Script::POS;
    str_data[l].label = pos;
    for (int32_t i = str_data[l].backpatch; i >= 0 && i != 0x00ffffff;)
    {
        uint8_t next0 = static_cast<uint8_t>(script_buf[i]);
        uint8_t next1 = static_cast<uint8_t>(script_buf[i + 1]);
        uint8_t next2 = static_cast<uint8_t>(script_buf[i + 2]);
        int32_t next = (next2 << 16) | (next1 << 8) | (next0);
        script_buf[i - 1] = Script::POS;
        script_buf[i] = static_cast<Script>(pos);
        script_buf[i + 1] = static_cast<Script>(pos >> 8);
        script_buf[i + 2] = static_cast<Script>(pos >> 16);
        i = next;
    }
}

/// Skip whitespace and comments
static const char *skip_space(const char *p) __attribute__((pure));
static const char *skip_space(const char *p)
{
    while (1)
    {
        while (isspace(*p))
            p++;
        if (p[0] == '/' && p[1] == '/')
        {
            while (*p && *p != '\n')
                p++;
        }
        else if (p[0] == '/' && p[1] == '*')
        {
            // hm, does this mean /*/ is a valid comment?
            p++;
            while (*p && (p[-1] != '*' || p[0] != '/'))
                p++;
            if (*p)
                p++;
        }
        else
            break;
    }
    return p;
}

/// Skip a word, such a variable name, label name, or function name
static const char *skip_word(const char *p) __attribute__((pure));
static const char *skip_word(const char *p)
{
    // prefix
    if (*p == '$')
        // global variables (stored in mapreg.txt)
        p++;
    if (*p == '@')
        // temporary variables (not stored)
        p++;
    if (*p == '#')
        // account variables (stored in the char server)
        p++;
    if (*p == '#')
        // global account variables (stored in the login server)
        p++;

    while (isalnum(*p) || *p == '_')
        p++;

    // postfix
    if (*p == '$')
        // string indicator
        p++;

    return p;
}

static struct
{
    /// The beginning of this script
    const char *ptr;
    /// The line within the file that this script begins
    int32_t line;
    /// Source file
    std::string file;
} start;

/// Display a parser error
// starts with: file:line: message: and 'h'ighlights the exact error character
static void disp_error_message(const char *mes, const char *pos)
{
    int32_t line;
    const char *p;

    for (line = start.line, p = start.ptr; p && *p; line++)
    {
        const char *linestart = p;
        const char *lineend = strchr(p, '\n');
        if (lineend == NULL || pos < lineend)
        {
            fprintf(stderr, "%s:%d: %s: ", start.file.c_str(), line, mes);
            fwrite(linestart, 1, pos - linestart, stderr);
            fprintf(stderr, "\'%c\'", *pos);
            fwrite(pos + 1, 1, strchrnul(pos + 1, '\n') - (pos + 1), stderr);
            fprintf(stderr, "\a\n");
            return;
        }
        p = lineend + 1;
    }
}

/// These 2 functions implement the parser
// A "simple" expression is:
//  * Something in parentheses, which is a subexpr
//  * An arrayname[subexpr] expression
//  * A (positive) integer or string literal
//  * A label, function, or variable name
// A "sub" expression may contain weighted operators
// the argument of the function is the highest weight you want to reject
// in order to do it from left to right, it can't be equal
enum class ExprWeight : uint8_t
{
    ALL,

    LOR,
    LAND,
    // C was stupid and put comparison tighter than bitwise operators
    COMP,
    XOR,
    // C reverses OR and XOR
    OR,
    AND,
    // C: ==, !=
    // C: < <= => >
    SHFT,
    ADD,
    MULT,
    FUNC,
};

static const char *parse_subexpr(const char *, ExprWeight);
static const char *parse_simpleexpr(const char *p)
{
    int32_t i;
    p = skip_space(p);

    if (*p == ';' || *p == ',')
    {
        disp_error_message("unexpected expr end", p);
        exit(1);
    }
    if (*p == '(')
    {

        p = parse_subexpr(p + 1, ExprWeight::ALL);
        p = skip_space(p);
        if ((*p++) != ')')
        {
            disp_error_message("missing ')'", p);
            exit(1);
        }
    }
    else if (isdigit(*p) || ((*p == '-' || *p == '+') && isdigit(p[1])))
    {
        char *np;
        i = strtoul(p, &np, 0);
        add_scripti(i);
        p = np;
    }
    else if (*p == '"')
    {
        add_scriptc(Script::STR);
        p++;
        while (*p && *p != '"')
        {
            if (*p == '\\')
            {
                p++;
                // TODO properly handle escapes other than \\ and \"
            }
            if (*p == '\n')
            {
                disp_error_message("unexpected newline @ string", p);
                exit(1);
            }
            add_scriptb(*p++);
        }
        if (!*p)
        {
            disp_error_message("unexpected eof @ string", p);
            exit(1);
        }
        add_scriptb('\0');
        // closing quote
        p++;
    }
    else
    {
        // label, variable, function, ...
        const char *p2 = skip_word(p);
        if (p2 == p)
        {
            disp_error_message("unexpected character", p);
            exit(1);
        }
        int32_t l = add_str(p, p2 - p);

        // needed for some sort of warning
        parse_cmd = l;
        if (l == search_str("if"))
            // somehow disables the warning for no comma after the condition
            parse_cmd_if++;
        p = p2;

        if (str_data[l].type != Script::FUNC && *p == '[')
        {
            // name[i] => getelementofarray(name,i)
            add_scriptl(search_str("getelementofarray"));
            add_scriptc(Script::ARG);
            add_scriptl(l);
            p = parse_subexpr(p + 1, ExprWeight::ALL);
            p = skip_space(p);
            if ((*p++) != ']')
            {
                disp_error_message("expected ']'", p);
                exit(1);
            }
            add_scriptc(Script::FUNC);
        }
        else
            add_scriptl(l);

    }

    return p;
}

/// Do that fancy infix stuff
const char *parse_subexpr(const char *p, ExprWeight limit)
{
    p = skip_space(p);

    if (*p == '-')
    {
        const char *tmpp = skip_space(p + 1);
        if (*tmpp == ';' || *tmpp == ',')
        {
            add_scriptl(LABEL_NEXTLINE);
            p++;
            return p;
        }
    }
    const char *tmpp = p;
    Script op;
    if ((op = Script::NEG, *p == '-') ||
        (op = Script::LNOT, *p == '!') ||
        (op = Script::NOT, *p == '~') ||
        (op = Script::NOP, *p == '+'))
    {
        // push the expression following, which MUST be a simple expression
        // or a function call, no higher operators allowed
        p = parse_subexpr(p + 1, static_cast<ExprWeight>(static_cast<uint8_t>(ExprWeight::FUNC) - 1));
        if (op != Script::NOP)
            // insert the unary op
            add_scriptc(op);
    }
    else
        p = parse_simpleexpr(p);

    p = skip_space(p);

    ExprWeight opl;
    int32_t len;
    while ((
            // I considered sorting this table, but it's complicated
            // because >> (5) must precede > (2)
            // but || (0) must precede | (4)
            (op = Script::ADD,  opl = ExprWeight::ADD,  len = 1,        *p == '+') ||
            (op = Script::SUB,  opl = ExprWeight::ADD,  len = 1,        *p == '-') ||
            (op = Script::MUL,  opl = ExprWeight::MULT, len = 1,        *p == '*') ||
            (op = Script::DIV,  opl = ExprWeight::MULT, len = 1,        *p == '/') ||
            (op = Script::MOD,  opl = ExprWeight::MULT, len = 1,        *p == '%') ||
            (op = Script::FUNC, opl = ExprWeight::FUNC, len = 1,        *p == '(') ||
            (op = Script::LAND, opl = ExprWeight::LAND, len = 2,        *p == '&' && p[1] == '&') ||
            (op = Script::AND,  opl = ExprWeight::AND,  len = 1,        *p == '&') ||
            (op = Script::LOR,  opl = ExprWeight::LOR,  len = 2,        *p == '|' && p[1] == '|') ||
            (op = Script::OR,   opl = ExprWeight::OR,   len = 1,        *p == '|') ||
            (op = Script::XOR,  opl = ExprWeight::XOR,  len = 1,        *p == '^') ||
            (op = Script::EQ,   opl = ExprWeight::COMP, len = 2,        *p == '=' && p[1] == '=') ||
            (op = Script::NE,   opl = ExprWeight::COMP, len = 2,        *p == '!' && p[1] == '=') ||
            (op = Script::SH_R, opl = ExprWeight::SHFT, len = 2,        *p == '>' && p[1] == '>') ||
            (op = Script::GE,   opl = ExprWeight::COMP, len = 2,        *p == '>' && p[1] == '=') ||
            (op = Script::GT,   opl = ExprWeight::COMP, len = 1,        *p == '>') ||
            (op = Script::SH_L, opl = ExprWeight::SHFT, len = 2,        *p == '<' && p[1] == '<') ||
            (op = Script::LE,   opl = ExprWeight::COMP, len = 2,        *p == '<' && p[1] == '=') ||
            (op = Script::LT,   opl = ExprWeight::COMP, len = 1,        *p == '<')
           ) && opl > limit)
    {
        p += len;
        if (op == Script::FUNC)
        {
            int32_t func = parse_cmd;
            // the parameter list is solely to enable argument checking
            const char *plist[128];

            if (str_data[func].type != Script::FUNC)
            {
                disp_error_message("expect function", tmpp);
                exit(0);
            }

            add_scriptc(Script::ARG);
            int32_t i = 0;
            while (*p && *p != ')' && i < 128)
            {
                plist[i] = p;
                p = parse_subexpr(p, ExprWeight::ALL);
                p = skip_space(p);
                if (*p == ',')
                    p++;
                else if (*p != ')')
                    disp_error_message("expect ',' or ')' after function parameter", p);
                p = skip_space(p);
                i++;
            }
            plist[i] = p;
            if (*(p++) != ')')
            {
                disp_error_message("function call: missing ')'", p);
                exit(1);
            }

            const char *arg = builtin_functions[str_data[func].val].arg;
            int32_t j = 0;
            for (j = 0; arg[j]; j++)
                if (arg[j] == '*')
                    break;
            if ((arg[j] == '\0' && i != j) || (arg[j] == '*' && i < j))
            {
                disp_error_message("illegal number of parameters",
                                   plist[(i < j) ? i : j]);
            }
        }
        else // not op == C_FUNC
        {
            // insert instructions to push the RHS
            p = parse_subexpr(p, opl);
        }
        // push the binary operator
        add_scriptc(op);
        p = skip_space(p);
    }
    // return pointer to first untreated operator
    // which is either one of the terminators ) , ] ;
    // or what is about to get passed to the while loop above
    return p;
}

/// Evaluate a top-level expression
// only used by the "command" evaluator below
static const char *parse_expr(const char *p)
{
    switch (*p)
    {
        case ')':
        case ';':
        case ':':
        case '[':
        case ']':
        case '}':
            disp_error_message("unexpected char", p);
            exit(1);
    }
    p = parse_subexpr(p, ExprWeight::ALL);
    return p;
}

/// Parse a command, ending with a semicolon
static const char *parse_line(const char *p)
{
    p = skip_space(p);
    if (*p == ';')
        return p;

    parse_cmd_if = 0;

    // command name
    const char *p2 = p;
    p = parse_simpleexpr(p);
    p = skip_space(p);

    int32_t cmd = parse_cmd;
    if (str_data[cmd].type != Script::FUNC)
    {
        disp_error_message("expect command", p2);
        exit(0);
    }

    add_scriptc(Script::ARG);
    int32_t i = 0;
    // plist is merely to make nice error messages
    const char *plist[128];
    while (p && *p && *p != ';' && i < 128)
    {
        plist[i] = p;

        p = parse_expr(p);
        p = skip_space(p);
        if (*p == ',')
            p++;
        // parse_cmd_if is incremented every time a command is "if"
        // it is allowed 2 missing commas per nest level:
        // one after the condition, and one after the (sub)command
        else if (*p != ';' && parse_cmd_if * 2 <= i)
            disp_error_message("expect ',' or ';' at cmd params", p);
        p = skip_space(p);
        i++;
    }
    plist[i] = p;
    if (!p || *(p++) != ';')
    {
        disp_error_message("expected ';'", p);
        exit(1);
    }
    add_scriptc(Script::FUNC);

    const char *arg = builtin_functions[str_data[cmd].val].arg;
    int32_t j = 0;
    for (j = 0; arg[j]; j++)
        if (arg[j] == '*')
            break;
    if ((arg[j] == 0 && i != j) || (arg[j] == '*' && i < j))
    {
        disp_error_message("illegal number of parameters",
                           plist[(i < j) ? i : j]);
    }

    return p;
}

/// add builtin functions to the token list
static void add_builtin_functions(void)
{
    for (int32_t i = 0; builtin_functions[i].func; i++)
    {
        int32_t n = add_str(builtin_functions[i].name);
        str_data[n].type = Script::FUNC;
        str_data[n].val = i;
        str_data[n].func = builtin_functions[i].func;
    }
}

/// Read db/const.txt
// each line is of the format:
// TOKEN        int_value       [type]
// type 0 or ommitted: an integer constant
// type 1: a character parameter
static void read_constdb(void)
{
    FILE *fp = fopen_("db/const.txt", "r");
    if (fp == NULL)
    {
        map_log("can't read db/const.txt\n");
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        char name[1024];
        int32_t val;
        int32_t type = 0;
        if (sscanf(line, "%[A-Za-z0-9_],%d,%d", name, &val, &type) >= 2 ||
            sscanf(line, "%[A-Za-z0-9_] %d %d", name, &val, &type) >= 2)
        {
            int32_t n = add_str(name);
            switch (type)
            {
            case 0:
                str_data[n].type = Script::INT;
                str_data[n].val = val;
                break;
            case 1:
                str_data[n].type = Script::PARAM;
                str_data[n].param = static_cast<SP>(val);
                break;
            default:
                map_log("Unknown constant type %d", type);
                abort();
            }
        }
    }
    fclose_(fp);
}

/// Not in do_init_script because do_init_itemdb, which calls us, comes first
void pre_init_script()
{
    add_builtin_functions();
    read_constdb();
}

/// Parse script src
// file and line are merely for prettification
std::vector<Script> parse_script(const std::string& file, const char *src, int32_t line)
{
    // it gets clear()ed because of the std::move at the end
    //script_buf.clear();

    // clear out the special label
    str_data[LABEL_NEXTLINE].type = Script::NOP;
    str_data[LABEL_NEXTLINE].backpatch = -1;
    str_data[LABEL_NEXTLINE].label = -1;
    for (int32_t i = WORD_START; i < str_data.size(); i++)
    {
        // clear labels and variables from previous scripts
        if (str_data[i].type == Script::POS || str_data[i].type == Script::NAME)
        {
            str_data[i].type = Script::NOP;
            str_data[i].backpatch = -1;
            str_data[i].label = -1;
        }
    }

    // clear exported labels from a previous script
    if (scriptlabel_db)
        // not strdb_final, because we don't need to run a function
        // Note: the keys at this time are already freed
        db_final(scriptlabel_db);
    scriptlabel_db = strdb_init();

    // globals for for error message
    start.ptr = src;
    start.line = line;
    start.file = file;

    const char *p = src;
    p = skip_space(p);
    if (*p != '{')
    {
        disp_error_message("expected '{'", p);
        abort();
        return std::vector<Script>();
    }
    for (p++; p && *p && *p != '}';)
    {
        p = skip_space(p);
        const char *p_skipw = skip_word(p);
        // special treatment for if there's a label
        const char *tmpp = skip_space(p_skipw);
        if (*tmpp == ':')
        {
            int32_t l = add_str(p, p_skipw - p);
            if (str_data[l].label != -1)
            {
                disp_error_message("dup label ", p);
                exit(1);
            }
            set_label(l);
            // Yes, we are adding a non-NUL-terminated string as the key to the DB
            // the only use of scriptlabel_db is in npc.cpp, doing a foreach
            // since there is no db_search, and there's a NUL out there *somewhere*
            // (but usually it will stop before the ':'), nothing goes wrong
            strdb_insert(scriptlabel_db, p, script_buf.size());
            p = tmpp + 1;
            continue;
        }

        // Parse a command
        p = parse_line(p);
        p = skip_space(p);
        add_scriptc(Script::EOL);

        // possibly backpatch the special label '-'
        set_label(LABEL_NEXTLINE);
        str_data[LABEL_NEXTLINE].type = Script::NOP;
        str_data[LABEL_NEXTLINE].backpatch = -1;
        str_data[LABEL_NEXTLINE].label = -1;
    }

    // NUL-terminate the script
    add_scriptc(Script::NOP);

    // resolve remaining names ("backpatch"ing)
    // these are the words *NOT* found to be labels, i.e. variables
    for (int32_t i = WORD_START; i < str_data.size(); i++)
    {
        if (str_data[i].type == Script::NOP)
        {
            str_data[i].type = Script::NAME;
            str_data[i].label = i;
            for (int32_t j = str_data[i].backpatch; j >= 0 && j != 0x00ffffff;)
            {
                uint8_t next0 = static_cast<uint8_t>(script_buf[j]);
                uint8_t next1 = static_cast<uint8_t>(script_buf[j + 1]);
                uint8_t next2 = static_cast<uint8_t>(script_buf[j + 2]);
                int32_t next = (next2 << 16) | (next1 << 8) | (next0);
                // script_buf[j - 1] == Script::NAME by default
                script_buf[j] = static_cast<Script>(i);
                script_buf[j + 1] = static_cast<Script>(i >> 8);
                script_buf[j + 2] = static_cast<Script>(i >> 16);
                j = next;
            }
        }
    }
    return std::move(script_buf);
}

/// utility functions

void ScriptState::resolve(size_t i)
{
    script_data& data = stack[i];
    if (data.get_type() != Script::NAME)
        return;

    const int32_t num = data.get<Script::NAME>();

    // look up variables
    const std::string& name = str_data[num & 0x00ffffff].str;
    const char prefix = name.front();
    const char postfix = name.back();

    MapSessionData *sd = NULL;
    if (prefix != '$')
        sd = map_id2sd(rid);

    if (postfix == '$')
        data.set<Script::STR>(get_reg_s(sd, num, name));
    else
        data.set<Script::INT>(get_reg_i(sd, num, name));
}

void set_reg_s(MapSessionData *sd, int32_t num, const std::string& name, const std::string& str)
{
    char prefix = name.front();
    if (prefix == '@')
        sd->regstr.set(num, str);
    else if (prefix == '$')
        mapreg_setregstr(num, str);
    else
        map_log("script: %s: illegal scope string variable !", __func__);
}

void set_reg_i(MapSessionData *sd, int32_t num, const std::string& name, int32_t val)
{
    const char prefix = name.front();

    if (str_data[num & 0x00ffffff].type == Script::PARAM)
        pc_setparam(sd, str_data[num & 0x00ffffff].param, val);
    else if (prefix == '@')
        sd->reg.set(num, val);
    else if (prefix == '$')
        mapreg_setreg(num, val);
    else if (prefix == '#')
    {
        if (name[1] == '#')
            pc_setaccountreg2(sd, name, val);
        else
            pc_setaccountreg(sd, name, val);
    }
    else // no prefix
        pc_setglobalreg(sd, name, val);
}

std::string get_reg_s(MapSessionData *sd, int32_t num, const std::string& name)
{
    char prefix = name.front();
    if (prefix == '@')
        return sd->regstr.get(num);
    else if (prefix == '$')
        return mapregstr_db.get(num);
    map_log("script: %s: illegal scope string variable !", __func__);
    abort();
}

int32_t get_reg_i(MapSessionData *sd, int32_t num, const std::string& name)
{
    const char prefix = name.front();

    if (str_data[num & 0x00ffffff].type == Script::PARAM)
        return pc_readparam(sd, str_data[num & 0x00ffffff].param);
    else if (prefix == '@')
        return sd->reg.get(num);
    else if (prefix == '$')
        return mapreg_db.get(num);
    else if (prefix == '#')
    {
        if (name[1] == '#')
            return pc_readaccountreg2(sd, name);
        else
            return pc_readaccountreg(sd, name);
    }
    else // no prefix
        return pc_readglobalreg(sd, name);
}

Script ScriptState::type_at(size_t i)
{
    return stack[i].get_type();
}
script_data& ScriptState::raw_at(size_t i)
{
    return stack[i];
}
bool ScriptState::has_at(size_t i)
{
    if (stack.size() != end)
    {
        map_log("Panic: stack.size() != end");
        abort();
    }
    return i < stack.size();
}

/// Convert a stack element to a string
std::string ScriptState::to_string(size_t i)
{
    resolve(i);
    script_data& data = stack[i];
    if (data.get_type() == Script::STR)
        return data.get<Script::STR>();
    if (data.get_type() != Script::INT)
    {
        map_log("Panic: expected Script::STR or Script::INT, got (Script)%d",
                static_cast<uint8_t>(data.get_type()));
        abort();
    }
    char buf[16];
    sprintf(buf, "%d", data.get<Script::INT>());
    std::string ret(buf);
    data.set<Script::STR>(ret);
    return ret;
}

/// convert a stack element to an integer
int32_t ScriptState::to_int(size_t i)
{
    resolve(i);
    script_data& data = stack[i];
    if (data.get_type() == Script::INT)
        return data.get<Script::INT>();
    if (data.get_type() != Script::STR)
    {
        map_log("Panic: expected Script::INT or Script::INT, got (Script)%d",
                static_cast<uint8_t>(data.get_type()));
        abort();
    }
    std::string str = data.get<Script::STR>();
    int32_t num = atoi(str.c_str());
    data.set<Script::INT>(num);
    return num;
}

template<Script Type>
typename ScriptStorageType<Type>::type ScriptState::get_as(size_t idx)
{
    if (stack[idx].get_type() != Type)
    {
        map_log("Panic: wrong type: expect (Script)%d, got (Script)%d",
                static_cast<uint8_t>(Type),
                static_cast<uint8_t>(stack[idx].get_type()));
        abort();
    }
    return stack[idx].get<Type>();
}

template<Script Type>
void ScriptState::push(typename ScriptStorageType<Type>::type value)
{
    script_data new_elt;
    new_elt.set<Type>(value);
    stack.push_back(std::move(new_elt));
}

#define INSTANTIATE_HERE(Type)  \
template typename ScriptStorageType<Script::Type>::type ScriptState::get_as<Script::Type>(size_t idx);  \
template void ScriptState::push<Script::Type>(typename ScriptStorageType<Script::Type>::type value)
INSTANTIATE_HERE(INT);
INSTANTIATE_HERE(POS);
INSTANTIATE_HERE(NAME);
INSTANTIATE_HERE(ARG);
INSTANTIATE_HERE(STR);
INSTANTIATE_HERE(RETINFO);

void ScriptState::push_copy_of(size_t i)
{
    script_data& data = stack[i];
    switch(data.get_type())
    {
#define CASE(Type)                                      \
    case Script::Type:                                  \
        push<Script::Type>(data.get<Script::Type>());   \
        return
    CASE(STR);
    CASE(INT);
    CASE(POS);
    CASE(NAME);
    CASE(ARG);
    CASE(RETINFO);
#undef CASE
    }
    map_log("Panic: copying an unexpected type, (Script)%d",
            static_cast<uint8_t>(data.get_type()));
    abort();
}

void ScriptState::pop(int32_t st, int32_t en)
{
    stack.erase(stack.begin() + st, stack.begin() + en);
}

// builtin functions were implemented here

static Script get_com(const Script *script, int32_t& pos)
{
    if (static_cast<uint8_t>(script[pos]) >= 0x80)
        return Script::INT;
    if (script[pos] >= Script::COUNT)
    {
        map_log("Panic: Got a fat command");
        abort();
    }
    return script[pos++];
}

/// Decode an integer
// just *try* to figure out how this does the opposite of add_scripti
static int32_t get_num(const Script *script, int32_t& pos)
{
    int32_t i = 0;
    int32_t j = 0;
    while (static_cast<uint8_t>(script[pos]) >= 0xc0)
    {
        // why is this mask 0x7f instead of 0x3f?
        i += (static_cast<uint8_t>(script[pos++]) & 0x7f) << j;
        j += 6;
    }
    return i + ((static_cast<uint8_t>(script[pos++]) & 0x7f) << j);
}

template<>
void ScriptState::op<Script::ADD>()
{
    size_t sz = stack.size();
    resolve(sz - 1);
    resolve(sz - 2);
    script_data& d2 = stack[sz - 2];
    script_data& d1 = stack[sz - 1];
    if (d1.get_type() == Script::STR || d2.get_type() == Script::STR)
    {
        std::string res = to_string(sz - 2) + to_string(sz - 1);
        d2.set<Script::STR>(res);
        stack.pop_back();
        return;
    }
    if (d2.get_type() != Script::INT || d1.get_type() != Script::INT)
    {
        map_log("Fatal: expected strings or ints, got (Script)%d + (Script)%d",
                static_cast<uint8_t>(d2.get_type()),
                static_cast<uint8_t>(d1.get_type()));
        abort();
    }
    d2.set<Script::INT>(d2.get<Script::INT>() + d1.get<Script::INT>());
    stack.pop_back();
}

template<Script Type, Script Op>
static int32_t op2(typename ScriptStorageType<Type>::type,
                   typename ScriptStorageType<Type>::type)
{
    map_log("Unimplemented binary operation: op %d applied to (Script)%d",
            static_cast<uint8_t>(Op), static_cast<uint8_t>(Type));
    abort();
    return 0;
}

#define OP_IMPL_STR(Op, op)                         \
template<>                                          \
int32_t op2<Script::STR, Script::Op>(std::string a, \
                                     std::string b) \
{                                                   \
    return a op b;                                  \
}
#define OP_IMPL_INT(Op, op)                     \
template<>                                      \
int32_t op2<Script::INT, Script::Op>(int32_t a, \
                                     int32_t b) \
{                                               \
    return a op b;                              \
}
OP_IMPL_STR(EQ, ==)
OP_IMPL_STR(NE, !=)
OP_IMPL_STR(GT, >)
OP_IMPL_STR(GE, >=)
OP_IMPL_STR(LT, <)
OP_IMPL_STR(LE, <=)

OP_IMPL_INT(EQ, ==)
OP_IMPL_INT(NE, !=)
OP_IMPL_INT(GT, >)
OP_IMPL_INT(GE, >=)
OP_IMPL_INT(LT, <)
OP_IMPL_INT(LE, <=)

OP_IMPL_INT(SUB, -)
OP_IMPL_INT(MUL, *)
OP_IMPL_INT(DIV, /)
OP_IMPL_INT(MOD, %)
OP_IMPL_INT(AND, &)
OP_IMPL_INT(OR, |)
OP_IMPL_INT(XOR, ^)
OP_IMPL_INT(LAND, &&)
OP_IMPL_INT(LOR, ||)
OP_IMPL_INT(SH_R, >>)
OP_IMPL_INT(SH_L, <<)
#undef OP_IMPL_STR
#undef OP_IMPL_INT

// must stuff here
template<Script Op>
void ScriptState::op()
{
    size_t sz = stack.size();
    resolve(sz - 2);
    resolve(sz - 1);
    script_data& d2 = stack[sz - 2];
    script_data& d1 = stack[sz - 1];
    if (d2.get_type() == Script::INT && d1.get_type() == Script::INT)
    {
        int32_t ret = op2<Script::INT, Op>(d2.get<Script::INT>(),
                                           d1.get<Script::INT>());
        d2.set<Script::INT>(ret);
        stack.pop_back();
        return;
    }
    if (d2.get_type() == Script::STR && d1.get_type() == Script::STR)
    {
        int32_t ret = op2<Script::STR, Op>(d2.get<Script::STR>(),
                                           d1.get<Script::STR>());
        d2.set<Script::INT>(ret);
        stack.pop_back();
        return;
    }

    map_log("script: %s: mixed parameters: (Script)%d, (Script)%d", __func__,
            static_cast<uint8_t>(d2.get_type()),
            static_cast<uint8_t>(d2.get_type()));
    abort();
    d2.set<Script::INT>(0);
    stack.pop_back();
}

/// Apply a unary operator to an integer
template<>
void ScriptState::op<Script::NEG>()
{
    script_data& data = stack.back();
    if (data.get_type() != Script::INT)
    {
        map_log("Panic: apply unary op - to noninteger type (Script)%d",
                static_cast<uint8_t>(data.get_type()));
    }
    data.set<Script::INT>(-data.get<Script::INT>());
}

/// Apply a unary operator to an integer
template<>
void ScriptState::op<Script::LNOT>()
{
    script_data& data = stack.back();
    if (data.get_type() != Script::INT)
    {
        map_log("Panic: apply unary ! to noninteger type (Script)%d",
                static_cast<uint8_t>(data.get_type()));
    }
    data.set<Script::INT>(!data.get<Script::INT>());
}

/// Apply a unary operator to an integer
template<>
void ScriptState::op<Script::NOT>()
{
    script_data& data = stack.back();
    if (data.get_type() != Script::INT)
    {
        map_log("Panic: apply unary ~ to noninteger type (Script)%d",
                static_cast<uint8_t>(data.get_type()));
    }
    data.set<Script::INT>(~data.get<Script::INT>());
}

/// Run a function
// how it works:
// 1. push the NAME (index into str_data) onto the stack
// 2. push a variable of type ARG onto the stack with no value
// 3. push various arguments ...
// 4. when you hit the FUNC, execute whatever's there
// args have already been pushed on the stack, starting with Script::ARG
void ScriptState::run_func()
{
    size_t end_sp = stack.size();
    size_t io = end_sp;
    do
    {
        if (io == 0)
        {
            map_log("Panic: tried to run function without argument mark!");
            abort();
            state = ScriptExecutionState::END;
            return;
        }
        --io;
    } while (stack[io].get_type() != Script::ARG);

    size_t start_sp = io - 1;
    start = start_sp;
    end = end_sp;

    int32_t func = stack[start_sp].get<Script::NAME>();

    // only 0 offset use of "start" ...
    // there is no 1 offset, that's the bogus Script::ARG
    if (str_data[func].type != Script::FUNC || !str_data[func].func)
    {
        map_log("Panic: run_func: not a function! \n");
        abort();
        state = ScriptExecutionState::END;
        return;
    }
    str_data[func].func(this);

    pop(start_sp, end_sp);

    if (state == ScriptExecutionState::RETFUNC)
    {
        // something about "annoying arguments"
        pop(defsp, start_sp);
        if (defsp < 4)
        {
            map_log("Error: Not enough of a stack remaining to return!");
            state = ScriptExecutionState::END;
            return;
        }
        if (stack[defsp - 1].get_type() != Script::RETINFO)
        {
            map_log("Return has no RETINFO - there was no callfunc or callsub!");
            state = ScriptExecutionState::END;
            return;
        }

        // number of arguments this function originally got called with
        int32_t ii = stack[defsp - 4].get<Script::INT>();
        // position in the old script
        pos = stack[defsp - 2].get<Script::INT>();

        script = stack[defsp - 1].get<Script::RETINFO>();
        int32_t olddefsp = defsp;
        defsp = stack[defsp - 3].get<Script::INT>();
        pop(olddefsp - 4 - ii, olddefsp);

        state = ScriptExecutionState::GOTO;
    }
}

/// Main script execution
void run_script_main(const Script *script, int32_t pos,
                     ScriptState *st, const Script *rootscript)
{
    // these decrement as it executes
    int32_t cmdcount = 8192;
    int32_t gotocount = 512;

    st->defsp = st->stack.size();
    st->script = script;

    int32_t rerun_pos = st->pos;
    for (st->state = ScriptExecutionState::ZERO; st->state == ScriptExecutionState::ZERO;)
    {
        switch (Script c = get_com(script, st->pos))
        {
        case Script::EOL:
            if (st->stack.size() != st->defsp)
            {
                map_log("Error: stack.sp(%d) != default(%d)\n", st->stack.size(), st->defsp);
                st->stack.resize(st->defsp);
            }
            rerun_pos = st->pos;
            break;
        case Script::INT:
            st->push<Script::INT>(get_num(script, st->pos));
            break;
        case Script::POS:
            st->push<Script::POS>(static_cast<uint8_t>(script[st->pos])
                                  | (static_cast<uint8_t>(script[st->pos + 1]) << 8)
                                  | (static_cast<uint8_t>(script[st->pos + 2]) << 16));
            st->pos += 3;
            break;
        case Script::NAME:
            st->push<Script::NAME>(static_cast<uint8_t>(script[st->pos])
                                   | (static_cast<uint8_t>(script[st->pos + 1]) << 8)
                                   | (static_cast<uint8_t>(script[st->pos + 2]) << 16));
            st->pos += 3;
            break;
        case Script::ARG:
            st->push<Script::ARG>(0);
            break;
        case Script::STR:
        {
            std::string str = reinterpret_cast<const char *>(&script[st->pos]);
            st->push<Script::STR>(str);
            st->pos += str.size() + 1;
            break;
        }
        case Script::FUNC:
            st->run_func();
            if (st->state == ScriptExecutionState::GOTO)
            {
                rerun_pos = st->pos;
                script = st->script;
                st->state = ScriptExecutionState::ZERO;
                if (gotocount > 0 && (--gotocount) <= 0)
                {
                    map_log("run_script: infinite loop (too many gotos)!\n");
                    abort();
                    st->state = ScriptExecutionState::END;
                }
            }
            break;

        case Script::ADD:  st->op<Script::ADD>();break;

        case Script::SUB:  st->op<Script::SUB>();break;
        case Script::MUL:  st->op<Script::MUL>();break;
        case Script::DIV:  st->op<Script::DIV>();break;
        case Script::MOD:  st->op<Script::MOD>();break;
        case Script::EQ:   st->op<Script::EQ>();break;
        case Script::NE:   st->op<Script::NE>();break;
        case Script::GT:   st->op<Script::GT>();break;
        case Script::GE:   st->op<Script::GE>();break;
        case Script::LT:   st->op<Script::LT>();break;
        case Script::LE:   st->op<Script::LE>();break;
        case Script::AND:  st->op<Script::AND>();break;
        case Script::OR:   st->op<Script::OR>();break;
        case Script::XOR:  st->op<Script::XOR>();break;
        case Script::LAND: st->op<Script::LAND>();break;
        case Script::LOR:  st->op<Script::LOR>();break;
        case Script::SH_R: st->op<Script::SH_R>();break;
        case Script::SH_L: st->op<Script::SH_L>();break;

        case Script::NEG:  st->op<Script::NEG>();break;
        case Script::NOT:  st->op<Script::NOT>();break;
        case Script::LNOT: st->op<Script::LNOT>();break;

        case Script::NOP:
            st->state = ScriptExecutionState::END;
            break;

        default:
            map_log("unknown command : %d @ %d\n", static_cast<uint8_t>(c), pos);
            abort();
            st->state = ScriptExecutionState::END;
            break;
        }
        if (cmdcount > 0 && (--cmdcount) <= 0)
        {
            printf("run_script: infinite loop (too many commands)!\n");
            st->state = ScriptExecutionState::END;
        }
    }
    switch (st->state)
    {
    case ScriptExecutionState::RERUNLINE:
        // for menu; and input;
        st->pos = rerun_pos;
        break;
    case ScriptExecutionState::END:
        MapSessionData *sd = map_id2sd(st->rid);
        st->pos = -1;
        if (sd && sd->npc_id == st->oid)
            npc_event_dequeue(sd);
        break;
    }

    if (st->state == ScriptExecutionState::END)
        return;
    MapSessionData *sd = map_id2sd(st->rid);
    if (!sd)
        return;

    sd->npc_stackbuf = std::move(st->stack);
    sd->npc_script = script;
    sd->npc_scriptroot = rootscript;
}

/// Run a script without any special variables
int32_t run_script(const std::vector<Script>& script, int32_t pos, int32_t rid, int32_t oid)
{
    return run_script_l(script, pos, rid, oid, 0, NULL);
}

int32_t run_script_l(const std::vector<Script>& rscript, int32_t pos, int32_t rid, int32_t oid, int32_t args_nr, ArgRec *args)
{
    if (rscript.empty() || pos < 0)
        return -1;

    const Script *script = &rscript[0];
    ScriptState st;
    MapSessionData *sd = map_id2sd(rid);
    const Script *rootscript = script;

    if (sd && !sd->npc_stackbuf.empty() && sd->npc_scriptroot == rootscript)
    {
        script = sd->npc_script;
        st.stack = std::move(sd->npc_stackbuf);
    }
    st.pos = pos;
    st.rid = rid;
    st.oid = oid;
    for (int32_t i = 0; i < args_nr; i++)
    {
        if (args[i].name[strlen(args[i].name) - 1] == '$')
            sd->regstr.set(add_str(args[i].name), args[i].s);
        else
            sd->reg.set(add_str(args[i].name), args[i].i);
    }
    run_script_main(script, pos, &st, rootscript);
    return st.pos;
}

/// Sets a global integer variable (temporary or permanent)
void mapreg_setreg(int32_t num, int32_t val)
{
    mapreg_db.set(num, val);
    mapreg_dirty = 1;
}

/// Sets a global string variable (temporary or permanent)
void mapreg_setregstr(int32_t num, const std::string& str)
{
    mapregstr_db.set(num, str);
    mapreg_dirty = 1;
}

/// Load the saved values
static int32_t script_load_mapreg(void)
{
    FILE *fp = fopen_(mapreg_txt, "r");

    // hopefully this only happens the first time the server starts
    if (!fp)
        return -1;

    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        char buf1[256], buf2[1024];
        int32_t n, i;
        if (sscanf(line, "%255[^,],%d\t%n", buf1, &i, &n) != 2 &&
            (i = 0, sscanf(line, "%[^\t]\t%n", buf1, &n) != 1))
            continue;
        if (buf1[strlen(buf1) - 1] == '$')
        {
            if (sscanf(line + n, "%[^\n\r]", buf2) != 1)
            {
                map_log("%s: %s broken data !\n", mapreg_txt, buf1);
                continue;
            }
            std::string p = buf2;
            int32_t s = add_str(buf1);
            mapregstr_db.set((i << 24) + s, p);
        }
        else
        {
            int32_t v;
            if (sscanf(line + n, "%d", &v) != 1)
            {
                map_log("%s: %s broken data !\n", mapreg_txt, buf1);
                continue;
            }
            int32_t s = add_str(buf1);
            mapreg_db.set((i << 24) + s, v);
        }
    }
    fclose_(fp);
    return 0;
}

/// Maybe save an integer
static void script_save_mapreg_intsub(int32_t keyi, int32_t datai, FILE *fp)
{
    int32_t num = keyi & 0x00ffffff, i = keyi >> 24;
    const std::string& name = str_data[num].str;
    if (name[1] != '@')
    {
        if (i == 0)
            fprintf(fp, "%s\t%d\n", name.c_str(), datai);
        else
            fprintf(fp, "%s,%d\t%d\n", name.c_str(), i, datai);
    }
}

/// Maybe save a string
static void script_save_mapreg_strsub(int32_t keyi, const std::string& datap, FILE *fp)
{
    int32_t num = keyi & 0x00ffffff, i = keyi >> 24;
    const std::string& name = str_data[num].str;
    if (name[1] != '@')
    {
        if (i == 0)
            fprintf(fp, "%s\t%s\n", name.c_str(), datap.c_str());
        else
            fprintf(fp, "%s,%d\t%s\n", name.c_str(), i, datap.c_str());
    }
}

static int32_t script_save_mapreg(void)
{
    int32_t lock;
    FILE *fp = lock_fopen(mapreg_txt, &lock);

    if (!fp)
        return -1;
    for (const auto& ip : mapreg_db)
        script_save_mapreg_intsub(ip.first, ip.second, fp);
    for (const auto& ip : mapregstr_db)
        script_save_mapreg_strsub(ip.first, ip.second, fp);
    lock_fclose(fp, mapreg_txt, &lock);
    mapreg_dirty = false;
    return 0;
}

static void script_autosave_mapreg(timer_id, tick_t)
{
    if (mapreg_dirty)
        script_save_mapreg();
}

void do_final_script(void)
{
    if (mapreg_dirty)
        script_save_mapreg();
}

/// Load mapreg
void do_init_script(void)
{
    script_load_mapreg();

    add_timer_interval(gettick() + MAPREG_AUTOSAVE_INTERVAL,
                       MAPREG_AUTOSAVE_INTERVAL, script_autosave_mapreg);

    scriptlabel_db = strdb_init();
}
