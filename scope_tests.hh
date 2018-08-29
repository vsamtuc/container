#pragma once

#include <cxxtest/TestSuite.h>
#include <vector>

#include "cdi.hh"

using namespace std;
using namespace cdi;

//
// Scope instances can be used to control context,
// therefore, we need a qualifier to
//


qualifier GlobalS { new scope_proxy<GlobalScope> };
qualifier NewS { new scope_proxy<NewScope> };

class ScopeTestSuite : public CxxTest::TestSuite
{
public:

	void test_print_globalS()
	{
		cout << GlobalS << endl;
		auto proxy = GlobalS.get<scope_api>();
		TS_ASSERT( proxy );
		proxy->drop(resource<int>({}).id());

		TS_ASSERT_EQUALS(GlobalS, qualifier(new scope_proxy<GlobalScope>));
		TS_ASSERT_DIFFERS(NewS, qualifier(new scope_proxy<GlobalScope>));

		qualifiers Q { GlobalS, NewS };
		TS_ASSERT_EQUALS(Q.size(),2);

		vector< qualifier > scopes;
		Q.collect<scope_api>(back_inserter(scopes));
		cout << Q << endl;
		TS_ASSERT_EQUALS(scopes.size(), 2);
	}

	struct Foo {
		int x;
		Foo(int _x) : x(_x) { count++; ++leaked; }
		~Foo() { --leaked; }
		inline static int count =0;
		inline static int leaked = 0;
	};

	struct B;
	struct A {

		static inline auto rsrc = resource<A*>(Global);

		B* other;
		A(B* o) : other(o) { }

		B* get_other() { return other; }

	};
	struct B {
		A* other;
		B(A* o) : other(o) { }
	};


	void tearDown() override {
		providence().clear();
	}

	void test_new_scope()
	{
		auto r1 = resource<Foo*>({New});
		provide(r1, [](){ return new Foo(5); });
		dispose(r1, [](auto p) { delete p; });

		vector<Foo*> arr;
		for(size_t i=0;i<10;i++)
			arr.push_back(get(r1));

		for(size_t i=0;i<10;i++)
			for(size_t j=i+1;j<10;j++) {
				TS_ASSERT_DIFFERS(arr[i],arr[j]);
			}
		TS_ASSERT_EQUALS(Foo::leaked, 10);

		for(size_t i=0;i<10;i++)
			delete arr[i];
	}

	void test_global_scope2()
	{
		TS_ASSERT_EQUALS(providence().resource_managers().size(), 0);

		auto r1 = resource<std::shared_ptr<Foo>>({Global});
		auto rm1 = declare(r1);
		TS_ASSERT_EQUALS(rm1, declare(r1));
		provide(r1, [](){ return std::make_shared<Foo>(42); });
		TS_ASSERT(providence().resource_managers().contains(r1));
		auto ptr = r1.get();
		TS_ASSERT_EQUALS(ptr->x, 42);
	}

	void test_global_cycle()
	{
		auto rb = resource<B*>({});

		provide(A::rsrc, [](B*o){ return new A(o); }, rb);
		provide(rb, [](A*o){ return new B(o); }, A::rsrc);

		TS_ASSERT_EQUALS(providence().resource_managers().size(), 2);
		u::str_builder report;
		TS_ASSERT( ! providence().check_consistency(report) );
		TS_ASSERT( report.str().find("Cyclical dependency")!=string::npos );

		try {
			auto x = A::rsrc.get()->other;
			cout << x << endl;
		} catch(const cdi::exception& e) {
			u::str_builder s;
			output_exception(s,e);
			TS_ASSERT( s.str().find("Cyclical dependency")!=string::npos );
			return;
		}
		TS_FAIL("A cyclical dependency was not caught");
	}

	void test_init()
	{
		struct Info {
			int a;
			double b;
			string c;
		};

		auto r = resource<Info*>({Global});

		string get_c;

		provide(r, [](){ return new Info; })
		.inject([](auto self) { self->a = 1; })
		.inject([](auto self) { self->b = 2; })
		.inject([](auto self) { self->c = "Hello"; })
		.initialize([&](auto self) { get_c = self->c; })
		.dispose([](auto self) { delete self; });

		TS_ASSERT_EQUALS(get_c, string());
		r.get();

		TS_ASSERT_EQUALS(get_c, "Hello");
	}

	void test_basic_injection()
	{
		auto ra = resource<A*>({});
		auto rb = resource<B*>({});

		ra	.provide([](){ return new A(nullptr); })
			.inject([](auto self, auto other){ self->other = other; }, rb)
			.dispose([](auto self) { delete self; });

		rb	.provide([](){ return new B(nullptr); })
			.dispose([](auto self) { delete self; });

		TS_ASSERT_EQUALS(providence().resource_managers().size(), 2);
		TS_ASSERT( providence().check_consistency(cerr) );

		TS_ASSERT_EQUALS(ra.get()->other, rb.get());
	}

	void test_global_cycle_with_injection1()
	{
		// both are default-constructed
		resource<A*>({})
			.provide([](){ return new A(nullptr); })
			.inject([](auto self, auto other){ self->other = other; }, resource<B*>({}))
			.dispose([](auto self) { delete self; });

		resource<B*>({})
			.provide([](){ return new B(nullptr); })
			.inject([](auto self, auto other){ self->other = other; }, resource<A*>({}))
			.dispose([](auto self) { delete self; });

		auto a = resource<A*>({}).get();
		auto b = resource<B*>({}).get();

		TS_ASSERT( providence().check_consistency(cerr) );

		TS_ASSERT_EQUALS(a->other, b);
		TS_ASSERT_EQUALS(b->other, a);
	}


	void test_global_cycle_with_injection2()
	{
		// A requires B, B is default-constucted
		resource<A*>({})
			.provide([](auto b){ return new A(b); }, resource<B*>({}))
			.dispose([](auto self) { delete self; });

		resource<B*>({})
			.provide([](){ return new B(nullptr); })
			.inject([](auto self, auto other){ self->other = other; }, resource<A*>({}))
			.dispose([](auto self) { delete self; });

		TS_ASSERT( providence().check_consistency(cerr) );

		A* a;
		try{
			a = resource<A*>({}).get();
		} catch(std::exception& e) {
			output_exception(cerr, e);
			throw;
		}
		auto b = resource<B*>({}).get();

		TS_ASSERT_EQUALS(a->other, b);
		TS_ASSERT_EQUALS(b->other, a);
	}

	void test_global_cycle_with_injection3()
	{
		// A is default constructed, B requires A
		resource<A*>({})
			.provide([](){ return new A(nullptr); })
			.inject([](auto self, auto other){ self->other = other; }, resource<B*>({}))
			.dispose([](auto self) { delete self; });

		resource<B*>({})
			.provide([](auto a){ return new B(a); }, resource<A*>({}))
			.dispose([](auto self) { delete self; });

		auto a = resource<A*>({}).get();
		auto b = resource<B*>({}).get();

		TS_ASSERT_EQUALS(a->other, b);
		TS_ASSERT_EQUALS(b->other, a);
	}

	struct TempScope : LocalScope<TempScope> { };
	static inline qualifier Temp { new scope_proxy<TempScope> };

	void test_local_scope()
	{
		auto r = resource<int*>({Temp});
		r	.provide([]() { return new int(10); })
			.dispose([](auto self) { delete self; });

		TS_ASSERT(! TempScope::is_active());

		// outer instance
		{
			TempScope s1;

			int* p1 = r.get();
			// inner Instance
			{
				TempScope s2;

				int* p2 = r.get();

				TS_ASSERT_DIFFERS(p1, p2);
			}
		}
	}

};
