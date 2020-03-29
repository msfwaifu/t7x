#include <std_include.hpp>
#include "macho.hpp"

#pragma warning(push)
#pragma warning(disable: 6297)

static uint64_t uleb128(const uint8_t*& p)
{
	uint64_t r = 0;
	int s = 0;
	do
	{
		r |= static_cast<uint64_t>(*p & 0x7f) << s;
		s += 7;
	}
	while (*p++ >= 0x80);
	return r;
}

static int64_t sleb128(const uint8_t*& p)
{
	int64_t r = 0;
	int s = 0;
	for (;;)
	{
		uint8_t b = *p++;
		if (b < 0x80)
		{
			if (b & 0x40)
			{
				r -= (0x80 - b) << s;
			}
			else
			{
				r |= (b & 0x3f) << s;
			}
			break;
		}
		r |= (b & 0x7f) << s;
		s += 7;
	}
	return r;
}

#pragma warning(pop)

class bind_parser
{
public:
	bind_parser(std::vector<macho::bind>& bind, const bool is_weak)
		: binds_(bind)
	{
		bind_.is_weak = is_weak;
		bind_.type = BIND_TYPE_POINTER;
	}

	static void readBinds(std::vector<macho::bind>& bind, const uint8_t* p, const uint8_t* end, bool is_weak)
	{
		bind_parser parser(bind, is_weak);
		while (p < end)
		{
			parser.readBindOp(p);
		}
	}

	void readBindOp(const uint8_t*& p)
	{
		const uint8_t op = *p & BIND_OPCODE_MASK;
		const uint8_t imm = *p & BIND_IMMEDIATE_MASK;
		p++;

		switch (op)
		{
		case BIND_OPCODE_DONE:
			break;

		case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
			bind_.ordinal = imm;
			break;

		case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
			bind_.ordinal = static_cast<uint8_t>(uleb128(p));
			break;

		case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
			if (imm == 0)
			{
				bind_.ordinal = 0;
			}
			else
			{
				bind_.ordinal = BIND_OPCODE_MASK | imm;
			}
			break;

		case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
			bind_.name = reinterpret_cast<const char*>(p);
			p += strlen(bind_.name) + 1;
			break;

		case BIND_OPCODE_SET_TYPE_IMM:
			bind_.type = imm;
			break;

		case BIND_OPCODE_SET_ADDEND_SLEB:
			bind_.addend = sleb128(p);
			break;

		case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
			bind_.seg_index = imm;
			bind_.seg_offset = uleb128(p);
			break;

		case BIND_OPCODE_ADD_ADDR_ULEB:
			bind_.seg_offset += uleb128(p);
			break;

		case BIND_OPCODE_DO_BIND:
			addBind();
			break;

		case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
			addBind();
			bind_.seg_offset += uleb128(p);
			break;

		case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
			addBind();
			bind_.seg_offset += imm * sizeof(void*);
			break;

		case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
			{
				const auto count = uleb128(p);
				const auto skip = uleb128(p);

				for (uint64_t i = 0; i < count; i++)
				{
					addBind();
					bind_.seg_offset += skip;
				}
				break;
			}

		default:
			fprintf(stderr, "unknown op: %x\n", op);
		}
	}

	void addBind()
	{
		macho::bind bind{};
		binds_.push_back(bind_);

		bind_.seg_offset += sizeof(void*);
	}

private:
	std::vector<macho::bind>& binds_;
	macho::bind bind_;
};

macho::macho(void* pointer) : pointer_(pointer)
{
}

macho::operator bool() const
{
	const auto* header = this->get_mach_header();
	return header->magic == MH_MAGIC_64 &&
		header->cputype == CPU_TYPE_X86_64 &&
		header->filetype == MH_EXECUTE; // Only executables for now
}

void* macho::get_pointer() const
{
	return this->pointer_;
}

void* macho::get_entry_point() const
{
	const auto entry_point = this->get_load_command<entry_point_command>(LC_MAIN);
	return this->get_rva<void>(entry_point->entryoff);
}

mach_header_64* macho::get_mach_header() const
{
	return this->get_rva<mach_header_64>(0);
}

std::vector<load_command*> macho::get_load_commands() const
{
	const auto* header = this->get_mach_header();

	std::vector<load_command*> commands;
	commands.reserve(header->ncmds);

	auto offset = sizeof(*header);

	for (size_t i = 0; i < header->ncmds; ++i)
	{
		auto* command = this->get_rva<load_command>(offset);
		commands.push_back(command);
		offset += command->cmdsize;
	}

	return commands;
}

std::vector<macho::bind> macho::get_binds() const
{
	std::vector<bind> binds;

	const auto dyld_info = this->get_load_command<dyld_info_command>(LC_DYLD_INFO_ONLY);

	{
		const uint8_t* p = this->get_rva<uint8_t>(dyld_info->bind_off);
		const uint8_t* end = p + dyld_info->bind_size;
		bind_parser::readBinds(binds, p, end, false);
	}

	{
		const uint8_t* p = this->get_rva<uint8_t>(dyld_info->lazy_bind_off);
		const uint8_t* end = p + dyld_info->lazy_bind_size;
		bind_parser::readBinds(binds, p, end, false);
	}

	{
		const uint8_t* p = this->get_rva<uint8_t>(dyld_info->weak_bind_off);
		const uint8_t* end = p + dyld_info->weak_bind_size;
		bind_parser::readBinds(binds, p, end, true);
	}

	return binds;
}
