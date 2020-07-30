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

#include "gtest/gtest.h"

#include "libreport/Report.h"

#include <iostream>
#include <sstream>

namespace {

    TEST(report, simple_value_serialized_and_read_back)
    {
        report::Report expected = report::Report {
            report::Context { "session", { { "key", "value" } } },
            {
                report::Execution {
                    report::Command {
                        "/usr/bin/ls",
                        { "ls" },
                        "/home/user",
                        { { "HOME", "/home/user" }, { "PATH", "/usr/bin:/usr/local/bin" } } },
                    report::Run {
                        42 ,
                        { 12 },
                        {
                            report::Event {"started", "2020-04-04T07:13:47.027Z", std::nullopt, std::nullopt },
                            report::Event {"signaled", "2020-04-04T07:13:47.045Z", std::nullopt, { 15 } },
                            report::Event {"terminated", "2020-04-04T07:13:47.074Z", { 0 }, std::nullopt }
                        }
                    }
                },
                report::Execution {
                    report::Command {
                        "/usr/bin/ls",
                        { "ls", "-l" },
                        "/home/user",
                        { { "HOME", "/home/user" }, { "PATH", "/usr/bin:/usr/local/bin" } } },
                    report::Run {
                        43 ,
                        { },
                        {
                            report::Event {"started", "2020-04-04T07:13:47.027Z", std::nullopt, std::nullopt },
                            report::Event {"signaled", "2020-04-04T07:13:47.045Z", std::nullopt, { 17 } },
                            report::Event {"terminated", "2020-04-04T07:13:47.074Z", { 8 }, std::nullopt }
                        }
                    }
                }
            }
        };

        std::stringstream buffer;

        auto serialized = report::to_json(buffer, expected);
        EXPECT_TRUE(serialized.is_ok());

        auto deserialized = report::from_json(buffer);
        EXPECT_TRUE(deserialized.is_ok());
        deserialized.on_success([&expected](auto result) {
            EXPECT_EQ(expected, result);
        });
    }

    TEST(report, parse_failure_handled)
    {
        std::stringstream buffer;

        buffer << "this { is } wrong" << std::endl;

        auto deserialized = report::from_json(buffer);
        EXPECT_FALSE(deserialized.is_ok());
    }
}