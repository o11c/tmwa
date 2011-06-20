#ifndef IP_HPP
#define IP_HPP

# include <cstddef>
# include <cstring>

# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>

# include <string>

/// The address of a computer on the internet
/// Currently, is always an IPv4 address
/// (also used for masks)
class IP_Address
{
    friend class IP_Mask;
    uint8_t data[4];
public:
    void from_sock(in_addr in);
    void from_sock(const sockaddr_in *sock)
    {
        from_sock(sock->sin_addr);
    }
    bool from_string(const std::string& in);
    std::string to_string() const;
    in_addr_t to_n() const
    {
        return *reinterpret_cast<const in_addr_t *>(data);
    }
    void from_n(in_addr_t v)
    {
        *reinterpret_cast<in_addr_t *>(data) = v;
    }

    bool high_mask(uint8_t bits)
    {
        if (bits > 32)
            return false;
        if (!bits)
            return true;
        uint32_t mask = -1;
        mask <<= (32 - bits);
        data[0] = mask >> 24;
        data[1] = mask >> 16;
        data[2] = mask >> 8;
        data[3] = mask;
        return true;
    }

    IP_Address() : data({0, 0, 0, 0}) {}
    explicit IP_Address(const std::string& in)
    {
        if (!from_string(in))
            memset(data, '\0', 4);
    }
    explicit IP_Address(const sockaddr_in *in)
    {
        from_sock(in);
    }

    bool operator == (const IP_Address& rhs) const
    {
        return memcmp(this, &rhs, sizeof(IP_Address)) == 0;
    }

    bool operator != (const IP_Address& rhs) const
    {
        return memcmp(this, &rhs, sizeof(IP_Address)) != 0;
    }
};

class IP_Mask
{
public:
    IP_Address addr;
    IP_Address mask;

    IP_Mask(IP_Address a, IP_Address m) : addr(a), mask(m) {}
    IP_Mask() : addr(), mask() {}
    // prefix x[.y[.z[.w]]] or mask x.x.x.x/# or x.x.x.x/y.y.y.y)
    void from_string(const std::string&);
    explicit IP_Mask(const std::string& str)
    {
        from_string(str);
    }
    bool covers(IP_Address test);
    bool covers_all()
    {
        return mask == IP_Address();
    }
    std::string to_string() const;
};
#endif //IP_HPP
