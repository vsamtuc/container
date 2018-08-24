#pragma once

#include <cxxtest/TestSuite.h>
#include <boost/core/demangle.hpp>

#include <iostream>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <typeindex>
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


struct local_scope_tag;

using LocalScope = GuardedScope<local_scope_tag>;

template <typename Value, typename ...Tags>
using Local = resource<Value, LocalScope , Tags...>;


class ProviderSuite : public CxxTest::TestSuite
{
public:

	static int foovoid() { return 100; }
	static int fooint(int a) { return a+10; }

	DEFINE_QUALIFIER(Name, string, const string&)


	void tearDown() override {
		providence().clear();
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
		TS_ASSERT_EQUALS(get(Rint), 110); 

		TS_ASSERT_EQUALS(get(Rval), 20); 
		TS_ASSERT_EQUALS(bind(&decltype(Rval)::get, Rval)(), 20 );
		TS_ASSERT_EQUALS(get(Rvoid), 100); 

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

	struct A {
		int x;
		string y;
		double z;
		void set_z(double _z) { z=_z; }
	};

	template <typename T, typename V, typename U>
	void inj(T* obj, V T::*attr, U val) {
		obj->*attr = val;
	}

	template <typename T, typename V, typename U>
	void inj(T& obj, V T::*attr, U val) {
		obj.*attr = val;
	}

	template <typename T, typename V, typename U>
	void inj(T* obj, void (T::*meth)(V) , U val) {
		(obj->*meth)(val);
	}

	template <typename T, typename V, typename U>
	void inj(T& obj, void (T::*meth)(V) , U val) {
		(obj.*meth)(val);
	}


	void test_syntax() 
	{ 
		A a;

		inj(&a, &A::x, 4);
		inj(a, &A::y, "hello");
		inj(a, &A::set_z, 3.14);

		TS_ASSERT_EQUALS(a.x, 4);
		TS_ASSERT_EQUALS(a.y, "hello");
		TS_ASSERT_EQUALS(a.z, 3.14);
	}

	#define PR(t) (u::str_builder() << #t << " -> " << u::demangle(typeid(t).name())).str()
	#define P(V)  (u::str_builder() << #V << " -> " << V).str()

	void xtest_type_traits()
	{
		cout << endl;
		cout << PR(Foo*) << endl;
		cout << PR(decay<Foo*>::type) << endl;
		cout << PR(decay<const Foo*>::type) << endl;
		cout << PR(decay<Foo const *>::type) << endl;
		cout << PR(decay<Foo* const&>::type) << endl;

		cout << P(is_pointer<Foo*>::value) << endl;
		cout << P(is_pointer<Foo*&>::value) << endl;;
		cout << P(is_object<Foo*>::value) << endl;

		cout << PR(remove_reference<Foo*&&>::type) << endl;
		cout << P(is_class<Foo*>::value) << endl;
		cout << P(is_class<Foo&>::value) << endl;
		cout << P(is_class<const Foo>::value) << endl;
		cout << P(is_class<Foo*>::value) << endl;

		cout << endl;
	}

	static int doubler(int x) { return 2*x; }
	static int summer(int a, int b, int c) { return a+b+c; }

	void test_bind() 
	{
		using std::bind;
		using namespace std::placeholders;
		auto f = bind(summer,_1, bind(doubler,_2), bind(doubler,_2));

		TS_ASSERT_EQUALS(f(1,4), 17);
	}

};

