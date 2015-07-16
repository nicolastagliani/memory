// Copyright (C) 2015 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "node_size_debugger.hpp"

const char* exe_name = "foonathan_memory_node_size_debugger";
std::string exe_spaces(std::strlen(exe_name), ' ');

struct simple_serializer
{
    std::ostream &out;

    void prefix() const {}

    void operator()(const debug_result &result) const
    {
        out << result.container_name << ":\n";
        for (auto pair : result.node_sizes)
            out << '\t' << pair.first << '=' << pair.second << '\n';
    }

    void suffix() const {}
};

struct verbose_serializer
{
    std::ostream &out;

    void prefix() const {}

    void operator()(const debug_result &result) const
    {
        out << "For container '" << result.container_name << "':\n";
        for (auto pair : result.node_sizes)
            out << '\t' << "With an alignment of " << std::setw(2) << pair.first
                << " is the base node size " << std::setw(2) << pair.second << ".\n";
    }

    void suffix() const {}
};

struct code_serializer
{
    std::ostream &out;
    std::size_t tab_width;

    void prefix() const
    {
        out << "// The following section was autogenerated by " << exe_name << '\n';
        out << "//=== BEGIN AUTOGENERATED SECTION ===//\n\n";
    }

    void operator()(const debug_result &result) const
    {
        /* namespace detail
         * {
         *      template <std::size_t Alignment>
         *      struct <name>_node_size;
         *
         *      template <>
         *      struct <name>_node_size<I>
         *      : std::integral_constant<std::size_t, I_base_size>
         *      {};
         *
         *      ...
         * } // namespace detail
         *
         * template <typename T>
         * struct <name>_node_size
         * : std::integral_constant<std::size_t,
         *    detail::<name>_node_size<FOONATHAN_ALIGNOF(T)>::value + sizeof(T)>
         * {};
         */
        auto newline = "\n";
        out << "namespace detail" << newline << '{' << newline
            << tab() << "template <std::size_t Alignment>" << newline
            << tab() << "struct " << struct_name(result.container_name) << ';' << newline;
        for (auto pair : result.node_sizes)
            out << newline
                << tab() << "template <>" << newline
                << tab() << "struct " << struct_name(result.container_name) <<  '<' << pair.first << '>' << newline
                << tab() << ": std::integral_constant<std::size_t, " << pair.second << '>' << newline
                << tab() << "{};" << newline;
        out << "} // namespace detail" << newline << newline
            << "template <typename T>" << newline
            << "struct " << struct_name(result.container_name) << newline
            << ": std::integral_constant<std::size_t," << newline
            << "       detail::" << struct_name(result.container_name) << '<' << alignment() << ">::value + sizeof(T)>" << newline
            << "{};" << newline << newline;
    }

    void suffix() const
    {
        out << "//=== END AUTOGENERATED SECTION ===//\n";
    }

    std::string tab() const
    {
        if (tab_width == 0u)
            return "\t";
        return std::string(tab_width, ' ');
    }

    std::string struct_name(const char *container_name) const
    {
        return container_name + std::string("_node_size");
    }

    std::string alignment() const
    {
        return "FOONATHAN_ALIGNOF(T)";
    }
};

using debuggers = std::tuple<debug_forward_list, debug_list,
                             debug_set, debug_multiset, debug_unordered_set, debug_unordered_multiset,
                             debug_map, debug_multimap, debug_unordered_map, debug_unordered_multimap>;

template <class Debugger, class Serializer>
void serialize_single(const Serializer &serializer)
{
    serializer.prefix();
    serializer(debug(Debugger{}));
    serializer.suffix();
}

template <class Serializer, typename ... Debuggers>
void serialize_impl(const Serializer &serializer, std::tuple<Debuggers...>)
{
    int dummy[] = {(serializer(debug(Debuggers{})), 0)...};
    (void)dummy;
}

template <class Serializer>
void serialize(const Serializer &serializer)
{
    serializer.prefix();
    serialize_impl(serializer, debuggers{});
    serializer.suffix();
}

void print_help(std::ostream &out)
{
    out << "Usage: " << exe_name << " [--version][--help]\n";
    out << "       "  << exe_spaces << " [--simple][--verbose]\n";
    out << "       "  << exe_spaces << " [--code [-t digit] [outputfile]]\n";
    out << "Obtains information about the internal node sizes of the STL containers.\n";
    out << '\n';
    out << "   --simple\tprints node sizes in the form 'alignment=base-node-size'\n";
    out << "   --verbose\tprints node sizes in a more verbose form\n";
    out << "   --code\tgenerates C++ code to obtain the node size\n";
    out << "   --help\tdisplay this help and exit\n";
    out << "   --version\toutput version information and exit\n";
    out << '\n';
    out << "Options for code generation: \n";
    out << "   -t\tfollowed by single digit specifying tab width, 0 uses '\\t'\n";
    out << '\n';
    out << "The base node size is the size of the node without the storage for the value type.\n"
        << "Add 'sizeof(value_type)' to the base node size for the appropriate alignment to get the whole size.\n";
    out << "With no options prints base node sizes of all containers in a simple manner.\n";
}

void print_version(std::ostream &out)
{
    out << exe_name << " version " << VERSION << '\n';
}

int print_invalid_option(std::ostream &out, const char *option)
{
    out << exe_name << ": invalid option -- '";
    while (*option == '-')
        ++option;
    out << option << "'\n";
    out << "Try '" << exe_name << " --help' for more information.\n";
    return 2;
}

int print_invalid_argument(std::ostream &out, const char *option)
{
    out << exe_name << ": invalid argument for option -- '" << option << "'\n";
    out << "Try '" << exe_name << " --help' for more information.\n";
    return 2;
}

int main(int argc, char *argv[])
{
    if (argc <= 1 || argv[1] == std::string("--simple"))
        serialize(simple_serializer{std::cout});
    else if (argv[1] == std::string("--verbose"))
        serialize(verbose_serializer{std::cout});
    else if (argv[1] == std::string("--code"))
    {
        std::size_t tab_width = 4u;

        std::ofstream file;
        std::ostream out(std::cout.rdbuf());

        for (auto cur = &argv[2]; *cur; ++cur)
        {
            if (*cur == std::string("-t"))
            {
                ++cur;
                if (*cur && std::isdigit(cur[0][0]) && !cur[0][1])
                    tab_width = std::size_t(cur[0][0] - '0');
                else
                    return print_invalid_argument(std::cerr, "-t");
            }
            else if (!file.is_open())
            {
                file.open(*cur);
                if (!file.is_open())
                    return print_invalid_argument(std::cerr, "outputfile");
                out.rdbuf(file.rdbuf());
            }
            else
                return print_invalid_argument(std::cerr, "--code");
        }

        code_serializer serializer{out, tab_width};
        serialize(serializer);
    }
    else if (argv[1] == std::string("--help"))
        print_help(std::cout);
    else if (argv[1] == std::string("--version"))
        print_version(std::cout);
    else
        return print_invalid_option(std::cerr, argv[1]);
}
