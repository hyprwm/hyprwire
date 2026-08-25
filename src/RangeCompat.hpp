#pragma once

#include <vector>
#include <iterator>
#include <utility>

template <typename T, typename Range>
void append_range_compat(std::vector<T>& dest, Range&& range) {
    dest.insert(dest.end(), std::begin(range), std::end(range));
}
