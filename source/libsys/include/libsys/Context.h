/*  Copyright (C) 2012-2020 by László Nagy
    This file is part of Bear.

    Bear is a tool to generate compilation database for clang tooling.

    Bear is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bear is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "libresult/Result.h"

#include <filesystem>
#include <list>
#include <map>
#include <string>

namespace fs = std::filesystem;

namespace sys {

    struct Context {
        virtual ~Context() noexcept = default;

        // Query methods about the process.
        [[nodiscard]] virtual std::map<std::string, std::string> get_environment() const;

        [[nodiscard]] virtual pid_t get_pid() const;
        [[nodiscard]] virtual pid_t get_ppid() const;

        // Query methods about the system.
        [[nodiscard]] virtual rust::Result<std::string> get_confstr(int key) const;
        [[nodiscard]] virtual rust::Result<std::map<std::string, std::string>> get_uname() const;

        // Return PATH from environment and fall back to confstr default one.
        [[nodiscard]] virtual rust::Result<std::list<fs::path>> get_path() const;

        [[nodiscard]] virtual rust::Result<std::list<fs::path>> list_dir(const fs::path& path) const;
    };
}