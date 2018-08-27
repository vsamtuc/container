#pragma once

#include "container.hh"


namespace cdi {

/**
	Class that provides storage for objects inside context.

	The implementation is based on std:any.
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
	asset(const Value& o) : obj(o) { }

	/**
		Get an object of the provided value stored inside the asset
		@tparam Value the type of the value, which must be CopyConstructible
		@return the stored value
		@throw std::bad_any_cast
	  */
	template <typename Value>
	Value get_object() const { return std::any_cast<Value>(obj); }

	/**
		Get an object of the provided value stored inside the asset
		@tparam Value the type of the value, which must be CopyConstructible
		@return the stored value
		@throw std::bad_any_cast
	  */
	template <typename Value>
	Value& get_object_ref() { return std::any_cast<Value&>(obj); }

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
};


/**
	A container that can materialize resources on demand, providing storage.

	Internally the context holds a map mapping `resourceid`s to `asset`s.
	The main method of a context is `get()`. When the context is asked for a
	resource instance, it first performs a lookup in the map. If not found, it
	initiates a process of instantiation, and stores the result in the map
	(also returning it).

	This class is meant to be used in the implementation of scopes.
  */
class context
{
public:

	/**
		Get a resource instance from this context.

		@tparam Resource the resource type
		@param r the resource
		@return the provided type for the resource
		@throw instantiation_error if something went wrong

		If a resource instance is not already in the context,
		this method calls create() to create one.
	  */
	template <typename Resource>
	typename Resource::instance_type get(const Resource& r) {
		using M = utilities::str_builder;
		using instance_type = typename Resource::instance_type;
		try {
			const asset& ass = asset_map.at(r);
			return ass.get_object<instance_type>();
		} catch(std::out_of_range) {
			return create(r);
		} catch(std::bad_any_cast) {
			throw instantiation_error(M() << "Cyclic instantiation for " << r.id());
		} catch(...) {
			std::throw_with_nested(instantiation_error(M()
				<< "Internal error during instantiation for " << r.id()));
		}
	}

	/**
		Create a resource instance in this context.

		@tparam Resource the resource type
		@param r the resource
		@return the cretaed resource instance
		@throw instantiation_error if something went wrong

		Currently this is only meant to be called by get()
	  */
	template <typename Resource>
	typename Resource::instance_type create(const Resource& r) {
		using instance_type = typename Resource::instance_type;
		// first, add a null entry, so that we can detect cycles!
		auto [iter, succ] = asset_map.emplace(r, std::any());
		assert(succ);
		asset& ass = (*iter).second;
		assert(! ass.object().has_value());

		try {
			resource_manager<Resource>* rm = providence().get_declared(r);
			if(rm==nullptr)
				throw instantiation_error(u::str_builder() << "In creating instance, undeclared resource " << r);
			ass.object() = rm->provide();
			rm->inject(ass.get_object_ref<instance_type>());
			return ass.get_object<instance_type>();
		} catch(...) {
			asset_map.erase(iter);
			std::throw_with_nested(instantiation_error(u::str_builder() << "In providing to " << r));
		}
	}

	/**
	   Empty the context, disposing all resource instances.

	   This method is executed by the destructor as well.
	  */
	void clear() {
		// call dispose on every object in the asset map
		for(auto& [rid, ass] : asset_map) {
			try {
				contextual_base* rm = providence().at(rid);
				rm->dispose_any(ass.object());
			} catch(std::out_of_range) {
				throw disposal_error(u::str_builder()
					<<"Could not obtain resource manager for " << rid
					<< " found in the context!");
			}
		}
		asset_map.clear();
	}

	/**
		Dispose the contents and destroy the context.
		*/
	~context() {
		try {
			clear();
		} catch(...) { }
	}

private:
	resource_map<asset> asset_map;
};


//==================================
//
//  Standard scopes
//
//==================================


/**
	A scope that always returns new resource instances.

	This scope is not associated with a context; every time
	it is asked for an object, it accesses the resource manager
	to provide a new value.

	It is not clear that this scope is useful in applications,
	but it is quite useful in testing.
  */
struct NewScope
{
	/**
		Create a resource instance in this scope.

		@tparam Resource the resource type
		@param r the resource
		@return the new resource instance
		@throw instantiation_error if something went wrong
	  */
	template <typename Resource>
	inline static typename Resource::instance_type get(const Resource& r) {
		static_assert( std::is_same_v<typename Resource::scope, NewScope>,
		"The Resource scope does not match this scope" );
		return declare(r)->provide();
	}
};


/**
	Implementation of scopes that are guarded by the existence of
	at least one object instance.

	@tparam Tag For each different type, a new scope class is defined.

	The Tag type is only used to instantiate different scopes.

	A scope defined by class instances of this template contains an internal
	context where objects are stored.

	A scope defined by a (concrete) class GuardedScope<T> is activatable in a turnstile
	fashion. Each instance of the type GuardScope<T> (on the stack, on the heap, as
	a subclass inside another class, in an array etc.)  increases the turnstile `count()`.
	The scope is active only if the turnstile count in non-zero.

	This class is non-copyable but it is movable (moving it does not affect the turnstile count)
  */
template <typename Tag>
struct GuardedScope
{
	/**
		Constructor, increases the turnstile count.
	  */
	inline GuardedScope() noexcept {  ++_n; }

	/**
		Destructor, decreases the turnstile count.
	  */
	inline ~GuardedScope() {
		-- _n;
		if(_n==0) ctx.clear();
	}

	GuardedScope(const GuardedScope<Tag>&)=delete;
	GuardedScope& operator=(const GuardedScope<Tag>&)=delete;

	/** Move operator (has no effect) */
	inline GuardedScope(GuardedScope&& _other) noexcept { }

	/**
		Static method that returns a resource instance in this scope.
		@tparam Resource the resource type
		@param r the resource
		@return a resource instance from this scope
		@throws inactive_scope_error if the scope is not active
		@throws instantiation_error if contextual instantiation fails
	  */
	template <typename Resource>
	static inline typename Resource::instance_type get(const Resource& r) {
		namespace u = utilities;
		if(! is_active()) throw inactive_scope_error(u::str_builder()
			<< "Trying to allocate " << r.id() << " while scope is inactive");
		return ctx.get(r);
	}

	/**
		Returns true if the scope is active
	  */
	static inline bool is_active() { return _n>0; }

	/**
		Returns the current turnstile count.
	  */
	static inline size_t count() { return _n; }
private:
	static inline size_t _n=0;
	static inline context ctx;
};



/**
	Implementation of scopes which provide a stack of contexts.

	@tparam Tag For each different type, a new scope class is defined.

	The Tag type is only used to instantiate different scopes.

	A scope defined by class instances of this template contains an internal
	context where objects are stored.

	A scope defined by a (concrete) class LocalScope<T> is active
	when at least an instance of this class exists. However, instances
	of LocalScope<T> expect (and throw an exception unless) that they
	have nested lifetimes. This can be ensured by only creating them
	as local variables on the stack, which is the indended use.

	This class is neither copyable nor movable.
  */
template <typename Tag>
class LocalScope
{
public:
	LocalScope() {
		saved_ctx = current_ctx;
		current_ctx = &ctx;
	}
	~LocalScope()
	{
		assert(current_ctx == &ctx);
		current_ctx = saved_ctx;
	}

	/**
		Static method that returns a resource instance in this scope.
		@tparam Resource the resource type
		@param r the resource
		@return a resource instance from this scope
		@throws inactive_scope_error if the scope is not active
		@throws instantiation_error if contextual instantiation fails
	  */
	template <typename Resource>
	static inline typename Resource::instance_type get(const Resource& r) {
		namespace u = utilities;
		if(! is_active()) throw inactive_scope_error(u::str_builder()
			<< "Trying to allocate " << r.id() << " while scope is inactive");
		return current_ctx->get(r);
	}

	/**
		Returns true if the scope is active
	  */
	static inline bool is_active() { return current_ctx!=nullptr; }

private:
	context ctx;
	context* saved_ctx;
	inline static context* current_ctx; // zero-initialized
};


/**
	A class that implements the global scope.

	The global scope is always active. Also, it is the default scope
	for resources. E.g.,
	```
	resource<string> r;
	```
	declares a resource in GlobalScope.
  */
class GlobalScope {
public:

	/**
		Static method that returns a resource instance in this scope.
		@tparam Resource the resource type
		@param r the resource
		@return a resource instance from this scope
		@throws instantiation_error if contextual instantiation fails
	  */
	template <typename Resource>
	static inline typename Resource::instance_type get(const Resource& r) {
		static_assert( std::is_same_v<typename Resource::scope, GlobalScope >,
		"The Resource scope does not match this scope" );

		return global_context.get(r);
	}

	/**
		Clears the global context, disposing of all assets.
	  */
	static void clear() {
		global_context.clear();
	}


//private:
	inline static context global_context;
};

/**
	Convenience declaration for resources in the global scope.
  */
template <typename Value, typename ... Tags>
using Global = resource<Value, GlobalScope, Tags...>;



inline void container::clear() {
	GlobalScope::clear();

	// Delete all resource managers
	for(auto& [rid  ,rm] : rms) {
		(void) rid;//maybe unused?
		delete rm;
	}
	rms.clear();
}


} // end namespace container
