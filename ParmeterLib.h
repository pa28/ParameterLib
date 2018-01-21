//
// Created by richard on 20/01/18.
//

#ifndef PARAMETERLIB_PARMETERLIB_H
#define PARAMETERLIB_PARMETERLIB_H

#include <algorithm>
#include <iterator>
#include <exception>
#include <string>
#include <variant>
#include <array>
#include <vector>
#include <memory>
#include <utility>
#include <iostream>
#include <sstream>

#include <cctype>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>

namespace plib {
    enum HasArg
    {
        has_no_argument = no_argument,
        has_optional_argument = optional_argument,
        has_requred_argument = required_argument,
    };


    enum ExitState
    {
        NoMoreArgs,
        PauseArgsProcessing,
    };


    using options_result_t = std::pair<ExitState, int>;


    /**
     * @brief Provide a default conversion value for type T
     *
     * @tparam T The type converted to
     * @return A default value of type T
     */
    template <class T>
    T converter() {
        return static_cast<T>(0);
    }


    /**
     * @brief Provide a default conversion value for std::string
     * @return an empty std::string
     */
    template <>
    std::string converter<std::string>() {
        return std::string{};
    }


    /**
     * @brief Provide a default conversion value for bool
     * @return true
     */
    template <>
    bool converter<bool>() {
        return true;
    }


    /**
     * @brief Provide a conversion from a null terminated char array to type T
     * @tparam T The type converted to
     * @param optarg A null terminated char array
     * @return The converted value
     */
    template <class T>
    T converter(char *optarg) {
        if (optarg == nullptr)
            return converter<T>();

        std::stringstream ss(optarg);
        T r;
        ss >> r;
        return r;
    }


    /**
     * @brief Provide a specialized converter for int
     * @param optarg The C string to convert
     * @return The intereger value after conversion
     */
    template <>
    int converter<int>(char *optarg) {
        if (optarg == nullptr)
            return 0;

        return std::stoi(std::string(optarg));
    }


    /**
     * @brief Provide a specialized converter for float
     * @param optarg The C string to convert
     * @return The intereger value after conversion
     */
    template <>
    float converter<float>(char *optarg) {
        if (optarg == nullptr)
            return 0;

        return std::stof(std::string(optarg));
    }


    /**
     * @brief Provide a specialized converter for std::string
     * @param optarg The C string to convert
     * @return A std::string created from optarg
     */
    template <>
    std::string converter<std::string>(char *optarg) {
        return std::string(optarg);
    }


    /**
     * @brief A parameter structure holding a parameter of type T
     * @tparam T
     */
    template <class T>
    struct parameter_t
    {
        std::string l_name;
        HasArg l_has_arg;
        int l_val, l_flag, l_seen;
        bool l_incomplete_conv;
        T l_value;

        parameter_t(const char * const name, HasArg has_arg, int val)
                : l_name(name), l_has_arg(has_arg), l_val(val), l_flag(), l_seen(0), l_incomplete_conv(false), l_value()
        {}

        T convert(char *optarg) const {
            return converter<T>(optarg);
        }
    };


    /*
     * Convenience types for each supported parameter type
     */
    using p_bool = plib::parameter_t<bool>;
    using p_int = plib::parameter_t<int>;
    using p_float = plib::parameter_t<float>;
    using p_string = plib::parameter_t<std::string>;


    /*
     * Convenience type for a variant which can contain any of the supported types.
     */
    using p_parameter_t = std::variant<plib::p_bool, plib::p_int, plib::p_float, plib::p_string>;


    /**
     * @brief The function that processes arguments
     * @tparam Ts A parameter pack of supported types
     * @param argc The count of arguments passed on the command line
     * @param argv The array of arguments passed on the command line
     * @param parameters A vector of p_parameter_t values specifying the program parameters
     * @return A std::pair containing the reason for return and the index to any residual program arguments
     */
    template <class... Ts>
    options_result_t processOptions(int argc, char ** argv, std::vector<p_parameter_t> & parameters)
    {
        // The return value
        options_result_t result{NoMoreArgs, argc};

        // Option parameters created from parameters to pass to getopt_long
        std::unique_ptr<struct option[]> options(new struct option[parameters.size()+1]);

        // A map of short option values back into parameters
        std::unique_ptr<char[]> small_opts(new char[parameters.size()]);
        auto so_begin = &small_opts[0];
        auto so_end = &small_opts[parameters.size()];

        // A buffer where the short option string will be built
        std::stringstream ss;

        // Iterate of parameters and visit each member to build the information needed for getopt_long
        for (size_t i = 0; i < parameters.size(); ++i) {
            std::visit([&](auto&& arg) {
                options[i].name = arg.l_name.c_str();
                options[i].has_arg = arg.l_has_arg;
                options[i].val = arg.l_val;
                options[i].flag = nullptr;
                small_opts[i] = (char)arg.l_val;

                ss << small_opts[i];
                if (arg.l_has_arg != has_no_argument)
                    ss << ':';

            },parameters[i]);
        }

        // Terminate the array of struct option values.
        options[parameters.size()].name = nullptr;
        options[parameters.size()].has_arg = 0;
        options[parameters.size()].flag = nullptr;
        options[parameters.size()].val = 0;

        int c;

        // Loop until all arguments are parsed, or one of the options requires a pause.
        while (true)
        {
            // Set option_index to the end of parameters
            auto option_index = static_cast<int>(parameters.size());

            // Call getopt_long
            c = getopt_long(argc, argv, ss.str().c_str(), options.get(), &option_index);

            // No more arguments?
            if (c == -1)
            {
                result.first = NoMoreArgs;
                result.second = optind;
                break;
            }
            else if (c > 0) {
                // This is a known argument
                if (option_index == parameters.size()) {
                    /*
                     * If option index is not set this was a short option.
                     * We have to find the option.
                     */
                    auto ptr = std::find(so_begin, so_end, (char)c);
                    if (ptr != so_end) {
                        option_index = static_cast<int>(ptr - so_begin);
                    }
                    else {
                        std::stringstream w;
                        w  << "getopt_long returned short option '"
                           << (char)c
                           << "' event though it is not defined.";

                        throw std::invalid_argument(w.str());
                    }
                }

                /*
                 * If option index is set this was a long option, or we found it in the index.
                 * Convert the argument.
                 */
                std::visit([](auto&& arg){
                    ++arg.l_seen;
                    if (arg.l_has_arg != has_no_argument) {
                        arg.l_value = arg.convert(optarg);
                    } else {
                        arg.l_value = arg.convert(nullptr);
                    }
                },parameters[option_index]);
            }
            else {
                throw std::logic_error("This implementation should not set 'flag'.");
            }
        }

        return result;
    }
}


/**
 * @brief A global namespace output operator for plib::paramert_t values
 * @tparam Ct The stream char type
 * @tparam Tt The stream char traits
 * @tparam T The output value type
 * @param os The output stream
 * @param t The output value
 * @return The output stream
 */
template <class Ct, class Tt, class T>
std::basic_ostream<Ct,Tt> & operator << (std::basic_ostream<Ct,Tt> & os, const plib::parameter_t<T> &t) {
    return os << t.l_name << " seen: " << t.l_seen << " value: " << t.l_value;
};


#endif //PARAMETERLIB_PARMETERLIB_H
