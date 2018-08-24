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
class basic_resource_manager;

/** 
	This sequence container is returned by the 
	type-erase resource manager, to denote known injections
	of resources to lifecycle calls.
 */
typedef std::vector<basic_resource_manager*> injection_list;

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
class basic_resource_manager
{
public:
	/** Construct manager for a resourceid 
		@param r the resource id
	  */
	basic_resource_manager(const resourceid& r) 
	: _rid(r) { }

	/** Virtual destructor */
	virtual ~basic_resource_manager() { }

	/** The resource id of the managed resource */
	inline resourceid rid() const { return _rid; }

	/** Return the set of resources required for instantiation */
	virtual const injection_list& provider_injections() const = 0;

	/** Return the set of resources required for instantiation */
	virtual const injection_list& disposer_injections() const = 0;

	/** Instantiates a resource instance polymorphically. */
	virtual std::any provide_any() const = 0;
	/** Disposes a resource instance polymorphically. */
	virtual void dispose_any(std::any&) const = 0;

private:
	resourceid _rid;
};


/**
	A contextual instance stores information about resource
	instances that only depends on the instance type.
  */
template <typename Instance>
class contextual : public basic_resource_manager
{
public:
	contextual(const resourceid& rid)
	: basic_resource_manager(rid) { }

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
		Return a new instance from the provider.

		@return a new instance of the resource
		@throw instantiation_error if a provider is not set.
	  */
	inline instance_type provide() const { 
		namespace u=utilities;
		if(! prov.func) throw instantiation_error(u::str_builder() 
			<< "A provider is not set for resource " << rid());
		return prov.func();
	}

	/**
		Return a new instance from the provider polymorphically.

		@return a new instance of the resource
		@throw instantiation_error if a provider is not set.
	  */
	inline std::any provide_any() const override {
		return std::any(provide());
	}

	/**
		Inject an object.
		@param obj reference to the object to be disposed
	  */
	inline void inject(instance_type& obj) const {
		for(auto& inj : injectors)
			inj.func(obj);
	}

	/**
		Dispose of an object.
		@param obj reference to the object to be disposed
	  */
	inline void dispose(instance_type& obj) const {
		if(! disp.func) return; // letting the disposer be null is not an error
		disp.func(obj);
	}

	/**
		Dispose of an object polymorphically.
		@param obj reference to the object to be disposed
	  */
	virtual void dispose_any(std::any& obj) const override {
		dispose(std::any_cast<instance_type&>(obj));
	}

	virtual const injection_list& provider_injections() const override {
		return prov.injected;
	}

	virtual const injection_list& disposer_injections() const override {
		return disp.injected;
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

private:
	detail::typed_call<instance_type()> prov;
	detail::typed_call<void(instance_type&)> disp;
};


struct provision_map : resource_map<basic_resource_manager*> 
{
	template <typename Resource>
	inline resource_manager<Resource>* get(const Resource& r) {
		basic_resource_manager* p;
		try {
			p = this->at(r);
			return static_cast<resource_manager<Resource>*>(p);
		} catch(std::out_of_range) {
			auto rm = new resource_manager<Resource>(r); 
			bool succ [[maybe_unused]];
			std::tie(std::ignore, succ) = emplace(r, rm);
			assert(succ); // since we just failed the lookup!
			// call recursively!
			return get(r);
		}
	}


private:
	void clear() {
		// Delete all resource managers
		for(auto& [rid  ,rm] : *this) { 
			(void) rid;//maybe unused?
			delete rm; 
		}
		resource_map<basic_resource_manager*>::clear();
	}
	friend class GlobalScope;
};


inline provision_map& providence() { 
	static provision_map prov_map;	
	return prov_map; 
}



//====================================================
//
// resource management api
//
//====================================================

template <typename Instance, typename Scope, typename...Tags>
resource_manager< resource<Instance,Scope,Tags...> >* 
resource<Instance,Scope,Tags...>::manager() const { 
	return providence().get(*this);
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


//==================================================
//
// Functional resource API
//
//==================================================


/**
	Return the resource manager for a resource
	@tparam Resource the resource type
	@param r the resource
	@return the resource manager for this resource
  */
template <typename Resource>
inline resource_manager<Resource>* declare(const Resource& r) { return r.manager(); }

/**
	Register a new provider for a resource.

	@tparam Resource the resource type
	@tparam Callable the callable provider
	@tparam Args a sequence of argument types to pass to the callable
	@param r the resource to be provisioned with a provider
	@param func the function called by the new provider
	@param args a sequence of arguments to be given to func at invocation
	@throws config_error if a provider already exists for this resource

	This call registers a new provider for resource `r`. When method
	provide() is invoked on the provider, `func` is called with the same
	number of parameters as passed to `args`. The actual parameters to
	the call are computed as follows:
	* if an argument is a resource instance, the resource will be
	  injected at the time `func` is called.
	* if an argument is any other type, the argument is stored and
	  provided to `func` when it is called.
	The requirements for (non-resource) arguments are the same as in std::bind().
 */
template <typename Resource, typename Callable,	typename ... Args>
inline auto provide(const Resource& r, Callable&& func, Args&& ... args  )
{
	return r.provide(std::forward<Callable>(func), std::forward<Args>(args)...);
}


/**
	Register a new disposer for a resource.

	@tparam Resource the resource type
	@tparam Callable the callable disposer
	@tparam Args a sequence of argument types to pass to the callable
	@param r the resource to be provisioned with a disposer
	@param func the function called by the new disposer
	@param args a sequence of arguments to be given to func at invocation
	@throws config_error if a disposer already exists for this resource

	This is a wrapper function for r.dispose(...).
	
	@see resource::dispose()
	@see provide()
 */
template <typename Resource, typename Callable, typename...Args>
inline auto dispose(const Resource& r, Callable&& func, Args&& ... args )
{
	return r.dispose(std::forward<Callable>(func), std::forward<Args>(args)...);
}



} // end namespace container
