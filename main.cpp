#include "ParmeterLib.h"

std::vector<plib::p_parameter_t> parameter
        {
                plib::p_bool{ "enable", plib::has_no_argument, 'e'},
                plib::p_int{ "start", plib::has_requred_argument, 's'},
                plib::p_float{ "pi", plib::has_requred_argument, 'p'},
                plib::p_string{ "file", plib::has_requred_argument, 'f'}
        };


int main(int argc, char ** argv) {
    std::cout << "Hello, World!" << std::endl;

    bool optLoopEnd = false;
    while (not optLoopEnd) {
        plib::options_result_t optRes = plib::processOptions(argc, argv, parameter);

        for (auto p: parameter) {
            std::visit([](auto&& arg) {std::cout << arg << std::endl;}, p);
        }

        std::cout << "\nResidual arguments:";
        while (optRes.second < argc) {
            std::cout << " '" << argv[optRes.second++] << '\'';
        }
        std::cout << std::endl;

        optLoopEnd = optRes.first == plib::NoMoreArgs;
    }
}