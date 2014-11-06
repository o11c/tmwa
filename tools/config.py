#!/usr/bin/env python
# coding: utf-8

#   config.py - generator for config file parsers
#
#   Copyright © 2014 Ben Longbons <b.r.longbons@gmail.com>
#
#   This file is part of The Mana World (Athena server)
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU Affero General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.

from __future__ import print_function

import glob
import os

from protocol import OpenWrite


generated = '// This is a generated file, edit %s instead\n' % __file__

copyright = '''//    {filename} - {description}
//
//    Copyright © 2014 Ben Longbons <b.r.longbons@gmail.com>
//
//    This file is part of The Mana World (Athena server)
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
'''


class AnyHeader(object):
    __slots__ = ('name')

    def __init__(self, name):
        self.name = name

class SystemHeader(AnyHeader):
    __slots__ = ()
    meta = 0

    def relative_to(self, path):
        return '<%s>' % self.name

class Header(AnyHeader):
    __slots__ = ()
    meta = 1

    def relative_to(self, path):
        return '"%s"' % os.path.relpath(self.name, path)


class ConfigType(object):
    __slots__ = ()

class SimpleType(ConfigType):
    __slots__ = ('name', 'headers')

    def __init__(self, name, headers):
        self.name = name
        self.headers = frozenset(headers)

    def __repr__(self):
        return 'SimpleType(%r, %r)' % (self.name, self.headers)

    def type_name(self):
        return self.name

    def dump_extract(self, cpp, var):
        cpp.write(
'''
            if (!extract(value.data, &{var}))
            {{
                value.span.error("Failed to extract value"_s);
                return false;
            }}
'''.lstrip('\n').format(var=var))

class PhonyType(ConfigType):
    __slots__ = ('type', 'name', 'call', 'headers')

    def __init__(self, type, name, call, extra_headers):
        self.type = type
        self.name = name
        self.call = call
        self.headers = type.headers | extra_headers

    def __repr__(self):
        return 'PhonyType(%r, %r, %r, %r)' % (self.type, self.name, self.call, self.headers)

    def type_name(self):
        return '// special %s' % self.type.type_name()

    def dump_extract(self, cpp, var):
        cpp.write('            %s %s;\n' % (self.type.type_name(),  self.name))
        self.type.dump_extract(cpp, self.name)
        cpp.write('            %s\n' % self.call)

class TransformedType(ConfigType):
    __slots__ = ('type', 'transform', 'headers')

    def __init__(self, type, transform, extra_headers=set()):
        self.type = type
        self.transform = transform
        self.headers = type.headers | extra_headers

    def __repr__(self):
        return 'TransformedType(%r, %r)' % (self.type, self.transform)

    def type_name(self):
        return self.type.type_name()

    def dump_extract(self, cpp, var):
        self.type.dump_extract(cpp, var)
        cpp.write('            %s;\n' % self.transform)

class BoundedType(ConfigType):
    __slots__ = ('type', 'low', 'high', 'headers')

    def __init__(self, type, low, high, extra_headers=set()):
        assert isinstance(type, ConfigType)
        self.type = type
        self.low = low
        self.high = high
        self.headers = type.headers | extra_headers

    def __repr__(self):
        return 'BoundedType(%r, %r, %r, %r)' % (self.type, self.low, self.high, self.headers)

    def type_name(self):
        return self.type.type_name()

    def dump_extract(self, cpp, var):
        self.type.dump_extract(cpp, var)
        cpp.write(
'''
            if (!({low} <= {var} && {var} <= {high}))
            {{
                line.error("Value of {name} not in range [{low}, {high}]"_s);
                return false;
            }}
'''.format(low=self.low, high=self.high, var=var, name=var.split('.')[-1]))

class MinBoundedType(ConfigType):
    __slots__ = ('type', 'low', 'headers')

    def __init__(self, type, low, extra_headers=set()):
        assert isinstance(type, ConfigType)
        self.type = type
        self.low = low
        self.headers = type.headers | extra_headers

    def __repr__(self):
        return 'MinBoundedType(%r, %r, %r, %r)' % (self.type, self.low, self.headers)

    def type_name(self):
        return self.type.type_name()

    def dump_extract(self, cpp, var):
        self.type.dump_extract(cpp, var)
        cpp.write(
'''
            if (!({low} <= {var}))
            {{
                line.error("Value of {name} not at least {low}"_s);
                return false;
            }}
'''.format(low=self.low, var=var, name=var.split('.')[-1]))

class Option(object):
    __slots__ = ('name', 'type', 'default', 'headers')

    def __init__(self, name, type, default, extra_headers=set()):
        self.name = name
        self.type = type
        self.default = default
        self.headers = type.headers | extra_headers

    def dump1(self, hpp):
        hpp.write('    %s %s = %s;\n' % (self.type.type_name(), self.name, self.default))
    def dump2(self, cpp, x):
        # NOTE about hashing
        # dead simple hash: pack 6 bits of first 5 letters into an int
        y = self.name[:5]
        if x != y:
            if x is not None:
                cpp.write('        break;\n')
            c0 = y[0] if len(y) > 0 else '\\0'
            c1 = y[1] if len(y) > 1 else '\\0'
            c2 = y[2] if len(y) > 2 else '\\0'
            c3 = y[3] if len(y) > 3 else '\\0'
            c4 = y[4] if len(y) > 4 else '\\0'
            assert len(y) >= 3, '<-- change this number in the source file for: %r' % self.name
            cpp.write("    case (('%s' << 24) | ('%s' << 18) | ('%s' << 12) | ('%s' << 6) | ('%s' << 0)):\n" % (c0, c1, c2, c3, c4))
        cpp.write('        if (key == "{name}"_s)\n'.format(name=self.name))
        cpp.write('        {\n')
        self.type.dump_extract(cpp, 'conf.%s' % self.name)
        cpp.write('            return true;\n')
        cpp.write('        }\n')
        return y

class Group(object):
    __slots__ = ('name', 'options')

    def __init__(self, name):
        self.name = name
        self.options = {}

    def opt(self, name, type, default, extra_headers=set(), pre=None, post=None, min=None, max=None):
        assert name not in self.options, 'Duplicate option name: %s' % name
        if pre is not None:
            type = TransformedType(type, pre)
        if min is not None:
            if max is not None:
                type = BoundedType(type, min, max)
            else:
                type = MinBoundedType(type, min)
        else:
            assert max is None
        if post is not None:
            type = TransformedType(type, post)
        self.options[name] = rv = Option(name, type, default, extra_headers)
        return rv

    def dump_in(self, path):
        var_name = '%s_conf' % self.name
        class_name = var_name.replace('_', ' ').title().replace(' ', '')
        short_hpp_name = '%s.hpp' % var_name
        hpp_name = os.path.join(path, short_hpp_name)
        short_cpp_name = '%s.cpp' % var_name
        cpp_name = os.path.join(path, short_cpp_name)

        values = sorted(self.options.values(), key=lambda o: o.name)

        desc = 'Config for %s::%s' % (path.split('/')[-1], self.name)
        with OpenWrite(hpp_name) as hpp, \
                OpenWrite(cpp_name) as cpp:
            hpp.write('#pragma once\n')
            hpp.write(copyright.format(filename=short_hpp_name, description=desc))
            cpp.write('#include "%s"\n' % short_hpp_name)
            cpp.write(copyright.format(filename=short_cpp_name, description=desc))
            headers = {Header('src/io/fwd.hpp'), Header('src/strings/fwd.hpp')}
            for o in values:
                headers |= o.headers

            hpp.write('\n')
            hpp.write('#include "fwd.hpp"\n')
            for h in sorted(headers, key=lambda h: (h.meta, h.name)):
                hpp.write('#include %s\n' % h.relative_to(path))
            hpp.write('\n')

            hpp.write('namespace tmwa\n{\n')
            cpp.write('namespace tmwa\n{\n')
            hpp.write('struct %s\n{\n' % class_name)
            for o in values:
                o.dump1(hpp)
            hpp.write('}; // struct %s\n' % class_name)
            hpp.write('bool parse_%s(%s& conf, XString key, io::Spanned<XString> value);\n' % (var_name, class_name))
            hpp.write('} // namespace tmwa\n')
            cpp.write('bool parse_%s(%s& conf, XString key, io::Spanned<XString> value)\n{\n' % (var_name, class_name))
            # see NOTE about hashing in Option.dump2
            cpp.write('    int key_hash = 0;\n')
            cpp.write('    if (key.size() > 0)\n')
            cpp.write('        key_hash |= key[0] << 24;\n')
            cpp.write('    if (key.size() > 1)\n')
            cpp.write('        key_hash |= key[1] << 18;\n')
            cpp.write('    if (key.size() > 2)\n')
            cpp.write('        key_hash |= key[2] << 12;\n')
            cpp.write('    if (key.size() > 3)\n')
            cpp.write('        key_hash |= key[3] << 6;\n')
            cpp.write('    if (key.size() > 4)\n')
            cpp.write('        key_hash |= key[4] << 0;\n')
            cpp.write('    switch (key_hash)\n{\n')
            x = None
            for o in values:
                x = o.dump2(cpp, x)
            cpp.write('        break;\n')
            cpp.write('    } // switch\n')
            cpp.write('} // fn parse_*_conf()\n')
            cpp.write('} // namespace tmwa\n')

class Realm(object):
    __slots__ = ('path', 'groups')

    def __init__(self, path):
        self.path = path
        self.groups = {}

    def conf(self, name=None):
        if not name:
            name = self.path.split('/')[-1]
        assert name not in self.groups, 'Duplicate group name: %s' % name
        self.groups[name] = rv = Group(name)
        return rv

    def dump(self):
        for g in self.groups.values():
            g.dump_in(self.path)

class Everything(object):
    __slots__ = ('realms')

    def __init__(self):
        self.realms = {}

    def realm(self, path):
        assert path not in self.realms, 'Duplicate realm path: %s' % path
        self.realms[path] = rv = Realm(path)
        return rv

    def dump(self):
        for g in glob.glob('src/*/*_conf.[ch]pp'):
            os.rename(g, g + '.old')
        for v in self.realms.values():
            v.dump()
        for g in glob.glob('src/*/*_conf.[ch]pp.old'):
            print('Obsolete: %s' % g)
            os.remove(g)


def lit(s):
    return '"%s"_s' % s.replace('\\', '\\\\').replace('"', '\\"')

def build_config():
    rv = Everything()

    # realms
    login_realm = rv.realm('src/login')
    admin_realm = rv.realm('src/admin')
    char_realm = rv.realm('src/char')
    map_realm = rv.realm('src/map')

    # confs
    login_conf = login_realm.conf()
    login_lan_conf = login_realm.conf('login_lan')

    admin_conf = admin_realm.conf()

    char_conf = char_realm.conf()
    char_lan_conf = char_realm.conf('char_lan')
    inter_conf = char_realm.conf('inter')

    map_conf = map_realm.conf()
    battle_conf = map_realm.conf('battle')

    # headers
    cstdint_sys = SystemHeader('cstdint')
    vector_sys = SystemHeader('vector')
    bitset_sys = SystemHeader('bitset')

    ip_h = Header('src/net/ip.hpp')
    rstring_h = Header('src/strings/rstring.hpp')
    literal_h = Header('src/strings/literal.hpp')
    ids_h = Header('src/mmo/ids.hpp')
    strs_h = Header('src/mmo/strs.hpp')
    timer_th = Header('src/net/timer.t.hpp')
    login_th = Header('src/login/login.t.hpp')
    udl_h = Header('src/ints/udl.hpp')
    net_point_h = Header('src/proto2/net-Point.hpp')
    char_h = Header('src/char/char.hpp')
    map_h = Header('src/map/map.hpp')
    map_th = Header('src/map/map.t.hpp')
    npc_h = Header('src/map/npc.hpp')

    # types
    boolean = SimpleType('bool', set())
    u8 = SimpleType('uint8_t', {cstdint_sys})
    u16 = SimpleType('uint16_t', {cstdint_sys})
    u32 = SimpleType('uint32_t', {cstdint_sys})
    u64 = SimpleType('uint64_t', {cstdint_sys})
    i8 = SimpleType('int8_t', {cstdint_sys})
    i16 = SimpleType('int16_t', {cstdint_sys})
    i32 = SimpleType('int32_t', {cstdint_sys})
    i64 = SimpleType('int64_t', {cstdint_sys})

    IP4Address = SimpleType('IP4Address', {ip_h})
    IP4Mask = SimpleType('IP4Mask', {ip_h})
    IpSet = SimpleType('std::vector<IP4Mask>', {vector_sys, ip_h})
    RString = SimpleType('RString', {rstring_h, literal_h})
    GmLevel = SimpleType('GmLevel', {ids_h})
    seconds = SimpleType('std::chrono::seconds', {timer_th})
    milliseconds = SimpleType('std::chrono::milliseconds', {timer_th})
    ACO = SimpleType('ACO', {login_th})
    ServerName = SimpleType('ServerName', {strs_h})
    AccountName = SimpleType('AccountName', {strs_h})
    AccountPass = SimpleType('AccountPass', {strs_h})
    Point = SimpleType('Point', {net_point_h})
    CharName = SimpleType('CharName', {strs_h})
    CharBitset = SimpleType('std::bitset<256>', {bitset_sys})
    MapName = SimpleType('MapName', {strs_h})
    ATK = SimpleType('ATK', {map_th})

    addmap = PhonyType(MapName, 'name', 'map_addmap(name);', {map_h})
    delmap = PhonyType(MapName, 'name', 'map_delmap(name);', {map_h})
    addnpc = PhonyType(RString, 'npc', 'npc_addsrcfile(npc);', {npc_h})
    delnpc = PhonyType(RString, 'npc', 'npc_delsrcfile(npc);', {npc_h})


    # options
    login_lan_conf.opt('lan_char_ip', IP4Address, 'IP4_LOCALHOST')
    login_lan_conf.opt('lan_subnet', IP4Mask, 'IP4Mask(IP4_LOCALHOST, IP4_BROADCAST)')

    login_conf.opt('admin_state', boolean, 'false')
    login_conf.opt('admin_pass', AccountPass, '{}')
    login_conf.opt('ladminallowip', IpSet, '{}')
    login_conf.opt('gm_pass', RString, '{}')
    login_conf.opt('level_new_gm', GmLevel, 'GmLevel::from(60_u32)', {udl_h})
    login_conf.opt('new_account', boolean, 'false')
    login_conf.opt('login_port', u16, '6901')
    login_conf.opt('account_filename', RString, lit('save/account.txt'))
    login_conf.opt('gm_account_filename', RString, lit('save/gm_account.txt'))
    login_conf.opt('gm_account_filename_check_timer', seconds, '15_s')
    login_conf.opt('login_log_filename', RString, lit('log/login.log'))
    login_conf.opt('display_parse_login', boolean, 'false')
    login_conf.opt('display_parse_admin', boolean, 'false')
    login_conf.opt('display_parse_fromchar', boolean, 'false')
    login_conf.opt('min_level_to_connect', GmLevel, 'GmLevel::from(0_u32)', {udl_h})
    login_conf.opt('order', ACO, 'ACO::DENY_ALLOW')
    login_conf.opt('allow', IpSet, '{}')
    login_conf.opt('deny', IpSet, '{}')
    login_conf.opt('anti_freeze_enable', boolean, 'false')
    login_conf.opt('anti_freeze_interval', seconds, '15_s')
    login_conf.opt('update_host', RString, '{}')
    login_conf.opt('main_server', ServerName, '{}')
    login_conf.opt('userid', AccountName, '{}')
    login_conf.opt('passwd', AccountPass, '{}')


    admin_conf.opt('login_ip', IP4Address, 'IP4_LOCALHOST')
    admin_conf.opt('login_port', u16, '6901')
    admin_conf.opt('admin_pass', AccountPass, 'stringish<AccountPass>("admin"_s)')
    admin_conf.opt('ladmin_log_filename', RString, lit('log/ladmin.log'))


    char_lan_conf.opt('lan_map_ip', IP4Address, 'IP4_LOCALHOST')
    char_lan_conf.opt('lan_subnet', IP4Mask, 'IP4Mask(IP4_LOCALHOST, IP4_BROADCAST)')

    char_conf.opt('userid', AccountName, '{}')
    char_conf.opt('passwd', AccountPass, '{}')
    char_conf.opt('server_name', ServerName, '{}')
    char_conf.opt('login_ip', IP4Address, '{}')
    char_conf.opt('login_port', u16, '6901')
    char_conf.opt('char_ip', IP4Address, '{}')
    char_conf.opt('char_port', u16, '6121')
    char_conf.opt('char_txt', RString, '{}')
    char_conf.opt('max_connect_user', u32, '0')
    char_conf.opt('autosave_time', seconds, 'DEFAULT_AUTOSAVE_INTERVAL', {char_h}, min='1_s')
    char_conf.opt('start_point', Point, '{ {"001-1.gat"_s}, 273, 354 }')
    char_conf.opt('unknown_char_name', CharName, 'stringish<CharName>("Unknown"_s)')
    char_conf.opt('char_log_filename', RString, lit('log/char.log'))
    char_conf.opt('char_name_letters', CharBitset, '{}')
    char_conf.opt('online_txt_filename', RString, lit('online.txt'))
    char_conf.opt('online_html_filename', RString, lit('online.html'))
    char_conf.opt('online_gm_display_min_level', GmLevel, 'GmLevel::from(20_u32)', {udl_h})
    char_conf.opt('online_refresh_html', u32, '20', min=1)
    char_conf.opt('anti_freeze_enable', boolean, 'false')
    char_conf.opt('anti_freeze_interval', seconds, '6_s', min='5_s')

    inter_conf.opt('storage_txt', RString, lit('save/storage.txt'))
    inter_conf.opt('party_txt', RString, lit('save/party.txt'))
    inter_conf.opt('accreg_txt', RString, lit('save/accreg.txt'))
    inter_conf.opt('party_share_level', u32, '10')

    map_conf.opt('userid', AccountName, '{}')
    map_conf.opt('passwd', AccountPass, '{}')
    map_conf.opt('char_ip', IP4Address, '{}')
    map_conf.opt('char_port', u16, '6121')
    map_conf.opt('map_ip', IP4Address, '{}')
    map_conf.opt('map_port', u16, '5121')
    map_conf.opt('map', addmap, '{}')
    map_conf.opt('delmap', delmap, '{}')
    map_conf.opt('npc', addnpc, '{}')
    map_conf.opt('delnpc', delnpc, '{}')
    map_conf.opt('autosave_time', seconds, 'DEFAULT_AUTOSAVE_INTERVAL', {map_h})
    map_conf.opt('motd_txt', RString, lit('conf/motd.txt'))
    map_conf.opt('mapreg_txt', RString, lit('save/mapreg.txt'))
    map_conf.opt('gm_log', RString, '{}')
    map_conf.opt('log_file', RString, '{}')

    battle_conf.opt('warp_point_debug',                     RString, '0')
    battle_conf.opt('enemy_critical',                       RString, '0')
    battle_conf.opt('enemy_critical_rate',                  RString, '100')
    battle_conf.opt('enemy_str',                            RString, '1')
    battle_conf.opt('enemy_perfect_flee',                   RString, '0')
    battle_conf.opt('casting_rate',                         RString, '100')
    battle_conf.opt('delay_rate',                           RString, '100')
    battle_conf.opt('delay_dependon_dex',                   RString, '0')
    battle_conf.opt('skill_delay_attack_enable',            RString, '0')
    battle_conf.opt('monster_skill_add_range',              RString, '0')
    battle_conf.opt('player_damage_delay',                  RString, '1')
    battle_conf.opt('flooritem_lifetime',                   milliseconds, 'LIFE_FLOORITEM', min='1_s')
    battle_conf.opt('item_auto_get',                        RString, '0')
    battle_conf.opt('item_first_get_time',                  RString, '3000')
    battle_conf.opt('item_second_get_time',                 RString, '1000')
    battle_conf.opt('item_third_get_time',                  RString, '1000')
    battle_conf.opt('base_exp_rate',                        RString, '100')
    battle_conf.opt('job_exp_rate',                         RString, '100')
    battle_conf.opt('death_penalty_type',                   RString, '0')
    battle_conf.opt('death_penalty_base',                   RString, '0')
    battle_conf.opt('death_penalty_job',                    RString, '0')
    battle_conf.opt('restart_hp_rate',                      RString, '0', min='0', max='100')
    battle_conf.opt('restart_sp_rate',                      RString, '0', min='0', max='100')
    battle_conf.opt('monster_hp_rate',                      RString, '0')
    battle_conf.opt('monster_max_aspd',                     RString, '199', pre='conf.monster_max_aspd = 2000 - conf.monster_max_aspd * 10;', min='10', max='1000')
    battle_conf.opt('atcommand_gm_only',                    RString, '0')
    battle_conf.opt('atcommand_spawn_quantity_limit',       RString, '{}')
    battle_conf.opt('gm_all_equipment',                     RString, '0')
    battle_conf.opt('monster_active_enable',                RString, '1')
    battle_conf.opt('mob_skill_use',                        RString, '1')
    battle_conf.opt('mob_count_rate',                       RString, '100')
    battle_conf.opt('basic_skill_check',                    RString, '1')
    battle_conf.opt('player_invincible_time',               RString, '5000')
    battle_conf.opt('skill_min_damage',                     RString, '0')
    battle_conf.opt('natural_healhp_interval',              RString, '6000', {map_h}, min='NATURAL_HEAL_INTERVAL')
    battle_conf.opt('natural_healsp_interval',              RString, '8000', {map_h}, min='NATURAL_HEAL_INTERVAL')
    battle_conf.opt('natural_heal_weight_rate',             RString, '50', min='50', max='101')
    battle_conf.opt('arrow_decrement',                      RString, '1')
    battle_conf.opt('max_aspd',                             RString, '199', pre='conf.max_aspd = 2000 - conf.max_aspd * 10;', min='10', max='1000')
    battle_conf.opt('max_hp',                               RString, '32500', min='100', max='1000000')
    battle_conf.opt('max_sp',                               RString, '32500', min='100', max='1000000')
    battle_conf.opt('max_lv',                               RString, '99')
    battle_conf.opt('max_parameter',                        RString, '99', min='10', max='10000')
    battle_conf.opt('monster_skill_log',                    RString, '0')
    battle_conf.opt('battle_log',                           RString, '0')
    battle_conf.opt('save_log',                             RString, '0')
    battle_conf.opt('error_log',                            RString, '1')
    battle_conf.opt('etc_log',                              RString, '1')
    battle_conf.opt('save_clothcolor',                      RString, '0')
    battle_conf.opt('undead_detect_type',                   RString, '0')
    battle_conf.opt('agi_penaly_type',                      RString, '0')
    battle_conf.opt('agi_penaly_count',                     RString, '3', min='2')
    battle_conf.opt('agi_penaly_num',                       RString, '0')
    battle_conf.opt('vit_penaly_type',                      RString, '0')
    battle_conf.opt('vit_penaly_count',                     RString, '3', min='2')
    battle_conf.opt('vit_penaly_num',                       RString, '0')
    battle_conf.opt('mob_changetarget_byskill',             RString, '0')
    battle_conf.opt('player_attack_direction_change',       RString, '1')
    battle_conf.opt('monster_attack_direction_change',      RString, '1')
    battle_conf.opt('display_delay_skill_fail',             RString, '1')
    battle_conf.opt('prevent_logout',                       RString, '1')
    battle_conf.opt('alchemist_summon_reward',              RString, '{}')
    battle_conf.opt('maximum_level',                        RString, '255')
    battle_conf.opt('drops_by_luk',                         RString, '0')
    battle_conf.opt('monsters_ignore_gm',                   RString, '')
    battle_conf.opt('multi_level_up',                       RString, '0')
    battle_conf.opt('pk_mode',                              RString, '0')
    battle_conf.opt('agi_penaly_count_lv',                  ATK, 'ATK::FLEE')
    battle_conf.opt('vit_penaly_count_lv',                  ATK, 'ATK::DEF')
    battle_conf.opt('hide_GM_session',                      RString, '0')
    battle_conf.opt('invite_request_check',                 RString, '1')
    battle_conf.opt('disp_experience',                      RString, '0')
    battle_conf.opt('hack_info_GM_level',                   RString, '60')
    battle_conf.opt('any_warp_GM_min_level',                RString, '20')
    battle_conf.opt('min_hair_style',                       RString, '0')
    battle_conf.opt('max_hair_style',                       RString, '20')
    battle_conf.opt('min_hair_color',                       RString, '0')
    battle_conf.opt('max_hair_color',                       RString, '9')
    battle_conf.opt('min_cloth_color',                      RString, '0')
    battle_conf.opt('max_cloth_color',                      RString, '4')
    battle_conf.opt('castrate_dex_scale',                   RString, '150')
    battle_conf.opt('area_size',                            RString, '14')
    battle_conf.opt('chat_lame_penalty',                    RString, '2')
    battle_conf.opt('chat_spam_threshold',                  RString, '10', min='0', max='32767')
    battle_conf.opt('chat_spam_flood',                      RString, '10', min='0', max='32767')
    battle_conf.opt('chat_spam_ban',                        RString, '1', min='0', max='32767')
    battle_conf.opt('chat_spam_warn',                       RString, '8', min='0', max='32767')
    battle_conf.opt('chat_maxline',                         RString, '255', min='1', max='512')
    battle_conf.opt('packet_spam_threshold',                RString, '2', min='0', max='32767')
    battle_conf.opt('packet_spam_flood',                    RString, '30', min='0', max='32767')
    battle_conf.opt('packet_spam_kick',                     boolean, 'true')
    battle_conf.opt('mask_ip_gms',                          boolean, 'true')
    battle_conf.opt('drop_pickup_safety_zone',              RString, '20')
    battle_conf.opt('itemheal_regeneration_factor',         RString, '1')
    battle_conf.opt('mob_splash_radius',                    RString, '-1')

    return rv

def main():
    cfg = build_config()
    cfg.dump()

if __name__ == '__main__':
    main()
