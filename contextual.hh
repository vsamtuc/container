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
	//
	// Implementation-related
	//

	// this call is implemented later by the container
	template <typename Arg, typename Scope, typename ... Tags>
	auto inject_partial(const resource<Arg,Scope,Tags...>&, Phase)
	 -> typename resource<Arg,Scope,Tags...>::return_type;

	// A call is a base class for dependencies of lifecycle calls
	struct call {
		template <typename Arg>
		constexpr Arg unwrap_inject(Phase, Arg arg) {
			return arg;
		}

		template <typename Arg, typename Scope, typename...Tags>
		inline auto unwrap_inject(Phase ph,
			const resource<Arg, Scope, Tags...> & res)
		{
			injected.push_back(res.manager());
			// return std::bind(std::mem_fn(&resource<Arg, Scope, Tags...>::get),
			// 				res, ph);
			return std::bind(inject_partial<Arg,Scope,Tags...>, res, ph);
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
resource<Instance, Scope,Tags...>::initialize(Callable func, Args&& ... args ) const
{
 	resource_manager<resource_type>* rm = manager();
 	rm->initializer(std::forward<Callable>(func), std::forward<Args>(args)...);
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
