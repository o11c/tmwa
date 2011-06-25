#ifndef STRINGS_HPP
#define STRINGS_HPP

#include <string>

inline void replace_all(std::string& str, const std::string& target, const std::string& replacement)
{
    if (target.empty())
        return;
    size_t pos = 0;
    while (pos < str.length())
    {
        pos = str.find(target, pos);
        if (pos == std::string::npos)
            break;
        str.replace(pos, target.size(), replacement);
        pos += replacement.size();
    }
}
#endif // STRINGS_HPP
