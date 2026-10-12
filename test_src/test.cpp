// Build (adjust to your P2996 compiler/fork), with kind_reflect/ next to this file:
//   g++ -std=c++26 -freflection -I. test.cpp -o test && ./test
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

#include "fat_struct.hpp"
#include "tagged_union.hpp"

static int g_failures = 0;
#define CHECK(cond) \
	do { if(!(cond)) { ++g_failures; std::printf("FAIL line %d: %s\n", __LINE__, #cond); } } while(0)

// ---------------------------------------------------------------- Fat_Struct
enum class F_Kind { None, A, B, C };
struct F_Tmpl {
	struct A { int x; };
	struct B { int x; int y; };
	struct C { double d; int z; int w; };
	struct Kind_Reflect_Common {
		int a;
		int b;
		bool c;
	};
};
using Fat = kind_reflect::Fat_Struct<F_Kind, F_Tmpl>;

static_assert(std::is_trivially_copyable_v<Fat>, "all slot types are plain, so Fat should be trivially copyable");

static void
test_fat ()
{
	Fat f;
	CHECK(f.empty());
	CHECK(f.kind() == F_Kind::None);

	// common fields are directly accessible
	f.a = 1; f.b = 2; f.c = true;
	CHECK(f.a == 1 && f.b == 2 && f.c);

	// emplace with arguments, access through a view
	f.emplace<F_Kind::B>(10, 20);
	CHECK(f.kind() == F_Kind::B);
	{
		auto v = f.as<F_Kind::B>();
		CHECK(v.x == 10);
		CHECK(v.y == 20);
		v.x = 11;                       // views are references into the object
	}
	CHECK(f.as<F_Kind::B>().x == 11);

	// structured bindings on a view
	{
		auto [x, y] = f.as<F_Kind::B>();
		CHECK(x == 11 && y == 20);
	}

	// emplace without arguments clears the kind's slots
	f.emplace<F_Kind::C>();
	{
		auto v = f.as<F_Kind::C>();
		CHECK(v.d == 0.0 && v.z == 0 && v.w == 0);
		v.d = 2.5; v.z = 3; v.w = 4;
	}

	// const access
	Fat const& cf = f;
	{
		auto v = cf.as<F_Kind::C>();
		CHECK(v.d == 2.5 && v.z == 3 && v.w == 4);
	}

	// copy (trivial) keeps everything
	Fat g = f;
	CHECK(g.kind() == F_Kind::C);
	CHECK(g.a == 1 && g.b == 2 && g.c);
	CHECK(g.as<F_Kind::C>().z == 3);
	CHECK(g == f);

	// equality: only the active kind's fields and the common fields matter
	g.as<F_Kind::C>().w = 99;
	CHECK(!(g == f));
	g.as<F_Kind::C>().w = 4;
	g.a = 5;
	CHECK(!(g == f));
	g.a = 1;
	CHECK(g == f);

	// different kinds are never equal
	Fat h = f;
	h.emplace<F_Kind::A>(7);
	CHECK(!(h == f));
	CHECK(h.as<F_Kind::A>().x == 7);

	// reset
	h.reset();
	CHECK(h.empty());

	// layout report (not asserted: depends on ABI)
	std::printf("sizeof(Fat) = %zu\n", sizeof(Fat));
}

// ------------------------------------------------------------- Tagged_Union
enum class T_Kind { None, Num, Text, Pair, Flag };
struct T_Tmpl {
	struct Num { int value; };                       // single field: member is named 'value'
	struct Text { std::string text; };               // single field: member is named 'text'
	struct Pair {                                    // multi field: needs an alias to name it
		using pair = void;
		int first;
		std::string second;
	};
	struct Flag {};                                  // no data
	struct Kind_Reflect_Common { int id; };
};
using TU = kind_reflect::Tagged_Union<T_Kind, T_Tmpl>;

static_assert(std::is_copy_constructible_v<TU>);
static_assert(std::is_nothrow_move_constructible_v<TU>);
static_assert(std::is_nothrow_swappable_v<TU>);

static void
test_tagged ()
{
	TU t;
	CHECK(t.empty());
	t.id = 42;

	t.emplace<T_Kind::Num>(5);
	CHECK(t.kind() == T_Kind::Num);
	CHECK(t.as.value == 5);

	t.emplace<T_Kind::Text>("hello");
	CHECK(t.kind() == T_Kind::Text);
	CHECK(t.as.text == "hello");

	t.emplace<T_Kind::Pair>(1, "two");
	CHECK(t.as.pair.first == 1);
	CHECK(t.as.pair.second == "two");

	t.emplace<T_Kind::Flag>();
	CHECK(t.kind() == T_Kind::Flag);
	CHECK(t.id == 42);

	// in-place construction
	TU u(TU::in_place_kind<T_Kind::Text>, "abc");
	CHECK(u.kind() == T_Kind::Text);
	CHECK(u.as.text == "abc");

	// copy construction / assignment
	TU c = u;
	CHECK(c == u);
	c.as.text = "changed";
	CHECK(!(c == u));
	c = u;
	CHECK(c == u);

	// move construction leaves the source empty
	TU m = std::move(c);
	CHECK(m == u);
	CHECK(c.empty());

	// swap, same kind and different kinds
	TU p(TU::in_place_kind<T_Kind::Pair>, 7, "seven");
	TU q(TU::in_place_kind<T_Kind::Text>, "text");
	swap(p, q);
	CHECK(p.kind() == T_Kind::Text && p.as.text == "text");
	CHECK(q.kind() == T_Kind::Pair && q.as.pair.first == 7 && q.as.pair.second == "seven");

	TU r(TU::in_place_kind<T_Kind::Text>, "other");
	p.swap(r);
	CHECK(p.as.text == "other");
	CHECK(r.as.text == "text");

	// swap with an empty object (the None side just receives the other payload)
	TU e;
	e.swap(q);
	CHECK(e.kind() == T_Kind::Pair);
	CHECK(q.empty());

	// move assignment (goes through swap)
	q = std::move(e);
	CHECK(q.kind() == T_Kind::Pair);

	// common fields participate in equality
	TU x(TU::in_place_kind<T_Kind::Num>, 1);
	TU y(TU::in_place_kind<T_Kind::Num>, 1);
	CHECK(x == y);
	y.id = 9;
	CHECK(!(x == y));

	// reset destroys the payload
	x.reset();
	CHECK(x.empty());
}

// Negative tests: each of these should fail to compile. Uncomment one at a time.
//
// struct Bad_Tmpl_NoCtor {                 // field not default constructible (Fat_Struct only)
//     struct A { struct NC { NC(int); } n; };
//     struct Kind_Reflect_Common { int a; };
// };
// enum class Bad_Kind { None, A };
// kind_reflect::Fat_Struct<Bad_Kind, Bad_Tmpl_NoCtor> bad1;
//
// enum class Bad_Kind2 { A };              // missing 'None'
// struct Bad_Tmpl2 { struct A { int x; }; struct Kind_Reflect_Common { int a; }; };
// kind_reflect::Fat_Struct<Bad_Kind2, Bad_Tmpl2> bad2;
//
// Fat f; f.as<F_Kind::A>();                // asserts at runtime (kind is None)

int
main ()
{
	test_fat();
	test_tagged();
	if(g_failures == 0) std::puts("all tests passed");
	else std::printf("%d check(s) failed\n", g_failures);
	return g_failures != 0;
}
