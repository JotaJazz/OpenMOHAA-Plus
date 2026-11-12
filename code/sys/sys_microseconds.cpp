/*
===========================================================================
Copyright (C) 2026 the OpenMOHAA Plus contributors

This file is part of OpenMOHAA Plus source code.

OpenMOHAA Plus source code is free software: you can redistribute it
and/or modify it under the terms of the GNU Affero General Public License
as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

OpenMOHAA Plus source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with OpenMOHAA Plus source code.  If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/

// sys_microseconds.cpp -- Added in OpenMOHAA Plus: monotonic microsecond clock for profiling

#include <chrono>

extern "C" long long Sys_Microseconds(void)
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}
