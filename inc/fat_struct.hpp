#pragma once

#include "detail/common.hpp"

namespace kind_reflect {

namespace fat_struct {

consteval std::meta::info
member_name (std::meta::info type, std::string_view n)
{
	using namespace std::meta;
	for(auto m : nonstatic_data_members_of(type, access_context::unchecked()))
		if(identifier_of(m) == n) return m;
	detail::fail("Internal error: member '", n, "' not found in '", display_string_of(type), "'.");
}
consteval void
check_slot_types (std::meta::info kind_enum, std::meta::info tmpl)
{
	using namespace std::meta;
	for(auto e : enumerators_of(kind_enum)) {
		if(detail::is_none(e)) continue;
		auto s = detail::struct_for(tmpl, e);
		for(auto f : nonstatic_data_members_of(s, access_context::unchecked())) {
			auto t = type_of(f);
			if(is_reference_type(t) || is_const_type(t) || is_volatile_type(t))
				detail::fail("Field '", identifier_of(f), "' of '", identifier_of(s), "' has type '",
						display_string_of(t), "'; fields must not be references or cv-qualified.");
			if(!is_default_constructible_type(t))
				detail::fail("Field '", identifier_of(f), "' of '", identifier_of(s), "' has type '",
						display_string_of(t), "', which is not default-constructible.");
		}
	}
}
consteval void
check_common_names (std::meta::info tmpl)
{
	using namespace std::meta;
	for(auto m : members_of(tmpl, access_context::unchecked())) {
		if(detail::is_good(m) && identifier_of(m) == detail::common_name) {
			for(auto n : nonstatic_data_members_of(m, access_context::unchecked())) {
				if(identifier_of(n).starts_with("__fs_"))
					detail::fail("Common field '", identifier_of(n), "' in '", display_string_of(tmpl),
							"' must not start with '__fs_' (reserved for generated members).");
			}
		}
	}
}
consteval bool
ensure_struct_format (std::meta::info kind_enum, std::meta::info tmpl)
{
	detail::check_all_members(tmpl);
	detail::check_pairing(kind_enum, tmpl);
	check_slot_types(kind_enum, tmpl);
	check_common_names(tmpl);
	return true;
}


consteval std::vector<std::meta::info>
collect_slots (std::meta::info kind_enum, std::meta::info tmpl)
{
	using namespace std::meta;
	std::vector<info> slots;
	for(auto e : enumerators_of(kind_enum)) {
		if(detail::is_none(e)) continue;
		std::vector<info> seen;
		for(auto f : nonstatic_data_members_of(detail::struct_for(tmpl, e), access_context::unchecked())) {
			auto t = dealias(type_of(f));
			size_t k = 0, have = 0;
			for(auto s : seen) if(s == t) ++k;
			for(auto s : slots) if(s == t) ++have;
			seen.push_back(t);
			if(have <= k) slots.push_back(t);
		}
	}
	auto comp = [](info a, info b) { return alignment_of(a) > alignment_of(b); };
	for(auto i = slots.begin(); i != slots.end(); ++i) {
		auto insertion = std::upper_bound(slots.begin(), i, *i, comp);
		std::rotate(insertion, i, i + 1);
	}
	// stl_tempbuf.h isn't constexpr
//		std::ranges::stable_sort(slots, [](info a, info b) { return alignment_of(a) > alignment_of(b); });
	return slots;
}

template<typename KIND, typename TMPL>
consteval bool
valid ()
{
	return detail::validation_message([]{ensure_struct_format(^^KIND, ^^TMPL);}).empty();
}
template<typename KIND, typename TMPL>
consteval auto
slot_table ()
{
	using namespace std::meta;
	if (!valid<KIND, TMPL>())
		return std::define_static_array(std::vector<info>{});
	return std::define_static_array(collect_slots(^^KIND, ^^TMPL));
}
template<typename KIND, typename TMPL>
constexpr auto Slot_Types = slot_table<KIND, TMPL>();
consteval std::string_view
slot_name (size_t i)
{
	std::string s = "__fs_s_";
	std::string d;
	do {d += char('0' + i % 10); i /= 10; } while(i);
	s.append(d.rbegin(), d.rend());
	return std::define_static_string(s);
}
template<typename KIND, typename TMPL>
struct Slots_IMPL {
	struct store;
	consteval {
		using namespace std::meta;
		std::vector<info> items;
		size_t i = 0;
		for(auto t : Slot_Types<KIND, TMPL>)
			items.push_back(data_member_spec(t, {.name = slot_name(i++)}));
		size_t pos = 0;
		while(pos < Slot_Types<KIND, TMPL>.size() &&
				alignment_of(Slot_Types<KIND, TMPL>[pos]) >= alignment_of(^^KIND)) ++pos;
		items.insert(items.begin() + pos, data_member_spec(^^KIND, {.name = "__fs_kind"}));
		define_aggregate(^^store, items);
	}
};
template<typename KIND, typename TMPL>
using Slots = typename Slots_IMPL<KIND, TMPL>::store;

template<typename KIND, typename TMPL>
consteval size_t
slot_index (std::meta::info f)
{
	using namespace std::meta;
	auto t = dealias(type_of(f));
	size_t k = 0;
	for(auto g : nonstatic_data_members_of(parent_of(f), access_context::unchecked())) {
		if(g == f) break;
		if(dealias(type_of(g)) == t) ++k;
	}
	size_t seen = 0;
	for(size_t i = 0; i < Slot_Types<KIND, TMPL>.size(); ++i)
		if(Slot_Types<KIND, TMPL>[i] == t && seen++ == k) return i;
	detail::fail("Internal error: no slot for field '", identifier_of(f), "' of type '", display_string_of(t), "'.");
}
template<typename KIND, typename TMPL>
consteval std::meta::info
slot_member (std::meta::info f)
{
	return member_name(dealias(^^Slots<KIND, TMPL>), slot_name(slot_index<KIND, TMPL>(f)));
}
template<std::meta::info S, bool Const>
struct View_IMPL {
	struct store;
	consteval {
		using namespace std::meta;
		std::vector<info> items;
		for(auto f : nonstatic_data_members_of(S, access_context::unchecked())) {
			auto t = type_of(f);
			items.push_back(data_member_spec(add_lvalue_reference(Const ? add_const(t) : t), {.name = identifier_of(f)}));
		}
		define_aggregate(^^store, items);
	}
};
template<std::meta::info S, bool Const>
using View = typename View_IMPL<S, Const>::store;

template<typename KIND, typename TMPL>
struct Validate {
	static constexpr std::string_view msg = detail::validation_message([]{ensure_struct_format(^^KIND, ^^TMPL); });
	static_assert(msg.empty(), msg);
};

} // namespace fat_struct

template<typename KIND, typename TMPL>
struct Fat_Struct : fat_struct::Validate<KIND, TMPL>, Common<TMPL>, private fat_struct::Slots<KIND, TMPL> {
public:
	using Common_t = Common<TMPL>;
	using Slots_t = fat_struct::Slots<KIND, TMPL>;
	template<KIND K> struct in_place_kind_t { explicit in_place_kind_t() = default; };
	template<KIND K> static constexpr in_place_kind_t<K> in_place_kind{};

	Fat_Struct () : Common_t(), Slots_t()
	{
		set_kind(KIND::None);
	}
	template<KIND K>
	auto
	as ()
	{
		assert(kind() == K);
		return make_view<K, false>(*this);
	}
	template<KIND K>
	auto
	as () const
	{
		assert(kind() == K);
		return make_view<K, true>(*this);
	}
	template<KIND K>
	void
	emplace (auto&&... args)
	{
		static_assert(K != KIND::None);
		static constexpr auto fields = fields_of<K>();
		static_assert(sizeof...(args) == 0 || sizeof...(args) == fields.size(),
				"pass either no arguments or one per field");
		set_kind(K);
		template for(constexpr auto f : fields)
			this->[:fat_struct::slot_member<KIND, TMPL>(f):] = {};
		if constexpr(sizeof...(args) > 0)
			assign_fields<K>(std::index_sequence_for<decltype(args)...>{},
					std::forward<decltype(args)>(args)...);
	}
	void
	reset () noexcept
	{
		set_kind(KIND::None);
	}
	[[nodiscard]]
	KIND
	kind () const noexcept
	{
		return this->[:kind_member():];
	}
	[[nodiscard]]
	bool
	empty () const noexcept
	{
		return kind() == KIND::None;
	}
	bool
	operator== (Fat_Struct const& o) const
	{
		if(kind() != o.kind()) return false;
		if(!equal(static_cast<Common_t const&>(*this), static_cast<Common_t const&>(o)))
			return false;
		bool eq = true;
		with_kind(o.kind(), [&](auto M) {
			constexpr KIND k = decltype(M)::value;
			if constexpr(k != KIND::None) {
				template for(constexpr auto f : fields_of<k>())
					eq = eq && equal(this->[:fat_struct::slot_member<KIND, TMPL>(f):],
							o.[:fat_struct::slot_member<KIND, TMPL>(f):]);
			}
		});
		return eq;
	}
private:
	void
	set_kind (KIND k) noexcept
	{
		this->[:kind_member():] = k;
	}
	static consteval std::meta::info
	kind_member ()
	{
		return fat_struct::member_name(dealias(^^Slots_t), "__fs_kind");
	}
	template<KIND K>
	static consteval std::meta::info
	struct_of ()
	{
		template for(constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^KIND)))
			if constexpr([:e:] == K)
				return detail::struct_for(^^TMPL, e);
		detail::fail("Internal error: kind not found in '", display_string_of(^^KIND), "'.");
	}
	template<KIND K>
	static consteval auto
	fields_of ()
	{
		return std::define_static_array(std::meta::nonstatic_data_members_of(struct_of<K>(), std::meta::access_context::unchecked()));
	}
	template<KIND K, bool Const, typename Self>
	static auto
	make_view (Self& self)
	{
		using V = fat_struct::View<struct_of<K>(), Const>;
		static constexpr auto fields = fields_of<K>();
		return [&]<size_t... I>(std::index_sequence<I...>) {
			return V{ self.[:fat_struct::slot_member<KIND, TMPL>(fields[I]):]... };
		}(std::make_index_sequence<fields.size()>{});
	}
	template<KIND K, size_t... I>
	void
	assign_fields (std::index_sequence<I...>, auto&&... args)
	{
		static constexpr auto fields = fields_of<K>();
		auto t = std::forward_as_tuple(std::forward<decltype(args)>(args)...);
		((this->[:fat_struct::slot_member<KIND, TMPL>(fields[I]):] = std::get<I>(std::move(t))), ...);
	}

	template<typename F>
	void
	with_kind (KIND k, F&& f) const
	{
		template for(constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^KIND))) {
			if(k == [:e:]) {
				f(std::integral_constant<KIND, ([:e:])>{});
			}
		}
	}
};

} // namespace kind_reflect
