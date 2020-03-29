#pragma once

#include <mach-o/loader.h>

class macho_loader
{
public:
	macho_loader(const std::string& file);

	mach_header_64* get_mach_header();
	std::vector<load_command*> get_load_commands();

	template <typename T>
	std::vector<T*> get_load_commands(const uint32_t command_id)
	{
		std::vector<T*> result;

		const auto commands = this->get_load_commands();
		for (auto& command : commands)
		{
			if (command->cmd == command_id)
			{
				result.push_back(reinterpret_cast<T*>(command));
			}
		}

		return result;
	}

	template <typename T>
	T* get_load_command(const uint32_t command_id)
	{
		const auto commands = this->get_load_commands<T>(command_id);
		if(commands.empty())
		{
			throw std::runtime_error("Command not found");
		}

		return commands.front();
	}

private:
	std::string binary_data_;
};
