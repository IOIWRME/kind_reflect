# kind_reflect

A header-only C++26 library that uses static reflection (P2996) to generate "kind" types from a plain description: an `enum class` of kinds plus a struct that holds one nested struct per kind.

Two generated types share the same description format:

| Type | Storage | Best when |
|---|---|---|
| `Tagged_Union<KIND, TMPL>` | One union; only the active kind's payload is alive | Payloads are non-trivial (strings, vectors) or kinds differ a lot in size |
| `Fat_Struct<KIND, TMPL>` | One flat set of slots; kinds of the same field type share slots | Cache-friendly, trivially copyable data with similar kinds |

Both also carry a set of *common* fields that exist for every kind and are accessed directly (`obj.id`).

## Requirements

- A compiler with C++26 static reflection (P2996), such as GCC with `-freflection`.
- Developed against GCC 16 with `-std=c++26 -freflection`. Other compilers and versions are untested.

## Installing

It is header-only. Either copy the headers or install them:

```sh
make install                      # copies the headers to ~/.local/include/kind_reflect/
make install PREFIX=/usr/local    # or choose another prefix (may need sudo)
make uninstall                    # removes them again
```

`DESTDIR` is honored for packaging. Then add the include directory to your compiler flags (`-I ~/.local/include`) and write:

```cpp
#include <kind_reflect/tagged_union.hpp>
#include <kind_reflect/fat_struct.hpp>
```

Everything lives in the `kind_reflect` namespace. The two headers can be included independently.

## Describing your kinds

```cpp
enum class Kind { None, Num, Text, Pair, Flag };

struct Tmpl {
    struct Num  { int value; };
    struct Text { std::string text; };
    struct Pair { using pair = void; int first; std::string second; };
    struct Flag {};
    struct Kind_Reflect_Common { int id; };   // optional
};
```

Rules, all checked at compile time:

- The enum must contain an enumerator named `None`. It is the empty state and the default value.
- Every other enumerator needs a nested struct of the same name in the template, and every nested struct (except the common one) needs an enumerator.
- The template must contain only nested structs. No functions, no unnamed types, and no struct named `None`.
- `Kind_Reflect_Common` is optional. Its fields become public members of the generated type.

## `Tagged_Union`

```cpp
using Value = kind_reflect::Tagged_Union<Kind, Tmpl>;

Value v;                                   // kind() == Kind::None
v.id = 42;                                 // common field
v.emplace<Kind::Text>("hello");
v.as.text;                                 // "hello"
v.emplace<Kind::Pair>(1, "two");
v.as.pair.first;                           // 1

Value w(Value::in_place_kind<Kind::Num>, 5);
w.as.value;                                // 5
```

How the payload is named inside `as`:

- A kind with one field is named after that field (`Num` gives `as.value`).
- A kind with several fields needs a name alias, `using name = void;`, and the whole struct is stored under that name (`as.pair.first`). An alias also works on a single-field kind and then wins over the field name.
- A kind with no fields has no payload.
- Two kinds must not produce the same name; give one an alias.

Behavior:

- `emplace<K>(args...)` destroys the current payload and constructs the new one in place. Aggregates can be built with parenthesized arguments.
- `reset()` destroys the payload and sets the kind to `None`.
- Copy, move, assignment and `swap` are available when every payload and common field supports them. Move construction leaves the source `None`.
- `kind()`, `empty()` and `operator==` are provided.

## `Fat_Struct`

```cpp
enum class Kind { None, A, B, C };
struct Tmpl {
    struct A { int x; };
    struct B { int x; int y; };
    struct C { double d; int z; int w; };
    struct Kind_Reflect_Common { int a; int b; bool c; };
};
using Fat = kind_reflect::Fat_Struct<Kind, Tmpl>;

Fat f;
f.a = 1;                                   // common field
f.emplace<Kind::B>(10, 20);
auto v = f.as<Kind::B>();                  // a view: v.x, v.y are references into f
v.x = 11;
auto [x, y] = f.as<Kind::B>();             // structured bindings work
```

Every kind's fields are laid out in shared slots. For each field type, the number of slots is the largest count of that type in any single kind, and a field takes the slot matching its position among the fields of the same type. In the example, `A::x`, `B::x` and `C::z` share one `int` slot, and `B::y` and `C::w` share another. Slots are ordered by decreasing alignment to limit padding.

Behavior and caveats:

- The views are temporaries made at the call site. Nothing in the object holds references, so layout and cache behavior are the same as a hand-written struct. Don't store views.
- `as<K>()` asserts that the active kind is `K`.
- `emplace<K>()` with no arguments zeroes the kind's slots. With arguments, pass exactly one per field.
- Switching kinds does not clear the old kind's data unless you call `emplace`.
- Field types must be default-constructible and must not be references or cv-qualified.
- Names starting with `__fs_` are reserved in the common struct.
- Copy and move are the compiler-generated ones, so the type is trivially copyable whenever the field types are.

## Equality

`operator==` compares the kind, the common fields, and the active payload. Each value is compared with:

1. a function `kind_reflect_equal(a, b)` found by argument-dependent lookup, if one exists;
2. otherwise `a == b`, if it compiles;
3. otherwise a member-by-member comparison for aggregates of the same type.

Note that `std::unique_ptr` compares by address under rule 2. If you want a deep comparison, provide a `kind_reflect_equal` overload in the namespace of the type:

```cpp
namespace mylib {
    bool kind_reflect_equal (Node_Ptr const& a, Node_Ptr const& b);
}
```

## Error messages

Invalid descriptions are reported with a `static_assert` that names the problem, for example:

```
static assertion failed: kind_reflect: Enumerator 'D' has no corresponding struct in 'Tmpl'
```

Later errors that mention `Internal error` or follow from using the invalid type usually come from that first problem. Building with `-fmax-errors=1` shows only the first.

## Limitations

- A payload cannot contain the enclosing type by value. Use a pointer or container.
- Reflection support is still limited to a few compilers.

## Building the tests

```sh
make test
```

This compiles `src/test.cpp` with `-std=c++26 -freflection -I inc` into `build/test` and runs it. It prints `all tests passed` on success and exits non-zero otherwise. `make clean` removes the build output.

## License

Released under the [MIT License](LICENSE).
