#include "ip.hpp"

void IP_Address::from_sock(in_addr in)
{
    memcpy(data, &in, 4);
}

bool IP_Address::from_string(const std::string& in)
{
    int n;
    return 4 == sscanf(in.c_str(), "%hhu.%hhu.%hhu.%hhu%n", &data[0], &data[1], &data[2], &data[3], &n)
        && n == in.length();
}

std::string IP_Address::to_string() const
{
    char buf[16];
    sprintf(buf, "%hhu.%hhu.%hhu.%hhu", data[0], data[1], data[2], data[3]);
    return buf;
}

bool IP_Mask::covers(IP_Address test)
{
    for (int i = 0; i < 4; i++)
    {
        if ((addr.data[i] & mask.data[i]) != (test.data[i] & mask.data[i]))
            return false;
    }
    return true;
}

void IP_Mask::from_string(const std::string& str)
{
    addr.from_n(0);
    mask.from_n(0);

    uint8_t *p = addr.data;
    int offset;
    switch (sscanf(str.c_str(), "%hhu.%hhu.%hhu.%hhu%n", &p[0], &p[1], &p[2], &p[3], &offset))
    {
    case 0: return;
    case 1: mask.high_mask(8); return;
    case 2: mask.high_mask(16); return;
    case 3: mask.high_mask(24); return;
    case 4: break;
    default: abort();
    }
    if (str[offset] != '/')
    {
        mask.high_mask(32);
        return;
    }
    ++offset;
    p = mask.data;
    switch (sscanf(str.c_str() + offset, "%hhu.%hhu.%hhu.%hhu", &p[0], &p[1], &p[2], &p[3]))
    {
    case 1: if (p[0] > 32) abort(); mask.high_mask(p[0]); return;
    case 4: return;
    }
    abort();
}

std::string IP_Mask::to_string() const
{
    char buf[32];
    sprintf(buf, "%hhu.%hhu.%hhu.%hhu/%hhu.%hhu.%hhu.%hhu",
            addr.data[0], addr.data[1], addr.data[2], addr.data[3],
            mask.data[0], mask.data[1], mask.data[2], mask.data[3]);
    return buf;
}
