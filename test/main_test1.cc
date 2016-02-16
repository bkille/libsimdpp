/*  Copyright (C) 2012  Povilas Kanapickas <povilas@radix.lt>

    Distributed under the Boost Software License, Version 1.0.
        (See accompanying file LICENSE_1_0.txt or copy at
            http://www.boost.org/LICENSE_1_0.txt)
*/

#include "utils/test_results.h"
#include "insn/tests.h"
#include <simdpp/simd.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <vector>

#if __linux__
#include <simdpp/dispatch/get_arch_linux_cpuinfo.h>
#endif
#if SIMDPP_X86
#include <simdpp/dispatch/get_arch_raw_cpuid.h>
#endif

inline simdpp::Arch get_arch(bool is_simulator)
{
    (void) is_simulator;
    std::vector<simdpp::Arch> supported_archs;
#if __linux__
    if (!is_simulator)
        supported_archs.push_back(simdpp::get_arch_linux_cpuinfo());
#endif
#if SIMDPP_X86
    supported_archs.push_back(simdpp::get_arch_raw_cpuid());
#endif
    if (supported_archs.empty()) {
        std::cerr << "No architecture information could be retrieved. Testing not supported\n";
        std::exit(EXIT_FAILURE);
    }

    simdpp::Arch result = supported_archs.front();
    for (unsigned i = 1; i < supported_archs.size(); ++i) {
        if (supported_archs[i] != result) {
            std::cerr << "Different CPU architecture evaluators return different results\n"
                      << std::hex << (unsigned)result << " " << i << " "
                      << std::hex << (unsigned)supported_archs[i] << "\n";
            std::exit(EXIT_FAILURE);
        }
    }
    return result;
}

void parse_args(int argc, char* argv[], bool& is_simulator)
{
    is_simulator = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--simulator") == 0)
            is_simulator = true;
    }
}

/*  We test libsimdpp by comparing the results of the same computations done in
    different 'architectures'. That is, we build a list of results for each
    instruction set available plus the 'null' instruction set (simple,
    non-vectorized code). All tests are generated from the same source code,
    thus any differencies are likely to be caused by bugs in the library.
*/
int main(int argc, char* argv[])
{
    bool is_simulator = false;
    parse_args(argc, argv, is_simulator);

    simdpp::Arch current_arch = get_arch(is_simulator);

    std::ostream& err = std::cerr;
    const auto& arch_list = get_test_archs();
    auto null_arch = std::find_if(arch_list.begin(), arch_list.end(),
                                  [](const simdpp::detail::FnVersion& a) -> bool
                                  {
                                      return a.arch_name && std::strcmp(a.arch_name, "arch_null") == 0;
                                  });

    if (null_arch == arch_list.end()) {
        std::cerr << "FATAL: NULL architecture not defined\n";
        return EXIT_FAILURE;
    }

    TestResults null_results(null_arch->arch_name);
    reinterpret_cast<void(*)(TestResults&)>(null_arch->fun_ptr)(null_results);

    std::cout << "Num results: " << null_results.num_results() << '\n';

    bool ok = true;

    for (auto it = arch_list.begin(); it != arch_list.end(); it++) {
        if (it->fun_ptr == NULL || it == null_arch) {
            continue;
        }

        if (!simdpp::test_arch_subset(current_arch, it->needed_arch)) {
            std::cout << "Not testing: " << it->arch_name << std::endl;
            continue;
        }
        std::cout << "Testing: " << it->arch_name << std::endl;

        TestResults results(it->arch_name);
        reinterpret_cast<void(*)(TestResults&)>(it->fun_ptr)(results);

        if (!test_equal(null_results, results, err)) {
            ok = false;
        }
    }

    if (!ok) {
        return EXIT_FAILURE;
    }
}
