#pragma once

#include <stack>
#include "contextual.hh"

//=================================
//
//  container
//
//=================================


namespace cdi {




/**
	A container is the holder of all resource-related information.

	The container is the only library object that is allocated
	statically  (note: this is currently FALSE!)

	It is the main point of entry for most-all operations
	on resources.
  */

class container
{
public:

	//============================================
	//
	// resource configuration
	//
	//============================================

	/**
		Return a resource manager for a resourceid
		@param rid the resource id for which a resource manager is returned
		@return a resource manager for `rid`
		@throws std::out_of_range if there is no resource manager (the resource is undeclared)
		*/
	inline auto at(const resourceid& rid) { return rms.at(rid); }

	/**
		Return a resource manager for a resource
		@tparam Resource the resource type of r
		@param r the resource for which a resource manager is returned
		@return a resource manager for `r`, or `nullptr`
		*/
	template <typename Resource>
	inline resource_manager<Resource>* get_declared(const Resource& r) {
		try {
			return static_cast<resource_manager<Resource>*>(rms.at(r));
		} catch(std::out_of_range) {
			return nullptr;
		}
	}

	/**
		Return a resource manager for a resource, creating one if needed
		@tparam Resource the resource type of r
		@param r the resource for which a resource manager is returned
		@return a resource manager for `r`

		After this call, the resource `r` is considered declared.
		*/
	template <typename Resource>
	inline resource_manager<Resource>* get(const Resource& r) {
		try {
			return static_cast<resource_manager<Resource>*>(rms.at(r));
		} catch(std::out_of_range) {
			auto rm = new resource_manager<Resource>(r);
			bool succ [[maybe_unused]];
			std::tie(std::ignore, succ) = rms.emplace(r, rm);
			assert(succ); // since we just failed the lookup!
			return rm;
		}
	}

	/**
		Return the collection of all resource managers.
	  */
	inline auto resource_managers() const { return rms; }


	void clear();


	//=========================================
	//
	// resource instantiation
	//
	//==========================================
private:
	struct deferred_work
	{
		asset* ass;
		contextual_base* rm;

		deferred_work(asset* a, contextual_base* r) : ass(a), rm(r) {}

		void inject() {
			assert(ass->phase()>=Phase::provided && ass->phase()<=Phase::created);
			if(ass->phase()==Phase::provided) {
				rm->inject(ass->object());
				ass->set_phase(Phase::injected);
			}
		}
		void create()
		{
			assert(ass->phase()>=Phase::injected && ass->phase()<=Phase::created);
			if(ass->phase()==Phase::injected) {
				rm->initialize(ass->object());
				ass->set_phase(Phase::created);
			}
		}
	};

	std::stack<deferred_work> deferred_injection;
	std::stack<deferred_work> deferred_creation;
	void defer_creation(const deferred_work& work) {
		if(work.ass->phase()==Phase::provided) {
			if(work.rm->number_of_injectors()>0) {
				deferred_injection.push(work);
				return;
			} else {
				work.ass->set_phase(Phase::injected);
			}
		}
		if(work.ass->phase()==Phase::injected) {
			if(work.rm->has_initializer()) {
				deferred_creation.push(work);
				return;
			} else {
				work.ass->set_phase(Phase::created);
			}
		}
		assert(work.ass->phase()==Phase::created);
	}
	inline void defer_creation(asset* ass, contextual_base* rm) {
		defer_creation(deferred_work(ass,rm));
	}

	size_t do_deferred() {
		size_t count=0;
		while(true) {
			if(!deferred_injection.empty()) {
				deferred_work work = deferred_injection.top();
				deferred_injection.pop();
				work.inject();
				count++;
				defer_creation(work);
			}
			else if(!deferred_creation.empty()) {
				deferred_work work = deferred_creation.top();
				deferred_creation.pop();
				work.create();
				count++;
				assert(work.ass->phase()==Phase::created);
			}
			else
				break;
		}
		return count;
	}

public:

	/**
		Get an instance with given phase.

		@param r the resource to return
		@param p the minimum phase of the resource
		@return the resource instance

		This function returns a resource instance for the given
		resource `r`, not necessarily completely created. This call
		is not intended for end-users; it is the main call responsible
		for instantiating resources, and at the heart of the container's
		operation.
	  */
	template <typename Resource>
	inline typename Resource::return_type get(const Resource& r, Phase p) {
		typedef typename Resource::scope scope;

		// Check phase request
		if(p==Phase::allocated || p==Phase::disposed)
			throw instantiation_error(u::str_builder()
				<< "Cannot return an object in "
				<< text_phase(p) << " phase, for " << r);

		resourceid rid = r.id();

		// Get an asset
		auto [ass, isnew] = scope::get_asset(rid);
		assert(ass!=nullptr);

		if(isnew) {
			// New asset, must instantiate
			assert(ass->phase()== Phase::allocated);

			// get the rm
			auto rm = get_declared(r);
			if (rm==nullptr) {
				scope::drop_asset(rid);
				throw instantiation_error(u::str_builder()
				<< "Undeclared resource in instantiating "<< r);
			}

			// build the resource
			try {
				rm->provide( ass->object() );
				ass->set_phase(Phase::provided);
			} catch(...) {
				scope::drop_asset(rid);
				std::throw_with_nested(instantiation_error(u::str_builder()
					<< "Error while instantiating " << r));
			}
			defer_creation(ass, rm);

			// ok, return
			//if(is_top) do_deferred();
			//return ass->asset::get_object<instance_type>();
		} else {
			// must check for cycles from within provider
			if(ass->phase()==Phase::allocated) {
				throw instantiation_error(u::str_builder()
					<< "Cyclical dependency in instantiating " << r);
			}
		}

		// ok, at this point we have a provided asset, execute
		// deferred steps to bring the assets to completion
		while(ass->phase()<p) {
			if(do_deferred()==0)
				// there were no deferred steps executed,
				// there must be a cycle...
				throw instantiation_error(u::str_builder()
					<< "Cyclical dependency in instantiating " << r);
		}
		return ass->asset::get<typename Resource::return_type>();

	}


private:
	resource_map<contextual_base*> rms;


	//=========================================
	//
	// checking the configuration
	//
	//==========================================


	// dependency graph node
	struct rsevent {
		Phase phase;
		resourceid rid;
		std::vector<rsevent*> req;
		bool mark; 	 // for dfs
		size_t ord;  // order in the node vector

		rsevent(Phase ph, const resourceid& r)
		: phase(ph), rid(r), mark(false), ord(0) { }
		inline bool operator==(const rsevent& other) const {
			return phase==other.phase && rid==other.rid;
		}
		rsevent* requires(rsevent* r) {
			this->req.push_back(r);
			return r;
		}
	};
	// A hasher for rsevent
	struct rsevent_hash {
		const std::hash<int> phase_hash;
		const u::hash_code<resourceid> rid_hash;
		inline size_t operator()(const rsevent& n) const {
			size_t seed = 0;
			u::hash_combine(seed, phase_hash((int) (n.phase)));
			u::hash_combine(seed, rid_hash(n.rid));
			return seed;
		}
	};
	// Dependency graph
	struct DepGraph {
		std::vector<rsevent*> nodes;
		std::unordered_set<rsevent, rsevent_hash> index;

		rsevent* get(Phase ph, const resourceid& rid) {
			auto [iter, succ] = index.emplace(ph, rid);
			rsevent* ret = const_cast<rsevent*>(&(*iter));
			if(succ) nodes.push_back(ret);
			return ret;
		}
		void enumerate_nodes() {
			for(size_t i=0; i < nodes.size(); ++i) {
				nodes[i]->ord = i;
				nodes[i]->mark = false;
			}
		}
	};

	void construct_graph(DepGraph& G) const {
		for(auto& [rid, rm] : rms) {
			rsevent* M = G.get(Phase::allocated, rid);
			rsevent* P = G.get(Phase::provided, rid);
			rsevent* I = G.get(Phase::injected, rid);
			rsevent* C = G.get(Phase::created, rid);
			rsevent* D = G.get(Phase::disposed, rid);

			// intra-resource dependencies
			D->requires(C)->requires(I)->requires(P)->requires(M);

			// add inter-resouce dependencies
			for(auto& rid1 : rm->provider_injections())
				P->requires(G.get(Phase::provided, rid1->rid()));
			for(size_t i=0; i < rm->number_of_injectors(); ++i) {
				for(auto& rid1 : rm->injector_injections(i))
					I->requires(G.get(Phase::provided, rid1->rid()));
			}
			for(auto& rid1 : rm->init_injections())
				C->requires(G.get(Phase::injected, rid1->rid()));
			for(auto& rid1 : rm->disposer_injections()) {
				D->requires(G.get(Phase::created, rid1->rid()));
				rsevent* u = G.get(Phase::disposed, rid1->rid());
				// this is a "happens before" constraint which is
				// actually a post-requirement of D
				u->requires(D);
			}
		}
	}

	static void topo_sort_visit(rsevent* node, std::vector<rsevent*>& sorted)
	{
		if(node->mark) return;
		node->mark = true;
		for(auto n : node->req)
			topo_sort_visit(n, sorted);
		sorted.push_back(node);
	}

	// Sort topologically in causal order
	static void topo_sort_graph(DepGraph& G)
	{
		std::vector<rsevent*> sorted;
		sorted.reserve(G.nodes.size()); // avoid allocations

		for(auto node : G.nodes) {
			topo_sort_visit(node, sorted);
		}
		assert(sorted.size() == G.nodes.size());
		G.nodes.swap(sorted);
		G.enumerate_nodes();
	}

	static const char* text_phase(Phase ph) {
		switch(ph) {
		case Phase::allocated:
			return "allocation";
		case Phase::provided:
			return "construction";
		case Phase::injected:
			return "injection";
		case Phase::created:
			return "creation";
		case Phase::disposed:
			return "disposal";
		}
		assert(false);
		return "** unknown phase value **";
	}

public:
	/**
		Check container for consistency.
		@param rstream an output stream where a report is printed
		@return true if the container is consistent

		This call will check the container model and print an analysis
		report to the given stream object.

		The consistency of the container is determined as follows:
		- there are no cyclical dependencies that cannot be satisfied
		- all dependencies are declared
	  */
	bool check_consistency(std::ostream& rstream) {
		// create the causality graph
		DepGraph G;
		construct_graph(G);
		topo_sort_graph(G);

		bool has_cycles = false;
		// report cycles
		for(auto node : G.nodes) {
			for(auto r : node->req)
				if(node->ord <= r->ord) {
					has_cycles = true;
					rstream << "Cyclical dependency: "
					<< node->rid << " " << text_phase(node->phase)
					<< " precedes " << r->rid << " " << text_phase(r->phase)
					<< '\n';
				}
		}
		return !has_cycles;
	}
};

//=========================================
//
// container access
//
//==========================================


inline container& providence() {
	static container c;
	return c;
}

namespace detail {

template <typename Arg, typename Scope, typename ... Tags>
auto inject_partial(const resource<Arg,Scope,Tags...>& r, Phase ph)
 -> typename resource<Arg,Scope,Tags...>::return_type
 {
	 return providence().get(r, ph);
 }

}

template <typename Instance, typename Scope, typename...Tags>
typename resource<Instance,Scope,Tags...>::return_type
resource<Instance,Scope,Tags...>::get() const
{
	return providence().get(*this, Phase::created);
}


template <typename Resource>
inline resource_manager<Resource>* resource_manager<Resource>::get(const Resource& r)
{
	return providence().get(r);
}



} // end namespace cdi
