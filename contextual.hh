#pragma once

#include "resource.hh"

#include <any>
#include <vector>

//=================================
//
//  resource management
//
//=================================

namespace cdi {

// forward
class contextual_base;

/**
	This sequence container is returned by the
	type-erase resource manager, to denote known injections
	of resources to lifecycle calls.
 */
typedef std::vector<contextual_base*> injection_list;

namespace detail {
	//
	// Implementation-related
	//

	// A call is a base class for dependencies of lifecycle calls
	struct call {
		template <typename Arg>
		constexpr Arg unwrap_inject(Arg arg) {
			return arg;
		}

		template <typename Arg, typename Scope, typename...Tags>
		inline auto unwrap_inject(const resource<Arg, Scope, Tags...> & res)
		{
			injected.push_back(res.manager());
			return std::bind(std::mem_fn(&resource<Arg, Scope, Tags...>::get), res);
		}
		injection_list injected;
	};

	// a lifecycle call
	template <typename FSig>
	struct typed_call : call {
		std::function<FSig> func;
	};
}


/**
	Base class for resource managers.

	This class provides a type-erased management interface.
  */
class contextual_base
{
public:
	/** Construct manager for a resourceid
		@param r the resource id
	  */
	contextual_base(const resourceid& r)
	: _rid(r) { }

	/** Virtual destructor */
	virtual ~contextual_base() { }

	/** The resource id of the managed resource */
	inline resourceid rid() const { return _rid; }

	/** Return the set of resources required for instantiation */
	virtual const injection_list& provider_injections() const = 0;

	/** Return the set of resources required for disposal */
	virtual const injection_list& disposer_injections() const = 0;

	/** Return the number of injections */
	virtual size_t number_of_injectors() const =0;

	/** Return the set of resources required for an injection */
	virtual const injection_list& injector_injections(size_t i) const=0;

	/** Instantiates a resource instance polymorphically. */
	virtual void provide(std::any&) const = 0;

	/** Injects a resource instance polymorphically. */
	virtual void inject(std::any&) const = 0;

	/** Disposes a resource instance polymorphically. */
	virtual void dispose(std::any&) const = 0;

private:
	resourceid _rid;
};


/**
	A contextual instance stores information about resource
	instances that only depends on the instance type.
  */
template <typename Instance>
class contextual : public contextual_base
{
public:
	contextual(const resourceid& rid)
	: contextual_base(rid) { }

	typedef Instance instance_type;

	/** Set the provider for this contextual */
	template <typename Callable, typename...Args>
	void provider(Callable&& func, Args&& ... args  )
	{
		prov.injected.clear();
 		prov.func = std::bind(std::forward<Callable>(func),
 			prov.unwrap_inject(std::forward<Args>(args))... );
	}

	/** Set the disposer for this contextual */
	template <typename Callable, typename...Args>
	void disposer(Callable&& func, Args&& ... args )
	{
 		using namespace std::placeholders;
 		disp.injected.clear();
 		disp.func = std::bind(std::forward<Callable>(func),
 			_1, disp.unwrap_inject(std::forward<Args>(args))... );
	}

	/** Add a new injector for this contextual */
	template <typename Callable, typename...Args>
	void injector(Callable&& func, Args&& ... args )
	{
 		using namespace std::placeholders;
 		injectors.push_back( detail::typed_call<void(instance_type&)>() );
 		auto& inj = injectors.back();
 		inj.func = std::bind(std::forward<Callable>(func),
 			_1, inj.unwrap_inject(std::forward<Args>(args))... );
	}

	/**
		Create a new instance from the provider.

		@param obj a holder for the new instance of the resource
		@throw instantiation_error if a provider is not set.
	  */
	inline void provide(std::any& obj) const override {
		obj = provide_instance();
	}


	/**
		Return a new instance from the provider.

		@param obj the destination for the new instance of the resource
		@throw instantiation_error if a provider is not set.
	  */
	inline instance_type provide_instance() const {
		namespace u=utilities;
		if(! prov.func) throw instantiation_error(u::str_builder()
			<< "A provider is not set for resource " << rid());
		return prov.func();
	}

	/**
		Inject an object.
		@param obj reference to the object to be injected
	  */
	inline void inject_instance(instance_type& obj) const {
		for(auto& inj : injectors)
			inj.func(obj);
	}

	/**
		Inject an object polymorhically.
		@param obj reference to the object to be injected
	  */
	inline void inject(std::any& obj) const override {
		inject_instance(std::any_cast<instance_type&>(obj));
	}


	/**
		Dispose of an object.
		@param obj reference to the object to be disposed
	  */
	inline void dispose_instance(instance_type& obj) const {
		if(! disp.func) return; // letting the disposer be null is not an error
		disp.func(obj);
	}

	/**
		Dispose of an object polymorphically.
		@param obj reference to the object to be disposed
	  */
	virtual void dispose(std::any& obj) const override {
		dispose_instance(std::any_cast<instance_type&>(obj));
	}

	virtual const injection_list& provider_injections() const override {
		return prov.injected;
	}

	virtual const injection_list& disposer_injections() const override {
		return disp.injected;
	}

	virtual size_t number_of_injectors() const override {
		return injectors.size();
	}
	virtual const injection_list& injector_injections(size_t i) const override {
		return injectors.at(i).injected;
	}

private:
	detail::typed_call<instance_type()> prov;
	detail::typed_call<void(instance_type&)> disp;
	std::vector< detail::typed_call<void(instance_type&)> > injectors;
};



/**
	A resource manager manages the lifecycle of resources.
  */
template <typename Resource>
class resource_manager : public contextual<typename Resource::instance_type>
{
public:
	/// the provided type of the managed resource
	typedef typename Resource::instance_type instance_type;
	/// the scope of the managed resource
	typedef typename Resource::scope scope;
	typedef contextual<instance_type> contextual_type;

	/**
		Construct resource manager for a resource
	  */
	resource_manager(const Resource& r)
	: contextual_type(r.id())
	{ }

	static inline resource_manager<Resource>* get(const Resource& r);

};


//====================================================
//
// resource management api
//
//====================================================

template <typename Instance, typename Scope, typename...Tags>
resource_manager< resource<Instance,Scope,Tags...> >*
resource<Instance,Scope,Tags...>::manager() const {
	//return providence().get(*this);
	return resource_manager< resource<Instance,Scope,Tags...> >::get(*this);

}


template <typename Instance, typename Scope, typename ...Tags>
template <typename Callable, typename...Args>
const resource<Instance, Scope,Tags...> &
resource<Instance, Scope,Tags...>::provide(Callable func, Args&& ... args ) const
{
 	resource_manager<resource_type>* rm = manager();
 	rm->provider(std::forward<Callable>(func), std::forward<Args>(args)...);
 	return (*this);
}

template <typename Instance, typename Scope, typename ...Tags>
template <typename Callable, typename...Args>
const resource<Instance, Scope,Tags...> &
resource<Instance, Scope,Tags...>::inject(Callable func, Args&& ... args ) const
{
 	resource_manager<resource_type>* rm = manager();
 	rm->injector(std::forward<Callable>(func), std::forward<Args>(args)...);
 	return (*this);
}


template <typename Instance, typename Scope, typename ...Tags>
template <typename Callable, typename...Args>
const resource<Instance, Scope,Tags...> &
resource<Instance, Scope,Tags...>::dispose(Callable func, Args&& ... args ) const
{
 	resource_manager<resource_type>* rm = manager();
 	rm->disposer(std::forward<Callable>(func), std::forward<Args>(args)...);
 	return (*this);
}





} // end namespace container
