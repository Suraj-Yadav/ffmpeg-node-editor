#pragma once

#include <functional>
#include <map>
#include <set>

template <typename E, typename... T>
auto contains(const std::set<T...>& c, const E& e) {
	return c.find(e) != c.end();
}

template <typename E, typename... T>
auto contains(const std::map<T...>& c, const E& e) {
	return c.find(e) != c.end();
}
template <typename C, typename E> auto contains(const C& c, const E& e) {
	return std::find(c.begin(), c.end(), e) != c.end();
}

template <class T, class E> auto erase(T& c, const E& e) {
	auto itr = std::remove(c.begin(), c.end(), e);
	if (itr != c.end()) { c.erase(itr, c.end()); }
}

class defer {
	using action = std::function<void()>;
	action _action;

   public:
	defer(const action& act) : _action(act) {}
	defer(const action&& act) : _action(act) {}

	defer(const defer& act) = delete;
	defer& operator=(const defer& act) = delete;
	defer(defer&& act) = delete;
	defer& operator=(defer&& act) = delete;
	~defer() { _action(); }
};

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>	// IWYU pragma: export
