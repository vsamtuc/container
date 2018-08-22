#pragma once

#include <cxxtest/TestSuite.h>
#include <boost/core/demangle.hpp>

#include <iostream>
#include <memory>
#include <type_traits>
#include <functional>
#include <unordered_set>
#include <sstream>

#include "cdi.hh"

using namespace cdi;
using namespace cdi::utilities;
using namespace std;

//=================================
//
//  Testing
//
//=================================


int foovoid() { return 100; }
int fooint(int a) { return a+10; }

DEFINE_QUALIFIER(Name, string, const string&)


struct local_scope_tag;

using LocalScope = GuardedScope<local_scope_tag>;

template <typename Value, typename ...Tags>
using Local = resource<Value, LocalScope , Tags...>;


class ProviderSuite : public CxxTest::TestSuite
{
public:

	void tearDown() override {
		GlobalScope::clear();
		prov_map.clear();
	}

	void test_provider_int()
	{
		LocalScope guard;

		//auto Rvoid = resource<int, GlobalScope>(Name("foovoid"));
		auto Rvoid = resource<int>(Name("foovoid"));
		auto Rval = Global<int>(Name("fooint_val"));
		auto Rint = Local<int, int>({});

		provide(Rvoid, foovoid);
		provide(Rval, fooint, 10);
		provide(Rint, fooint, Rvoid );

		// at this point, we have not created Rvoid !
		TS_ASSERT_EQUALS(inject(Rint), 110); 

		TS_ASSERT_EQUALS(inject(Rval), 20); 
		TS_ASSERT_EQUALS(bind(inject<decltype(Rval)>, Rval)(), 20 );
		TS_ASSERT_EQUALS(inject(Rvoid), 100); 

		TS_ASSERT_EQUALS(Rval.manager()->provide(), 20);
		TS_ASSERT_EQUALS(Rvoid.manager()->provide(), 100);
		TS_ASSERT_EQUALS(Rint.manager()->provide(), 110);
	}

	void test_provider_int2() 
	{
		test_provider_int();
	}


	struct Foo {
		int x;
		Foo(int _x) : x(_x) { count++; ++leaked; }
		~Foo() { --leaked; }
		inline static int count =0;
		inline static int leaked = 0;
	};

	void test_provider__ptr()
	{
		using RFoo = Global< shared_ptr<Foo> >;
		auto Foo1 = RFoo({});
		auto Foo2 = RFoo(Name("foo2"));

		// dependency on undeclared resource Foo1
		provide(Foo2, [](shared_ptr<Foo> a){ return make_shared<Foo>(a->x+1); }, Foo1);

		// declare Foo1
		provide(Foo1, []() { return make_shared<Foo>(4); });

		// check the result
		auto c = inject(Foo2);
		TS_ASSERT_EQUALS(c->x, 5);

		shared_ptr<Foo> a = inject(Foo1);
		auto b = inject(Foo1);

		TS_ASSERT_EQUALS(a,b);
		TS_ASSERT_EQUALS(a.use_count(), 3);
		TS_ASSERT_EQUALS(a->x, 4);
	}


	void test_provider_shared_ptr()
	{
		using RFoo = Local< Foo* >;

		{
			LocalScope guard;

			auto Foo1 = RFoo({});
			auto Foo2 = RFoo(Name("foo2"));

			// dependency on undeclared resource Foo1
			provide(Foo2, [](Foo* a){ return new Foo(a->x+1); }, Foo1);
			dispose(Foo2, [](Foo*& p){ delete p; });

			// declare Foo1
			provide(Foo1, []() { return new Foo(4); });
			dispose(Foo1, [](Foo*& p){ delete p; });

			// check the result
			auto c = inject(Foo2);
			TS_ASSERT_EQUALS(c->x, 5);

			Foo* a = inject(Foo1);
			auto b = inject(Foo1);

			TS_ASSERT_EQUALS(a,b);
			TS_ASSERT_EQUALS(Foo::leaked, 2);
			TS_ASSERT_EQUALS(a->x, 4);
		}
		TS_ASSERT_EQUALS(Foo::leaked, 0);
	}


};

