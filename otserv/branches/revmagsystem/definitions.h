//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// various definitions needed by most files
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////


#ifndef __definitions_h
#define __definitions_h



typedef unsigned long long uint64_t;

#if defined __WINDOWS__ || defined WIN32

#define EWOULDBLOCK WSAEWOULDBLOCK

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

#pragma warning(disable:4786) // msvc too long debug names in stl
#include <winsock.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#else

#include <stdint.h>
typedef int64_t __int64;

#endif


#endif // __definitions_h
