#pragma once

#include "resource.hh"

//=================================
//
//  Basic provider
//
//=================================

namespace cdi {

using M = utilities::str_builder;


/**
	Base class for resource managers.

	This class provides a type-erased management interface.
  */
struct basic_resource_manager
{
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
	inline const resource_set& requires() const { return req; }
	/** Add a requirement to the set of resources required for instantiation.
		This method is call when a provider is registered.
	  */
	void add_requirement(const resourceid& res) { req.insert(res); }

	/** Instantiates a resource instance polymorphically. */
	virtual std::any provide_any() const = 0;
	/** Disposes a resource instance polymorphically. */
	virtual void dispose_any(std::any&) const = 0;

private:
	resourceid _rid;
	resource_set req;	
};


/**
  	A resource provider is a function that instantiates resources.
  	This is a virtual base class.
  */
template <typename Value>
using provider = std::function< Value () >;

/**
  	A resource disposer is a function that disposes resource instances.
  	This is a virtual base class.
  */
template <typename Value>
using disposer = std::function< void(Value&) >;


/**
	A type manager knows how to provide and dispose values of a certain
	type.
  */
template <typename Value>
struct type_manager 
{
	provider<Value> prov;   //< Provider function
	disposer<Value> disp;	//< Disposer function
};

/**
	A resource manager manages the lifecycle of resources.
  */
template <typename Resource>
struct resource_manager : basic_resource_manager, 
	type_manager<typename Resource::provided_type>
{
public:
	/// the provided type of the managed resource
	typedef typename Resource::provided_type provided_type;
	/// the scope of the managed resource
	typedef typename Resource::scope scope;

	/**
		Construct resource manager for a resource
	  */
	resource_manager(const Resource& r)
	: basic_resource_manager(r.id())
	{ }

	using type_manager<provided_type>::prov;
	using type_manager<provided_type>::disp;

	/**
		Return a new instance from the provider.

		@return a new instance of the resource
		@throw instantiation_error if a provider is not set.
	  */
	inline provided_type provide() const { 
		if(! prov) throw instantiation_error(M() << "A provider is not set for resource " << rid());
		return prov();
	}

	/**
		Return a new instance from the provider polymorphically.

		@return a new instance of the resource
		@throw instantiation_error if a provider is not set.
	  */
	inline std::any provide_any() const {
		return std::any(provide());
	}

	/**
		Dispose of an object.
		@param obj reference to the object to be disposed
	  */
	inline void dispose(provided_type& obj) const {
		if(! disp) return; // letting the disposer be null is not an error
		disp(obj);
	}

	/**
		Dispose of an object polymorhically.
		@param obj reference to the object to be disposed
	  */
	virtual void dispose_any(std::any& obj) const override {
		dispose(std::any_cast<provided_type&>(obj));
	}

};


struct provision_map : resource_map<basic_resource_manager*> {
	template <typename Resource>
	inline resource_manager<Resource>* get(const Resource& r) {
		basic_resource_manager* p;
		try {
			p = this->lookup(r);
			return static_cast<resource_manager<Resource>*>(p);
		} catch(std::out_of_range) {
			auto rm = new resource_manager<Resource>(r); 
			bool succ [[maybe_unused]];
			std::tie(std::ignore, succ) = bind(r, rm);
			assert(succ); // since we just failed the lookup!
			return rm;
		}
	}
	friend class ProvisionScope;
};
static inline provision_map prov_map;



/**
	A resource_manager_map contains the collection of resource managers.
  */
struct ProvisionScope 
{
	/**
		Declares the scope interface, but only supports one type of resource
	  */
	template <typename Value, typename ...Tags>
	static inline 
	typename resource<Value, ProvisionScope, Tags...>::provided_type 
	get(const resource<Value, ProvisionScope, Tags...>& r);

	template <typename Resource, typename ...Tags>
	static inline resource_manager<Resource>* 
	get(const resource< resource_manager<Resource>*  , ProvisionScope, Tags...>& r)
	{
		return prov_map.get(r);
	}

	static void clear() { prov_map.clear(); }

private:
};



/**
	Return the resource manager for a resource
	@tparam Resource the resource type
	@param r the resource
	@return the resource manager for this resource
  */
template <typename Resource>
resource_manager<Resource>* declare(const Resource& r) {
	return prov_map.get(r);
}

template <typename Value, typename Scope, typename...Tags>
resource_manager< resource<Value,Scope,Tags...> >* 
resource<Value,Scope,Tags...>::manager() const { return declare(*this); }


template <typename Arg>
constexpr Arg unwrap_inject(Arg arg) {  return arg;  }

template <typename Arg, typename Scope, typename...Tags>
inline auto  unwrap_inject(const resource<Arg, Scope, Tags...> & res) 
{ 
	return std::bind(inject< resource<Arg, Scope, Tags...> >, res);
}


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
void provide(const Resource& r, Callable func, Args&& ... args  )
{
 	resource_manager<Resource>* rm = declare(r);
 	assert(rm!=nullptr);

 	// this is also checked
 	if(rm->prov)
 		throw config_error(M() << "A provider already exists for " << r.id());

 	rm->prov = std::bind(func, unwrap_inject(std::forward<Args>(args))... );
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

	This call registers a new disposer for resource `r`. When method
	dispose(obj) is invoked on the disposer, `func` is called with a non-const reference
	to obj as the first argument followed by the same
	number of parameters as passed to `args`. The actual parameters following obj
	call are computed as for provide()

	@see provide()
 */
template <typename Resource, typename Callable, typename...Args>
void dispose(const Resource& r, Callable func, Args&& ... args )
{
 	resource_manager<Resource>* rm = declare(r);
 	assert(rm!=nullptr);

 	// this is also checked
 	if(rm->disp)
 	 		throw config_error(M() << "A disposer already exists for " << r.id());

 	using namespace std::placeholders;
 	rm->disp = std::bind(func, _1, unwrap_inject(std::forward<Args>(args))... );
}



} // end namespace container
