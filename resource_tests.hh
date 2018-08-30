#pragma once

#include <cxxtest/TestSuite.h>

#include "cdi.hh"

using namespace cdi;
using namespace cdi::utilities;
using namespace std;


// get method names init and dispose automatically...
template <typename CompClass, class = std::void_t<> >
inline constexpr bool
has_dispose = false;

template <typename CompClass>
inline constexpr bool
has_dispose<CompClass, std::void_t<decltype(std::declval<CompClass>().dispose())> > = true;

// get method names init and dispose automatically...
template <typename CompClass, class = std::void_t<> >
inline constexpr bool
has_initialize = false;

template <typename CompClass>
inline constexpr bool
has_initialize<CompClass, std::void_t<decltype(std::declval<CompClass>().initialize())> > = true;

template <typename CompClass>
class component
{

	void auto_dispose() const {
		if constexpr (has_dispose<CompClass>)
			r.dispose([](auto self) { self->dispose(); });
	}

	void auto_initialize() const {
		if constexpr (has_initialize<CompClass>)
			r.initialize([](auto self) { self->initialize(); });
	}

public:
	using resource_type = resource< std::shared_ptr<CompClass> >;
	using instance_type = typename resource_type::instance_type;
	using return_type = typename resource_type::return_type;

	inline operator resourceid() const { return r; }

	component(const qualifiers& quals) : r(quals) { }

	template <typename ... Args>
	auto provide(Args&& ... args) const {
		r.provide([](auto&&...a){ return std::make_shared<CompClass>(a...); },
		   std::forward<Args>(args)...
		);
		auto_initialize();
		auto_dispose();
		return *this;
	}

	template <typename MType, typename Arg>
	std::enable_if_t< std::is_member_object_pointer_v<MType>, const component<CompClass>&>
	inject(MType member, Arg&& arg) const {
		r.inject([member](auto self, auto a) { (*self).*member = a; },
			std::forward<Arg>(arg));
		return *this;
	}

	// template <typename MType, typename...MArgs, typename ...Args>
	// auto inject(MType (CompClass::*member)(MArgs...), Args&&...args) const {
	// 	r.inject([member](auto self, auto...a)->void {
	// 			((*self).*member)(a...);
	// 		},
	// 		std::forward<Args>(args)...);
	// 	return *this;
	// }

	template <typename MType, typename ...Args>
	std::enable_if_t< std::is_member_function_pointer_v<MType>, const component<CompClass>&>
	inject(MType member, Args&&...args) const {
		r.inject([member](auto self, auto...a)->void {
				((*self).*member)(a...);
			},
			std::forward<Args>(args)...);
		return *this;
	}

	template <typename MType, typename...MArgs, typename ...Args>
	auto initialize(MType (CompClass::*member)(MArgs...), Args&&...args) const {
		r.initialize([member](auto self, auto...a)->void {
				((*self).*member)(a...);
			},
			std::forward<Args>(args)...);
		return *this;
	}

	template <typename MType, typename...MArgs, typename ...Args>
	auto dispose(MType (CompClass::*member)(MArgs...), Args&&...args) const {
		r.dispose([member](auto self, auto...a)->void {
				((*self).*member)(a...);
			},
			std::forward<Args>(args)...);
		return *this;
	}

private:
	resource_type r;
};



class ResourceSuite : public CxxTest::TestSuite
{
public:

	void tearDown() {
		providence().clear();
	}

	struct Foo {
		int a;
		void set_a(int _a) { a=_a; }
	};

	struct Bar : Foo
	{
		int x;
		Bar() : x(-1) { }
		Bar(string a) : x(0) { }
		Bar(int a) : x(a) { }

		void initialize() { ++init; }
		void dispose() { ++disp; }

		void set_x(int p, int q) { x=p*q;}

		void add_x(int q) {
			x+=q;
		}
		inline static size_t init = 0;
		inline static size_t disp = 0;
	};

	DEFINE_QUALIFIER(Name, string, const string&);

	void test_get_bar() {
		using ctype = component<Bar>;
		TS_ASSERT_EQUALS( resourceid(ctype({})) , resourceid( resource<shared_ptr<Bar>>({})) );

		resource<int> constnum({});
		constnum.provide([]() { return 100; });

		int N = 10;
		ctype r({});
		r.provide(N);

		ctype s({Default});
		s.provide(constnum);

		ctype t({Null});
		t.provide("Hello");

		ctype u({Null,Default});
		u.provide().inject(&Bar::set_a, 10).inject(&Bar::a, 20);

		ctype v(Name("v"));
		v.provide().inject(&Bar::x, constnum).inject(&Bar::add_x, 17);

		ctype w(Name("w"));
		w.provide().inject(&Bar::set_x, constnum, 3);

		TS_ASSERT_EQUALS( (get(r)->x) ,10);
		TS_ASSERT_EQUALS( (get(s)->x) ,100);
		TS_ASSERT_EQUALS( (get(t)->x) ,0);
		TS_ASSERT_EQUALS( (get(u)->x), -1);
		TS_ASSERT_EQUALS( (get(u)->a), 20);
		TS_ASSERT_EQUALS( (get(v)->x), 117);
		TS_ASSERT_EQUALS( (get(w)->x), 300);
	}

	void test_after_get_bar()
	{
		TS_ASSERT_EQUALS(Bar::init, 6);
		TS_ASSERT_EQUALS(Bar::disp, 6);
	}

	template <typename Instance>
	struct minimal_resource
	{
		//typedef minimal_resource<Instance> resource_type;
		typedef Instance instance_type;
		typedef Instance return_type;

		const scope_api& scope() const {
			static scope_proxy<GlobalScope> api;
			return api;
		}

		operator resourceid () const {
			return resourceid(typeid(minimal_resource<Instance>), {});
		}
	};

	struct foo : minimal_resource<foo>
	{
		int n;
		foo(int x) : n(x) { }
	};

	void test_fake()
	{
		foo r(2);

		declare(r);
		provide(r, []() { return foo(10); });

		TS_ASSERT_EQUALS(get(r).n, 10);
		TS_ASSERT( is_resource_type<foo> );
		TS_ASSERT( !is_resource_type<int> );
		TS_ASSERT( !is_resource_type<string> );
	}

	void test_resource_constructor()
	{
		auto R = resource<string>(Default).id();
		TS_ASSERT_EQUALS(R.type(), type_index(typeid(resource<string>)));
		TS_ASSERT_EQUALS(R.quals(), qualifiers({Default}));
		TS_ASSERT_EQUALS( (resource<string>(Default).id()), (resource<string>({Default}).id()) );

	}

	void test_resource_return()
	{
		resource<string> r({});
		provide(r, [](){ return "hello world"; });

		auto s = r.get();
		s.append(" and bye");

		TS_ASSERT_DIFFERS(s, r.get());
	}

	void test_resource_intset()
	{
		// define as the decayed type of a func
		resource< std::decay_t<const set<int>> > s({});
		s	.provide([]() { return set<int>(); })
			.initialize([](auto& self){
				for(int i=1; i<=3; i++) {
					self.insert(i);
				}
			});

		auto x = s.get();
		TS_ASSERT_EQUALS(x, (set<int>{1,2,3}));
		x.insert(5);
		TS_ASSERT_EQUALS(s.get(), (set<int>{1,2,3}));
	}

	void test_resourceid()
	{

	}

};
