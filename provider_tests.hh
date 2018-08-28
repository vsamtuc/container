#pragma once

#include <cxxtest/TestSuite.h>

#include <iostream>
#include "cdi.hh"

using namespace cdi;
using namespace std;

class InContainer : public CxxTest::TestSuite
{
	void tearDown() {
		providence().clear();
	}

};


//=================================
//
//  Testing
//
//=================================





class ProviderSuite : public InContainer
{
public:


	struct LocalScope : GuardedScope<LocalScope> {};

	template <typename Value, typename ...Tags>
	using Local = resource<Value, LocalScope , Tags...>;

	static int foovoid() { return 100; }
	static int fooint(int a) { return a+10; }

	DEFINE_QUALIFIER(Name, string, const string&)


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
		TS_ASSERT_EQUALS(get(Rint), 110);

		TS_ASSERT_EQUALS(get(Rval), 20);
		TS_ASSERT_EQUALS(get(Rvoid), 100);

		TS_ASSERT_EQUALS(Rval.manager()->provide_instance(), 20);
		TS_ASSERT_EQUALS(Rvoid.manager()->provide_instance(), 100);
		TS_ASSERT_EQUALS(Rint.manager()->provide_instance(), 110);

	}

	void test_provider_int2()
	{
		// This is a test for clearing the container between tests!
		test_provider_int();
	}

	struct Foo {
		int x;
		Foo(int _x) : x(_x) { count++; ++leaked; }
		~Foo() { --leaked; }
		inline static int count =0;
		inline static int leaked = 0;
	};


	void test_provider_ptr()
	{
		using RFoo = Global< shared_ptr<Foo> >;
		auto Foo1 = RFoo({});
		auto Foo2 = RFoo(Name("foo2"));

		// dependency on undeclared resource Foo1
		provide(Foo2, [](shared_ptr<Foo> a){ return make_shared<Foo>(a->x+1); }, Foo1);

		// declare Foo1
		provide(Foo1, []() { return make_shared<Foo>(4); });

		// check the result
		auto c = get(Foo2);
		TS_ASSERT_EQUALS(c->x, 5);

		shared_ptr<Foo> a = get(Foo1);
		auto b = get(Foo1);

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
			//provide(Foo2, [](Foo* a){ return new Foo(a->x+1); }, Foo1);
			provide(Foo2, [](auto a){ return new Foo(a->x+1); }, Foo1);
			TS_ASSERT_EQUALS( (declare(Foo2)->provider_injections().size()), 1);

			dispose(Foo2, [](Foo*& p){ delete p; });

			// declare Foo1
			provide(Foo1, []() { return new Foo(4); });
			dispose(Foo1, [](Foo*& p){ delete p; });

			// check the result
			auto c = get(Foo2);
			TS_ASSERT_EQUALS(c->x, 5);

			Foo* a = get(Foo1);
			auto b = get(Foo1);

			TS_ASSERT_EQUALS(a,b);
			TS_ASSERT_EQUALS(Foo::leaked, 2);
			TS_ASSERT_EQUALS(a->x, 4);
		}
		TS_ASSERT_EQUALS(Foo::leaked, 0);
	}

	static int weird_func() {
		static int count=0;
		return count++;
	}

	// Test that passing std::bind objects to a provider, injector,
	// etc delays their evaluation
	//
	void test_bind_arg()
	{
		resource<int> q({});
		provide(q, [](auto x) { return x; }, bind(weird_func));

		TS_ASSERT_EQUALS(weird_func(), 0);
		TS_ASSERT_EQUALS(weird_func(), 1);
		TS_ASSERT_EQUALS(q.get(), 2);
		TS_ASSERT_EQUALS(weird_func(), 3);
		TS_ASSERT_EQUALS(q.get(), 2);
	}

	void test_phase()
	{
		TS_ASSERT(Phase::allocated < Phase::provided);
		TS_ASSERT(Phase::injected > Phase::provided);
		TS_ASSERT(Phase::injected < Phase::created);
		TS_ASSERT(Phase::disposed > Phase::created);
	}

};
