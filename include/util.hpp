#pragma once

#include <algorithm>
#include <type_traits>

template <class T, class E> auto contains(T& t, E const& e) {
	if constexpr (requires { t.find(e); })
		return t.find(e) != t.cend();
	else
		return std::find(t.cbegin(), t.cend(), e) != t.cend();
}
