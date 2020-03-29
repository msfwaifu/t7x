#pragma once

#include "macho.hpp"

class macho_loader
{
public:
	macho_loader(const std::string& file);

private:
	void allocate_segments();
	void map_segments() const;
	void map_imports() const;
	void map_binary();

	std::string binary_data_;
	macho binary_;
	macho mapped_binary_;
};
