#pragma once

#include "container.hh"


namespace cdi {


/**
	A container that can materialize resources on demand, providing storage.

	Internally the context holds a map mapping `resourceid`s to `asset`s.
	The main method of a context is `get()`. When the context is asked for an
	asset, given an rid, it first performs a lookup in the map. If not found, it
	creates an uninitialized asset, and stores the result in the map,
	also returning it.

	This class is meant to be used in the implementation of scopes.
  */
class context
{
public:

	/**
		Get an asset for given rid, creating one if needed.

		@param rid the resource id for this asset
		@return a tuple with pointer to asset and a boolean flag indicating
		if the asset was new.

		Note: the information whether this is a new asset is important!
	  */
	std::tuple<asset*, bool> get(const resourceid& rid)
	{
		auto [iter, isnew] = asset_map.emplace(rid, std::any());
		return { & iter->second, isnew };
	}

	/**
		Remove an asset from the context manually.

		This call does not dispose of the resource; it is assumed that the
		caller has already done so.
		*/
	void drop(const resourceid& rid)
	{
		//  maybe return the value?
		asset_map.erase(rid);
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
				rm->dispose(ass.object());
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

	static std::tuple<asset*, bool> get_asset(const resourceid& rid)
	{
		// This is broken, in case of any cycle, it will cause
		// an endless loop!
		// Soln: figure out some way to return "false" when needed!
		// Possible: some sort of signalling the caller to "make a copy?"
		static asset ass(std::any{});
		ass = asset(std::any());
		return { (&ass) , true };
	}

	static void drop_asset(const resourceid& rid)
	{ }
};


/**
	Implementation of scopes that are activated by the existence of
	at least one object instance.

	@tparam Tag For each different type, a new scope class is defined.

	The Tag type is only used to instantiate different scopes. Any type can
	be used, usually an empty `struct`.

	A scope defined by class instances of this template contains an internal
	context where objects are stored and is activatable in a turnstile
	fashion. Internally, the class instance maintains a (static member) counter,
	available through the static method `count()`.

	Each object instance of the class instance (on the stack,
	on the heap, as a subclass instance, in an array etc.)  increases the
	turnstile `count()`. The scope is active only if the turnstile count
	in non-zero.

	This template's class instances are copyable and movable
	(moving them does not affect the turnstile count)
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
		if(_n==0)
			try {
				ctx.clear();
			} catch(...) { }
	}

	inline GuardedScope(const GuardedScope<Tag>&) noexcept { ++_n; }
	inline GuardedScope& operator=(const GuardedScope<Tag>&) noexcept { ++_n; };

	/** Move operators (have no effect) */
	inline GuardedScope(GuardedScope&& _other) noexcept { }
	inline GuardedScope& operator=(GuardedScope&& _other) noexcept { }


	static inline auto get_asset(const resourceid& rid)
	{
		if(! is_active()) throw inactive_scope_error(u::str_builder()
			<< "Trying to allocate " << rid << " while scope is inactive");
		return ctx.get(rid);
	}

	static inline void drop_asset(const resourceid& rid)
	{
		if(! is_active()) throw inactive_scope_error(u::str_builder()
			<< "Trying to drop " << rid << " while scope is inactive");
		ctx.drop(rid);
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
	of LocalScope<T> expect that (and throw an exception unless) they
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

	static inline std::tuple<asset*, bool> get_asset(const resourceid& rid)
	{
		if(! is_active()) throw inactive_scope_error(u::str_builder()
			<< "Trying to allocate " << rid << " while scope is inactive");
		return current_ctx->get(rid);
	}

	static inline void drop_asset(const resourceid& rid)
	{
		if(! is_active()) throw inactive_scope_error(u::str_builder()
			<< "Trying to drop " << rid << " while scope is inactive");
		current_ctx->drop(rid);
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

	static inline std::tuple<asset*, bool> get_asset(const resourceid& rid)
	{
		return global_context.get(rid);
	}

	static inline void drop_asset(const resourceid& rid)
	{
		global_context.drop(rid);
	}

	/**
		Clears the global context, disposing of all assets.
	  */
	static void clear() {
		global_context.clear();
	}

private:
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
