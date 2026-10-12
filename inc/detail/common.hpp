#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <meta>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <cassert>
#include <cstddef>

namespace kind_reflect {

namespace detail {

using namespace std::meta;

constexpr std::string_view common_name = "Kind_Reflect_Common";

template<typename... Parts>
[[noreturn]] consteval void
fail (Parts const&... parts)
{
	std::string s = "kind_reflect: ";
	((s += std::string_view(parts)), ...);
	throw std::define_static_string(s);
}
template<typename F>
consteval std::string_view
validation_message (F&& f)
{
	try {
		f();
	} catch (char const* msg) {
		return msg;
	} catch (std::meta::exception const& e) {
		return std::define_static_string(std::string_view(e.what()));
	}
	return {};
}
consteval void
check_member (info m)
{
	if(is_type(m) && is_class_type(m)) {
		if(!has_identifier(m))
			fail("'", display_string_of(parent_of(m)), "' contains an unnamed type (",
			     display_string_of(m), "). All nested types must be named.");
		if(identifier_of(m) == "None")
			fail("'", display_string_of(parent_of(m)), "' must not define a struct named 'None'; ",
			     "'None' is reserved for the empty state.");
	} else if(is_function(m)) {
		if(!is_special_member_function(m) && is_user_declared(m))
			fail("'", display_string_of(parent_of(m)), "' must not declare functions (found '",
			     display_string_of(m), "').");
	} else {
		fail("'", display_string_of(parent_of(m)), "' may only contain struct types (found '",
		     display_string_of(m), "').");
	}
}
consteval void
check_all_members (info tmpl)
{
	for(auto m : members_of(tmpl, access_context::unchecked()))
		check_member(m);
}
consteval bool
is_good (info m)
{
	return is_type(m) && is_class_type(m) && has_identifier(m);
}
consteval bool
is_none (info e)
{
	return identifier_of(e) == "None";
}
consteval void
check_pair (info e, info tmpl)
{
	for(auto m : members_of(tmpl, access_context::unchecked())) {
		if(!is_good(m)) continue;
		if(identifier_of(m) == common_name) continue;
		if(identifier_of(e) == identifier_of(m)) return;
	}
	fail("Enumerator '", identifier_of(e), "' has no corresponding struct in '", display_string_of(tmpl), "'");
}
consteval void
check_pairing (info kind_enum, info tmpl)
{
	bool has_None = false;
	for(auto e : enumerators_of(kind_enum)) {
		if(identifier_of(e) == "None") { has_None = true; continue; }
		check_pair(e, tmpl);
	}
	if(!has_None)
		fail("Enumeration '", display_string_of(kind_enum), "' must have an enumerator named 'None'.");
	for(auto m : members_of(tmpl, access_context::unchecked())) {
		if(!is_good(m) || identifier_of(m) == common_name) continue;
		bool found = false;
		for(auto e : enumerators_of(kind_enum))
			if(!is_none(e) && identifier_of(e) == identifier_of(m)) found = true;
		if(!found)
			fail("Struct '", identifier_of(m), "' in '", display_string_of(tmpl),
					"' has no corresponding enumerator in '", display_string_of(kind_enum), "'.");
	}
}
consteval info
struct_for (info tmpl, info e)
{
	for(auto m : members_of(tmpl, access_context::unchecked())) {
		if(!detail::is_good(m) || identifier_of(m) == common_name) continue;
		if(identifier_of(m) == identifier_of(e)) return m;
	}
	fail("Enumerator '", identifier_of(e), "' has no corresponding struct in '", display_string_of(tmpl), "'.");
}
consteval std::vector<info>
get_common_fields_of (info tmpl)
{
	std::vector<info> fields;
	for(auto m : members_of(tmpl, access_context::unchecked())) {
		if(!is_good(m)) continue;
		if(identifier_of(m) == common_name) {
			for(auto n : nonstatic_data_members_of(m, access_context::current()))
				fields.push_back(data_member_spec(type_of(n), {.name = identifier_of(n)}));
		}
	}
	return fields;
}

template<typename TMPL>
struct Common_IMPL {
	struct store;
	consteval {
		std::vector<info> items;
		auto common_fields = get_common_fields_of(^^TMPL);
		items.insert(items.end(), common_fields.begin(), common_fields.end());
		define_aggregate(^^store, items);
	}
};

} // namespace detail

template<typename TMPL>
using Common = detail::Common_IMPL<TMPL>::store;

template<typename A, typename B>
bool equal (A const& a, B const& b)
{
	using namespace std::meta;
	if constexpr(requires { {kind_reflect_equal(a, b)} -> std::convertible_to<bool>; }) {
		return kind_reflect_equal(a, b);
	} else if constexpr(requires { {a == b} -> std::convertible_to<bool>; }) {
		return a == b;
	} else {
		static_assert(std::is_same_v<A, B>, "no comparison between these types: provide operator== or kind_reflect_equal");
		template for(constexpr auto m : std::define_static_array(nonstatic_data_members_of(^^A, access_context::unchecked())))
			if(!equal(a.[:m:], b.[:m:])) return false;
		return true;
	}
}

} // namespace kind_reflect
