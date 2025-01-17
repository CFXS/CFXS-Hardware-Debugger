// ---------------------------------------------------------------------
// CFXS L0 ARM Debugger <https://github.com/CFXS/CFXS-L0-ARM-Debugger>
// Copyright (C) 2022 | CFXS
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>
// ---------------------------------------------------------------------
// [CFXS] //
#pragma once

namespace L0::Target {

    class MemoryRegion {
    public:
        enum class AccessPermissions : uint8_t {
            READ       = 0x01,
            WRITE      = 0x02,
            READ_WRITE = 0x03,
        };

    public:
        MemoryRegion(const char* name, uint64_t address, uint64_t size, AccessPermissions accessPermissions) :
            m_Name(name), m_Address(address), m_Size(size), m_AccessPermissions(accessPermissions) {
        }

        const char* GetName() const {
            return m_Name;
        }
        uint64_t GetAddress() const {
            return m_Address;
        }
        uint64_t GetSize() const {
            return m_Size;
        }
        AccessPermissions GetAccessPermissions() const {
            return m_AccessPermissions;
        }

    private:
        const char* m_Name;
        uint64_t m_Address;
        uint64_t m_Size;
        AccessPermissions m_AccessPermissions;
    };

} // namespace L0::Target