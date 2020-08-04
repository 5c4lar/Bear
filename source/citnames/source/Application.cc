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

#include "Application.h"
#include "Configuration.h"
#include "CompilationDatabase.h"
#include "Semantic.h"

#include "libreport/Report.h"

#include <filesystem>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace {

    bool is_exists(const fs::path& path)
    {
        std::error_code error_code;
        return fs::exists(path, error_code);
    }

    struct Arguments {
        fs::path input;
        fs::path output;
        bool append;
        bool run_check;
    };

    rust::Result<Arguments> into_arguments(const flags::Arguments& args)
    {
        auto input = args.as_string(cs::Application::INPUT);
        auto output = args.as_string(cs::Application::OUTPUT);
        auto append = args.as_bool(cs::Application::APPEND).unwrap_or(false);
        auto run_check = args.as_bool(cs::Application::RUN_CHECKS).unwrap_or(false);

        return rust::merge(input, output)
                .map<Arguments>([&append, &run_check](auto tuple) {
                    const auto& [input, output] = tuple;
                    return Arguments {
                        fs::path(input),
                        fs::path(output),
                        append,
                        run_check
                    };
                });
    }

    rust::Result<Arguments> validate(const Arguments& arguments)
    {
        if (!is_exists(arguments.input)) {
            return rust::Err(std::runtime_error(
                    fmt::format("Missing input file: {}", arguments.input)));
        }
        return rust::Ok(Arguments {
            arguments.input,
            arguments.output,
            (arguments.append && is_exists(arguments.output)),
            arguments.run_check
        });
    }
}

namespace cs {

    struct Application::State {
        Arguments arguments;
        report::ReportSerializer report_serializer;
        cs::FilterPtr filter;
        cs::Semantic semantic;
        cs::output::CompilationDatabase output;
    };

    rust::Result<Application> Application::from(const flags::Arguments& args, sys::env::Vars&& environment)
    {
        auto configuration = cfg::default_value(environment);

        auto arguments = into_arguments(args).and_then<Arguments>(&validate);
        auto filter = arguments.map<FilterPtr>([&configuration](auto arguments) {
           return make_filter(configuration.content, arguments.run_check);
        });
        auto semantic = Semantic::from(configuration.compilation);

        return rust::merge(arguments, filter, semantic)
                .map<Application::State*>([&configuration](auto tuples) {
                    const auto& [arguments, filter, semantic] = tuples;
                    // read the configuration
                    cs::output::CompilationDatabase output(configuration.format);
                    report::ReportSerializer report_serializer;
                    return new Application::State { arguments, report_serializer, filter, semantic, output };
                })
                .map<Application>([](auto impl) {
                    spdlog::debug("application object initialized.");
                    return Application { impl };
                });
    }

    rust::Result<int> Application::operator()() const
    {
        // get current compilations from the input.
        return impl_->report_serializer.from_json(impl_->arguments.input)
            .map<output::Entries>([this](auto commands) {
                spdlog::debug("commands have read. [size: {}]", commands.executions.size());
                return impl_->semantic.transform(commands);
            })
            // read back the current content and extend with the new elements.
            .and_then<output::Entries>([this](auto compilations) {
                spdlog::debug("compilation entries created. [size: {}]", compilations.size());
                return (impl_->arguments.append)
                    ? impl_->output.from_json(impl_->arguments.output.c_str())
                            .template map<output::Entries>([&compilations](auto old_entries) {
                                spdlog::debug("compilation entries have read. [size: {}]", old_entries.size());
                                return output::merge(old_entries, compilations);
                            })
                    : rust::Result<output::Entries>(rust::Ok(compilations));
            })
            // filter out entries
            .map<output::Entries>([this](auto entries) {
                output::Entries result;
                std::copy_if(entries.begin(), entries.end(),
                             std::back_inserter(result),
                             [this](auto entry) { return impl_->filter->operator()(entry); });
                return result;
            })
            // write the entries into the output file.
            .and_then<int>([this](auto compilations) {
                spdlog::debug("compilation entries to output. [size: {}]", compilations.size());
                return impl_->output.to_json(impl_->arguments.output.c_str(), compilations);
            })
            // just map to success exit code if it was successful.
            .map<int>([](auto) {
                return EXIT_SUCCESS;
            });
    }

    Application::Application(Application::State* const impl)
        : impl_(impl)
    {
    }

    Application::Application(Application&& rhs) noexcept
        : impl_(rhs.impl_)
    {
        rhs.impl_ = nullptr;
    }

    Application& Application::operator=(Application&& rhs) noexcept
    {
        if (&rhs != this) {
            delete impl_;
            impl_ = rhs.impl_;
        }
        return *this;
    }

    Application::~Application()
    {
        delete impl_;
        impl_ = nullptr;
    }
}
