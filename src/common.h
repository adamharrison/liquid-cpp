#ifndef COMMON_H
#define COMMON_H

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <cassert>

namespace Liquid {
    template<typename T>
    using vector = std::vector<T>;
    using string = std::string;
    template<typename S, typename T>
    using unordered_map = std::unordered_map<S,T>;
    template<typename T>
    using unique_ptr = std::unique_ptr<T>;
    template<typename T>
    using shared_ptr = std::shared_ptr<T>;
    using std::make_unique;
    using std::move;
}

#endif
