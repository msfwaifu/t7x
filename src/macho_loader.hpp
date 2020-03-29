#pragma once

#include "macho.hpp"

class macho_loader
{
public:
	macho_loader(const std::string& file);
	~macho_loader();
};
