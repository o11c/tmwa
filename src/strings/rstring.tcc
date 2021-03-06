//    strings/rstring.tcc - Inline functions for rstring.hpp
//
//    Copyright © 2013 Ben Longbons <b.r.longbons@gmail.com>
//
//    This file is part of The Mana World (Athena server)
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "mstring.hpp"


namespace tmwa
{
namespace strings
{
    template<class It>
    void RString::_assign(It b, It e)
    {
        owned = nullptr;
        if (b == e)
        {
            *this = RString();
            return;
        }
        if (!std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<It>::iterator_category>::value)
        {
            // can't use std::distance
            MString m;
            for (; b != e; ++b)
                m += *b;
            *this = RString(m); // will recurse
            return;
        }
        size_t diff = std::distance(b, e);
        owned = static_cast<Rep *>(::operator new(sizeof(Rep) + diff + 1));
        owned->count = 0;
        owned->size = diff;
        std::copy(b, e, owned->body);
        owned->body[diff] = '\0';
    }

    template<class It>
    RString::RString(It b, It e)
    {
        _assign(b, e);
    }

    template<uint8_t n>
    RString::RString(const VString<n>& v)
    {
        _assign(v.begin(), v.end());
    }
} // namespace strings
} // namespace tmwa
