#pragma once

#include "detail/common.hpp"

namespace kind_reflect {

namespace tagged_union {

consteval size_t
num_kinds (std::meta::info kind_enum) { return std::meta::enumerators_of(kind_enum).size(); }
consteval std::vector<std::meta::info>
name_aliases_of (std::meta::info m)
{
	using namespace std::meta;
	std::vector<info> out;
	for(auto x : members_of(m, std::meta::access_context::unchecked()))
		if(is_type_alias(x)) out.push_back(x);
	return out;
}
consteval std::vector<std::string_view>
compute_names (std::meta::info kind_enum, std::meta::info tmpl)
{
	using namespace std::meta;
	std::vector<std::string_view> out{};
	size_t i = 0;
	auto es = enumerators_of(kind_enum);
	for(auto e : es) {
		if(detail::is_none(e)) {
			out.push_back(std::string_view{});
			continue;
		}
		auto m = detail::struct_for(tmpl, e);
		auto fields = nonstatic_data_members_of(m, access_context::unchecked());
		auto aliases = name_aliases_of(m);
		if(aliases.size() > 1)
			detail::fail("Struct '", identifier_of(m), "' declares more than one name alias; at most one is allowed.");
		if(fields.empty()) {
			out.push_back(std::string_view{});
			continue;
		}
		if(aliases.size() == 1)
			out.push_back(identifier_of(aliases[0]));
		else if(fields.size() == 1)
			out.push_back(identifier_of(fields[0]));
		else
			detail::fail("Struct '", identifier_of(m), "' has more than one field and needs a name alias, e.g. 'using name = void;'.");
	}
	for(size_t I = 0; I < i; ++I) {
		for(size_t K = 0; K < I; ++K) {
			if(!out[K].empty() && out[I] == out[K])
				detail::fail("Kinds '", identifier_of(es[K]), "' and '", identifier_of(es[I]),
	             "' both produce the union member name '", out[I], "'; give one of them a name alias.");
		}
	}
	return out;
}

consteval bool
ensure_struct_format (std::meta::info kind_enum, std::meta::info tmpl)
{
	detail::check_all_members(tmpl);
	detail::check_pairing(kind_enum, tmpl);
	(void)compute_names(kind_enum, tmpl);
	return true;
}

struct Slot { bool present; std::meta::info type; };

template<typename KIND, typename TMPL>
consteval bool
valid ()
{
	return detail::validation_message([]{ensure_struct_format(^^KIND, ^^TMPL);}).empty();
}
template<typename KIND, typename TMPL>
consteval auto
gen_slots () 
{
	using namespace std::meta;
	std::array<Slot, num_kinds(^^KIND)> out{};
	if(!valid<KIND, TMPL>()) return out;
	size_t i = 0;
	for(auto e : enumerators_of(^^KIND)) {
		if(detail::is_none(e)) { out[i++] = Slot{false, {}}; continue; }
		auto m = detail::struct_for(^^TMPL, e);
		auto fields = nonstatic_data_members_of(m, access_context::unchecked());
		auto aliases = name_aliases_of(m);
		if(fields.empty()) {
			out[i++] = Slot{false, {}};
		} else if(aliases.size() == 1) {
			out[i++] = Slot{true, m};
		} else if(fields.size() == 1) {
			out[i++] = Slot{true, type_of(fields[0])};
		} else {
			out[i++] = Slot{true, m};
		}
	}
	return out;
}
template<typename KIND, typename TMPL>
consteval auto
gen_names ()
{
	using namespace std::meta;
	std::array<std::string_view, num_kinds(^^KIND)> out{};
	if(!valid<KIND, TMPL>()) return out;
	auto v = compute_names(^^KIND, ^^TMPL);
	std::copy(v.begin(), v.end(), out.begin());
	return out;
}
template<typename KIND, typename TMPL> constexpr auto Union_Slots = gen_slots<KIND,TMPL>();
template<typename KIND, typename TMPL> constexpr auto Union_Names = gen_names<KIND,TMPL>();
template<typename KIND, typename TMPL>
consteval Slot
find_slot (std::meta::info enumerator)
{
	using namespace std::meta;
	size_t n = 0;
	for(auto e : enumerators_of(^^KIND)) {
		if(e == enumerator)
			return Union_Slots<KIND,TMPL>[n];
		++n;
	}
	detail::fail("Internal error: enumerator '", identifier_of(enumerator), "' not found in '", display_string_of(^^KIND), "'.");
}
template<typename KIND, typename TMPL>
consteval std::string_view
find_name (std::meta::info enumerator)
{
	using namespace std::meta;
	size_t n = 0;
	for(auto e : enumerators_of(^^KIND)) {
		if(e == enumerator)
			return Union_Names<KIND,TMPL>[n];
		++n;
	}
	detail::fail("Internal error: enumerator '", identifier_of(enumerator), "' not found in '", display_string_of(^^KIND), "'.");
}
template<typename KIND, typename TMPL>
consteval std::meta::info
active_member (std::meta::info store, std::meta::info enumerator)
{
	using namespace std::meta;
	auto s = find_slot<KIND, TMPL>(enumerator);
	auto n = find_name<KIND, TMPL>(enumerator);
	if(!s.present) return info{};
	for(auto m : nonstatic_data_members_of(store, access_context::unchecked()))
		if(identifier_of(m) == n) return m;
	detail::fail("Internal error: union member '", n, "' missing from '", display_string_of(store), "'.");
}
enum class Op { Copy_Construct, Copy_Assign, Move_Construct, Move_Assign, Nothrow_Move_Construct, Nothrow_Move_Assign };
consteval bool
check (std::meta::info t, Op op)
{
	using namespace std::meta;
	switch(op) {
	case Op::Copy_Construct: return is_copy_constructible_type(t);
	case Op::Copy_Assign: return is_copy_assignable_type(t);
	case Op::Move_Construct: return is_move_constructible_type(t);
	case Op::Move_Assign: return is_move_assignable_type(t);
	case Op::Nothrow_Move_Construct: return is_nothrow_move_constructible_type(t);
	case Op::Nothrow_Move_Assign: return is_nothrow_move_assignable_type(t);
	}
	return false;
}
consteval bool
all_support (std::meta::info store, std::meta::info common, Op op)
{
	using namespace std::meta;
	for(auto m : nonstatic_data_members_of(store, access_context::unchecked()))
		if(!check(type_of(m), op)) return false;
	for(auto m : nonstatic_data_members_of(common, access_context::unchecked()))
		if(!check(type_of(m), op)) return false;
	return true;
}

template<typename KIND, typename TMPL>
struct Union {
	union store;
	consteval {
		using namespace std::meta;
		std::vector<info> items;
		for(size_t i = 0; i < Union_Slots<KIND, TMPL>.size(); ++i) {
			auto s = Union_Slots<KIND, TMPL>[i];
			auto n = Union_Names<KIND, TMPL>[i];
			if(s.present)
				items.push_back(data_member_spec(s.type, {.name = n}));
		}
		define_aggregate(^^store, items);
	}
	union { store as; };
	Union () noexcept {}
	~Union () {}
};

template<typename KIND, typename TMPL>
struct Validate {
	static constexpr std::string_view msg = detail::validation_message([]{ensure_struct_format(^^KIND, ^^TMPL); });
	static_assert(msg.empty(), msg);
};

} // namespace tagged_union

template<typename KIND, typename TMPL>
struct Tagged_Union : tagged_union::Validate<KIND, TMPL>, Common<TMPL>, tagged_union::Union<KIND, TMPL> {
public:
	using Common_t = Common<TMPL>;
	using Store = typename tagged_union::Union<KIND, TMPL>::store;
	static constexpr bool can_copy						= tagged_union::all_support(^^Store, ^^Common_t, tagged_union::Op::Copy_Construct);
	static constexpr bool can_copy_assign			= tagged_union::all_support(^^Store, ^^Common_t, tagged_union::Op::Copy_Assign);
	static constexpr bool can_move						= tagged_union::all_support(^^Store, ^^Common_t, tagged_union::Op::Move_Construct);
	static constexpr bool can_move_assign			= tagged_union::all_support(^^Store, ^^Common_t, tagged_union::Op::Move_Assign);
	static constexpr bool nothrow_move				= tagged_union::all_support(^^Store, ^^Common_t, tagged_union::Op::Nothrow_Move_Construct);
	static constexpr bool nothrow_move_assign	= tagged_union::all_support(^^Store, ^^Common_t, tagged_union::Op::Nothrow_Move_Assign);
	static constexpr bool swappable = can_move && can_move_assign && nothrow_move && nothrow_move_assign;
	template<KIND K> struct in_place_kind_t { explicit in_place_kind_t() = default; };
	template<KIND K> static constexpr in_place_kind_t<K> in_place_kind{};

	Tagged_Union () : Common_t(), kind_(KIND::None) {}
	template<KIND K>
	Tagged_Union (in_place_kind_t<K>, auto&&... args) : Common_t(), kind_(KIND::None)
	{
		construct_as<K>(std::forward<decltype(args)>(args)...);
		kind_ = K;
	}
	Tagged_Union (Tagged_Union const& o) requires can_copy
		: Common_t(static_cast<Common_t const&>(o)), kind_(KIND::None)
	{
		copy_construct_active(o);
		kind_ = o.kind_;
	}
	Tagged_Union (Tagged_Union&& o) noexcept(nothrow_move) requires can_move
		: Common_t(std::move(static_cast<Common_t&>(o))), kind_(KIND::None)
	{
		move_construct_active(o);
		kind_ = o.kind_;
		o.reset();
	}
	Tagged_Union&
	operator= (Tagged_Union const& o) requires (can_copy && can_copy_assign)
	{
		if(this == &o) return *this;
		reset();
		Common_t::operator=(static_cast<Common_t const&>(o));
		copy_construct_active(o);
		kind_ = o.kind_;
		return *this;
	}
	Tagged_Union&
	operator= (Tagged_Union&& o) noexcept requires (swappable)
	{
		swap(o);
		return *this;
	}
	Tagged_Union&
	operator= (Tagged_Union&& o) requires (can_move && can_move_assign && !nothrow_move)
	{
		if(this == &o) return *this;
		reset();
		Common_t::operator=(std::move(static_cast<Common_t&>(o)));
		move_construct_active(o);
		kind_ = o.kind_;
		o.reset();
		return *this;
	}
	~Tagged_Union () { destroy_active(); }
	void
	reset () noexcept
	{
		destroy_active();
		kind_ = KIND::None;
	}
	template<KIND K>
	void
	emplace (auto&&... args)
	{
		reset();
		construct_as<K>(std::forward<decltype(args)>(args)...);
		kind_ = K;
	}
	void
	swap (Tagged_Union& o) noexcept requires (swappable)
	{
		if(this == &o) return;
		std::swap(static_cast<Common_t&>(*this), static_cast<Common_t&>(o));
		if(kind_ == o.kind_) {
			with_active(kind_, [&](auto M) {
				constexpr auto m = decltype(M)::value;
				std::swap(this->as.[:m:], o.as.[:m:]);
			});
			return;
		}
		tagged_union::Union<KIND, TMPL> tmp;
		KIND tk = kind_;
		with_active(tk, [&](auto M) {
			constexpr auto m = decltype(M)::value;
			std::construct_at(&tmp.as.[:m:], std::move(this->as.[:m:]));
		});
		destroy_active();
		kind_ = KIND::None;
		move_construct_active(o);
		kind_ = o.kind_;
		o.destroy_active();
		o.kind_ = KIND::None;
		with_active(tk, [&](auto M) {
			constexpr auto m = decltype(M)::value;
			std::construct_at(&o.as.[:m:], std::move(tmp.as.[:m:]));
			std::destroy_at(&tmp.as.[:m:]);
		});
		o.kind_ = tk;
	}
	friend void
	swap (Tagged_Union& a, Tagged_Union& b) noexcept(noexcept(a.swap(b))) requires requires {a.swap(b);}
	{
		a.swap(b);
	}
	[[nodiscard]]
	KIND
	kind () const noexcept
	{
		return kind_;
	}
	[[nodiscard]]
	bool
	empty () const noexcept
	{
		return kind_ == KIND::None;
	}
	bool
	operator== (Tagged_Union const& o) const
	{
		if(kind_ != o.kind_) return false;
		if(!equal(static_cast<Common_t const&>(*this), static_cast<Common_t const&>(o)))
			return false;
		bool eq = true;
		with_active(o.kind_, [&](auto M) {
			constexpr auto m = decltype(M)::value;
			eq = eq && equal(this->as.[:m:], o.as.[:m:]);
		});
		return eq;
	}
private:
	KIND kind_;

	template<typename F>
	void
	with_active (KIND k, F&& f) const
	{
		template for(constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^KIND))) {
			if(k == [:e:]) {
				constexpr auto m = tagged_union::active_member<KIND, TMPL>(^^Store, e);
				if constexpr(m != std::meta::info{})
					f(std::integral_constant<std::meta::info, m>{});
			}
		}
	}
	template<KIND K>
	static consteval std::meta::info
	member_for ()
	{
		template for(constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^KIND)))
			if constexpr(std::meta::extract<KIND>(std::meta::constant_of(e)) == K)
				return tagged_union::active_member<KIND, TMPL>(^^Store, e);
		detail::fail("Internal error: kind not found in '", std::meta::display_string_of(^^KIND), "'.");
	}
	template<KIND K>
	void
	construct_as (auto&&... args)
	{
		constexpr auto m = member_for<K>();
		if constexpr(m == std::meta::info{})
			static_assert(sizeof...(args) == 0, "this kind carries no data");
		else
			std::construct_at(&this->as.[:m:], std::forward<decltype(args)>(args)...);
	}
	void
	copy_construct_active (Tagged_Union const& o)
	{
		with_active(o.kind_, [&](auto M) {
			constexpr auto m = decltype(M)::value;
			std::construct_at(&this->as.[:m:], o.as.[:m:]);
		});
	}
	void
	move_construct_active (Tagged_Union& o)
	{
		with_active(o.kind_, [&](auto M) {
			constexpr auto m = decltype(M)::value;
			std::construct_at(&this->as.[:m:], std::move(o.as.[:m:]));
		});
	}
	void
	destroy_active ()
	{
		with_active(kind_, [this](auto M) {
			std::destroy_at(&this->as.[:decltype(M)::value:]);
		});
	}
};

} // namespace kind_reflect
