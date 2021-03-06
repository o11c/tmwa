#pragma once
//    strings/literal.hpp - A string stored in the readonly data segment.
//
//    Copyright © 2014 Ben Longbons <b.r.longbons@gmail.com>
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

#include "fwd.hpp"

#include "base.hpp"


namespace tmwa
{
namespace strings
{
    /// A statically owned string that is guaranteed to be NUL-terminated.
    /// This is a more permissive lifetime than anybody else has.
    class LString : public _crtp_string<LString, AString, LPair>
    {
        iterator _b, _e;
    private:
        LString(const char *b, const char *e);
        friend LString operator "" _s(const char *, size_t);
        // for tail slicing
        LString(const char *b, const char *e, const RString *) : LString(b, e) {}
        friend class _crtp_string<LString, AString, LPair>;
    public:

        iterator begin() const;
        iterator end() const;
        const RString *base() const;
        const char *c_str() const;
    };

    class FormatString
    {
        const char *_format;

        friend constexpr FormatString operator "" _fmt(const char *, size_t);
        constexpr explicit
        FormatString(const char *f) : _format(f) {}
    public:
        constexpr
        const char *format_string() const { return _format; }
    };


    // cxxstdio helpers
    // I think the conversion will happen automatically. TODO test this.
    // Nope, it doesn't, since there's a template
    // Actually, it might now.
    const char *decay_for_printf(const LString& zs);

    inline
    LString operator "" _s(const char *s, size_t)
    {
        return LString(s, s + __builtin_strlen(s));
    }
    constexpr
    FormatString operator "" _fmt(const char *s, size_t)
    {
        return FormatString(s);
    }
} // namespace strings
} // namespace tmwa
