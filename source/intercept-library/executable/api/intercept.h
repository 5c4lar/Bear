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

namespace pear {
    namespace flag {

        constexpr char HELP[] = "--help";

        constexpr char WRAPPER_CC[] = "--session-cc-wrapper";
        constexpr char WRAPPER_CXX[] = "--session-c++-wrapper";

        constexpr char VERBOSE[] = "--session-verbose";
        constexpr char DESTINATION[] = "--session-destination";
        constexpr char LIBRARY[] = "--session-library";
        constexpr char PATH[] = "--exec-path";
        constexpr char FILE[] = "--exec-file";
        constexpr char SEARCH_PATH[] = "--exec-search-path";
        constexpr char COMMAND[] = "--";

    }
}
