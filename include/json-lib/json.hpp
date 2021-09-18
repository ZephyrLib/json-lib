
#ifndef HEADER_JSON_PARSER_JSON
#define HEADER_JSON_PARSER_JSON 1

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <ostream>

#include "definition.hpp"

namespace json {

    namespace details {
        template<typename x, typename deleter_t = typename std::unique_ptr<x>::deleter_type>
        struct unique_ptr : std::unique_ptr<x, deleter_t> {
            using std::unique_ptr<x>::unique_ptr;
            unique_ptr(x * o) : std::unique_ptr<x>(o) {}
        };
    }

    using definition = details::definition<details::unique_ptr, std::map, std::vector, double, std::string, std::ostream>;

    using var = typename definition::var_t;
    using object = typename definition::object_t;
    using array = typename definition::array_t;
    using primitive = typename definition::primitive_t;
    using number = typename definition::number_t;
    using literal = typename definition::literal_t;

    using V = var;
    using O = object;
    using A = array;
    using P = primitive;
    using N = number;
    using L = literal;
    using B = boolean;

    template<typename istream_t>
    inline static typename var::ptr_t parse(istream_t & s) { return details::parse<definition, istream_t>(s); }

}

#endif
