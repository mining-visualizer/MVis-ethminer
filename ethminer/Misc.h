
#pragma once

/*
This file is part of mvis-ethereum.

mvis-ethereum is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

mvis-ethereum is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with mvis-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream> 
#include <iostream> 
#include <boost/filesystem.hpp>

bool getlineEx(std::istream& is, std::string& t);
boost::filesystem::path getAppDataFolder(void);
std::string& LowerCase(std::string& _s);
bool fileExists(std::string _path);