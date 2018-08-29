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

class asset;

/**
	A virtual base class defining the API of scopes.
  */
class scope_api
{
public:
	//using qual_base::qual_base;
	virtual std::tuple<asset*, bool>
	 		get(const resourceid&) const =0;
	virtual	void drop(const resourceid&) const =0;
};


inline qualifier scope_spec(const qualifiers&);


/**
	These labels denote the lifecycle of a resource instance.
  */
enum class Phase {
	allocated, //< storage has been obtained (uninitialized)
	provided,  //< storage contains a provided value
	injected,  //< injections have been performed
	created,   //< initialize() has been called
	disposed   //< the instance has been disposed
};


/**
	Class that provides storage for instances inside contexts.

	An asset combines storage for an instance with metadata needed
	by the container. The storage implementation is based on std:any.
	The metadata consists of the phase of this instnance.

	@see Phase
  */
class asset
{
public:
	/**
		Set the asset to a value.

		@tparam Value the type of the value, which must be CopyConstructible
		@param o the value to store
	  */
	template <typename Value>
	asset(const Value& o) : obj(o), ph(Phase::allocated) { }

	/** Return the phase for this asset */
	inline Phase phase() const { return ph; }

	/** Set the phase for this asset */
	inline void set_phase(Phase p) { ph=p; }

	/**
		Get an object of the provided value stored inside the asset
		@tparam Value the type of the value, which must be CopyConstructible
		@return the stored value
		@throw std::bad_any_cast
	  */
	template <typename Value>
	Value get() const { return std::any_cast<Value>(obj); }

	/**
		Get an object of the provided value stored inside the asset
		@tparam Value the type of the value, which must be CopyConstructible
		@return the stored value
		@throw std::bad_any_cast
	  */
	template <typename Value>
	Value& get_ref() { return std::any_cast<Value&>(obj); }

	/**
		Get a reference to the std::any object within the asset.
	  */
	std::any& object() { return obj; }

	/**
		Get a const reference to the std::any object within the asset.
	  */
	const std::any& object() const { return obj; }
private:
	std::any obj;
	Phase ph;
};


// forward
class contextual_base;

/**
	This sequence container is returned by the
	type-erase resource manager, to denote known injections
	of resources to lifecycle calls.
 */
typedef std::vector<contextual_base*> injection_list;

namespace detail {
	template < class, class =std::void_t<> >
	struct has_resource_members : std::false_type { };

	template <class T>
	struct has_resource_members<T, std::void_t<
		typename T::instance_type,
		typename T::return_type
	> > : std::true_type { };
}

/**
	Useful bool constexpr for template metaprogramming,
	checks if a type is a resource type.

	The requirements for a resource type R are the following:
	- R::instance_type  must be defined
	- R::return_type    must be defined
	- std::is_convertible<R, resourceid>, i.e.,
	    resourceid( r )  must work (this can be done if R
	   declares an operator resourceid() cast).

	These requirements are all satisfied by resource<T>.
	However, any user-defined type satisfying the above requirements
	can be used as a resource type.
	*/
template <typename Resource>
inline constexpr bool is_resource_type = std::conjunction_v<
  detail::has_resource_members<Resource>,
  std::is_convertible<Resource, resourceid>
  >;


namespace detail {
	//
	// Implementation-related
	//

	// this call is implemented later by the container
	template <typename Resource>
	auto inject_partial(const Resource&, Phase)
	 -> typename Resource::return_type;

	// A call is a base class for dependencies of lifecycle calls
	struct call {
		template <typename Arg , std::enable_if_t< ! is_resource_type<Arg>, std::true_type>... >
		constexpr Arg unwrap_inject(Phase, Arg arg) {
			return arg;
		}

		template <typename Resource, std::enable_if_t< is_resource_type<Resource>, std::true_type>... >
		inline auto unwrap_inject(Phase ph, const Resource & res)
		{
			injected.push_back(res.manager());
			return std::bind(inject_partial<Resource>, res, ph);
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
	: _rid(r), scopeq(scope_spec(r.quals())) { }

	/** Virtual destructor */
	virtual ~contextual_base() { }

	/** The resource id of the managed resource */
	inline resourceid rid() const { return _rid; }

	/**
		Return the scope API for this resource.
	  */
	inline const scope_api& scope() const { return *scopeq.get<scope_api>(); }

	/**
		Return the scope API for this resource as a qualifier.
	  */
	inline qualifier scope_qual() const { return scopeq; }

	/** Return the set of resources required for instantiation */
	virtual const injection_list& provider_injections() const = 0;

	/** Return the set of resources required for initialization */
	virtual const injection_list& init_injections() const = 0;

	/** Return the set of resources required for disposal */
	virtual const injection_list& disposer_injections() const = 0;

	/** Return true if the resource has an initializer */
	virtual bool has_provider() const =0;

	/** Return true if the resource has an initializer */
	virtual bool has_initializer() const =0;

	/** Return true if the resource has a disposer */
	virtual bool has_disposer() const =0;

	/** Return the number of injections */
	virtual size_t number_of_injectors() const =0;

	/** Return the set of resources required for an injection */
	virtual const injection_list& injector_injections(size_t i) const=0;

	/** Instantiates a resource instance polymorphically. */
	virtual void provide(std::any&) const = 0;

	/** Injects a resource instance polymorphically. */
	virtual void inject(std::any&) const = 0;

	/** Initializes the resource instance polymorphically. */
	virtual void initialize(std::any&) const = 0;

	/** Disposes a resource instance polymorphically. */
	virtual void dispose(std::any&) const = 0;

private:
	resourceid _rid;  // rid
	qualifier scopeq; // scope qualifier
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
 			prov.unwrap_inject(Phase::provided, std::forward<Args>(args))... );
	}

	/** Set the disposer for this contextual */
	template <typename Callable, typename...Args>
	void initializer(Callable&& func, Args&& ... args )
	{
 		using namespace std::placeholders;
 		init.injected.clear();
 		init.func = std::bind(std::forward<Callable>(func),
 			_1, disp.unwrap_inject(Phase::injected, std::forward<Args>(args))... );
	}

	/** Set the disposer for this contextual */
	template <typename Callable, typename...Args>
	void disposer(Callable&& func, Args&& ... args )
	{
 		using namespace std::placeholders;
 		disp.injected.clear();
 		disp.func = std::bind(std::forward<Callable>(func),
 			_1, disp.unwrap_inject(Phase::created, std::forward<Args>(args))... );
	}

	/** Add a new injector for this contextual */
	template <typename Callable, typename...Args>
	void injector(Callable&& func, Args&& ... args )
	{
 		using namespace std::placeholders;
 		injectors.push_back( detail::typed_call<void(instance_type&)>() );
 		auto& inj = injectors.back();
 		inj.func = std::bind(std::forward<Callable>(func),
 			_1, inj.unwrap_inject(Phase::provided, std::forward<Args>(args))... );
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
		if(! prov.func)
			throw instantiation_error(u::str_builder()
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
		Initialize an object.
		@param obj reference to the object to be disposed
	  */
	inline void initialize_instance(instance_type& obj) const {
		if(! init.func) return; // letting the disposer be null is not an error
		init.func(obj);
	}

	/**
		Initialize an object polymorphically.
		@param obj reference to the object to be disposed
	  */
	virtual void initialize(std::any& obj) const override {
		initialize_instance(std::any_cast<instance_type&>(obj));
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

	//================================
	// introspection for optimization
	//================================

	virtual bool has_provider() const override {
		return bool( prov.func );
	}

	virtual bool has_initializer() const override {
		return bool( init.func );
	}

	virtual bool has_disposer() const override {
		return bool( disp.func );
	}

	virtual const injection_list& provider_injections() const override {
		return prov.injected;
	}

	virtual const injection_list& init_injections() const override {
		return init.injected;
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
	std::vector< detail::typed_call<void(instance_type&)> > injectors;
	detail::typed_call<void(instance_type&)> init;
	detail::typed_call<void(instance_type&)> disp;
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
	typedef contextual<instance_type> contextual_type;

	/**
		Construct resource manager for a resource
	  */
	resource_manager(const Resource& r)
	: contextual_type(r)
	{ }


	static inline resource_manager<Resource>* get(const Resource& r);
};


//====================================================
//
// resource management api
//
//====================================================

template <typename Instance>
resource_manager< resource<Instance> >*
resource<Instance>::manager() const {
	//return providence().get(*this);
	return resource_manager< resource<Instance> >::get(*this);

}


template <typename Instance>
template <typename Callable, typename...Args>
const resource<Instance> &
resource<Instance>::provide(Callable func, Args&& ... args ) const
{
 	resource_manager<resource_type>* rm = manager();
 	rm->provider(std::forward<Callable>(func), std::forward<Args>(args)...);
 	return (*this);
}

template <typename Instance>
template <typename Callable, typename...Args>
const resource<Instance> &
resource<Instance>::inject(Callable func, Args&& ... args ) const
{
 	resource_manager<resource_type>* rm = manager();
 	rm->injector(std::forward<Callable>(func), std::forward<Args>(args)...);
 	return (*this);
}

template <typename Instance>
template <typename Callable, typename...Args>
const resource<Instance> &
resource<Instance>::initialize(Callable func, Args&& ... args ) const
{
 	resource_manager<resource_type>* rm = manager();
 	rm->initializer(std::forward<Callable>(func), std::forward<Args>(args)...);
 	return (*this);
}

template <typename Instance>
template <typename Callable, typename...Args>
const resource<Instance> &
resource<Instance>::dispose(Callable func, Args&& ... args ) const
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
inline resource_manager<Resource>* declare(const Resource& r) {
	return resource_manager< Resource >::get(r);
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
inline auto provide(const Resource& r, Callable&& func, Args&& ... args  )
{
	auto rm = declare(r);
	rm->provider(std::forward<Callable>(func), std::forward<Args>(args)...);
	return r;
}

template <typename Resource, typename Callable, typename...Args>
inline auto inject(const Resource& r, Callable&& func, Args&& ... args )
{
	auto rm = declare(r);
	rm->injector(std::forward<Callable>(func), std::forward<Args>(args)...);
	return r;
}

template <typename Resource, typename Callable, typename...Args>
inline auto initialize(const Resource& r, Callable&& func, Args&& ... args )
{
	auto rm = declare(r);
	rm->initializer(std::forward<Callable>(func), std::forward<Args>(args)...);
	return r;
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
	auto rm = declare(r);
	rm->disposer(std::forward<Callable>(func), std::forward<Args>(args)...);
	return r;
}


} // end namespace container
