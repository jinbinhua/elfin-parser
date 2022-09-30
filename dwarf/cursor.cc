// Copyright (c) 2013 Austin T. Clements. All rights reserved.
// Use of this source code is governed by an MIT license
// that can be found in the LICENSE file.

#include "internal.hh"
#include "../elf/sig_handler.hh"

#include <stdexcept>
#include <cstring>

using namespace std;

DWARFPP_BEGIN_NAMESPACE

int64_t
cursor::sleb128()
{
        // Appendix C
        uint64_t result = 0;
        unsigned shift = 0;
        while (pos < sec->end) {
                uint8_t byte = *(uint8_t*)(pos++);
                result |= (uint64_t)(byte & 0x7f) << shift;
                shift += 7;
                if ((byte & 0x80) == 0) {
                        if (shift < sizeof(result)*8 && (byte & 0x40))
                                result |= -((uint64_t)1 << shift);
                        return result;
                }
        }
        underflow();
        return 0;
}

shared_ptr<section>
cursor::subsection()
{
        // Section 7.4
        const char *begin = pos;
        section_length length = fixed<uword>();
        format fmt;
        if (length < 0xfffffff0) {
                fmt = format::dwarf32;
                length += sizeof(uword);
        } else if (length == 0xffffffff) {
                length = fixed<uint64_t>();
                fmt = format::dwarf64;
                length += sizeof(uword) + sizeof(uint64_t);
        } else {
                throw format_error("initial length has reserved value");
        }
        pos = begin + length;
        return make_shared<section>(sec->type, begin, length, sec->ord, fmt);
}

void
cursor::skip_initial_length()
{
        switch (sec->fmt) {
        case format::dwarf32:
                pos += sizeof(uword);
                break;
        case format::dwarf64:
                pos += sizeof(uword) + sizeof(uint64_t);
                break;
        default:
                throw logic_error("cannot skip initial length with unknown format");
        }
}

section_offset
cursor::offset()
{
        switch (sec->fmt) {
        case format::dwarf32:
                return fixed<uint32_t>();
        case format::dwarf64:
                return fixed<uint64_t>();
        default:
                throw logic_error("cannot read offset with unknown format");
        }
}

void
cursor::string(std::string &out)
{
        size_t size;
        const char *p = this->cstr(&size);
        out.resize(size);
        memmove(&out.front(), p, size);
}

const char *
cursor::cstr(size_t *size_out)
{
        // Scan string size
        const char *p = pos;
        while (pos < sec->end && *pos)
                pos++;
        if (pos == sec->end)
                throw format_error("unterminated string");
        if (size_out)
                *size_out = pos - p;
        pos++;
        return p;
}

void
cursor::skip_form(DW_FORM form)
{
	    signal(SIGABRT, sigabrt_handler);
		signal(SIGSEGV, sigsegv_handler);
        section_offset tmp;

        // Section 7.5.4
        switch (form) {
        case DW_FORM::addr:
                pos += sec->addr_size;
                break;
        case DW_FORM::sec_offset:
        case DW_FORM::ref_addr:
        case DW_FORM::strp:
		case DW_FORM::line_strp:
		case DW_FORM::strp_sup:
                switch (sec->fmt) {
                case format::dwarf32:
                        pos += 4;
                        break;
                case format::dwarf64:
                        pos += 8;
                        break;
                case format::unknown:
                        throw logic_error("cannot read form with unknown format");
                }
                break;

                // size+data forms
        case DW_FORM::block1:
                tmp = fixed<ubyte>();
                pos += tmp;
                break;
        case DW_FORM::block2:
                tmp = fixed<uhalf>();
                pos += tmp;
                break;
        case DW_FORM::block4:
                tmp = fixed<uword>();
                pos += tmp;
                break;
        case DW_FORM::block:
        case DW_FORM::exprloc:
                tmp = uleb128();
                pos += tmp;
                break;

                // fixed-length forms
        case DW_FORM::flag_present:
                break;
        case DW_FORM::flag:
        case DW_FORM::data1:
        case DW_FORM::ref1:
		case DW_FORM::strx1:
		case DW_FORM::addrx1:
                pos += 1;
                break;
        case DW_FORM::data2:
        case DW_FORM::ref2:
		case DW_FORM::strx2:
		case DW_FORM::addrx2:
                pos += 2;
                break;
		case DW_FORM::strx3:
		case DW_FORM::addrx3:
                pos += 3;
                break;
        case DW_FORM::data4:
        case DW_FORM::ref4:
		case DW_FORM::strx4:
		case DW_FORM::addrx4:
		case DW_FORM::ref_sup4:
                pos += 4;
                break;
        case DW_FORM::data8:
        case DW_FORM::ref_sig8:
		case DW_FORM::ref_sup8:
                pos += 8;
                break;
		case DW_FORM::data16:
                pos += 16;
                break;

                // variable-length forms
        case DW_FORM::sdata:
        case DW_FORM::udata:
        case DW_FORM::ref_udata:
		case DW_FORM::strx:
		case DW_FORM::addrx:
		case DW_FORM::implicit_const:
		case DW_FORM::loclistx:
		case DW_FORM::rnglistx:
                while (pos < sec->end && (*(uint8_t*)pos & 0x80))
                        pos++;
                pos++;
                break;
        case DW_FORM::string:
                while (pos < sec->end && *pos)
                        pos++;
                pos++;
                break;

        case DW_FORM::indirect:
                skip_form((DW_FORM)uleb128());
                break;

        default:
                throw format_error("unknown form " + to_string(form));
        }
}

void
cursor::underflow()
{
        throw underflow_error("cannot read past end of DWARF section");
}

const char *dwarf_cursor::cstr(DW_FORM form)
{
	switch (form) {
	case DW_FORM::string:
	        return cstr();
	case DW_FORM::strp:
	        return cursor(m_dwarf.get_section(section_type::str), offset()).pos;
	case DW_FORM::line_strp:
	        return cursor(m_dwarf.get_section(section_type::line_str), offset()).pos;
	case DW_FORM::strp_sup:
	case DW_FORM::strx:
	case DW_FORM::strx1:
	case DW_FORM::strx2:
	case DW_FORM::strx4:
	        throw value_type_mismatch("not implemented for from " + to_string(form));
	default:
	        throw value_type_mismatch("cannot read " + to_string(form) + " as string");
	}
}

DWARFPP_END_NAMESPACE
