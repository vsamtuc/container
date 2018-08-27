#pragma once

#include "contextual.hh"

//=================================
//
//  container
//
//=================================


namespace cdi {


/**
	Class that provides storage for instances inside contexts.

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
	A container is the holder of all resource-related information.

	The container is the only library object that is allocated
	statically  (note: this is currently FALSE!)

	It is the main point of entry for most-all operations
	on resources.
  */

class container
{
public:

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
private:
	resource_map<contextual_base*> rms;

	enum class Phase { managed, provided, injected, created, disposed };

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
			rsevent* M = G.get(Phase::managed, rid);
			rsevent* P = G.get(Phase::provided, rid);
			rsevent* I = G.get(Phase::injected, rid);
			rsevent* C = G.get(Phase::created, rid);
			rsevent* D = G.get(Phase::disposed, rid);

			// intra-resource dependencies
			D->requires(C)->requires(I)->requires(P)->requires(M);

			// add inter-resouce dependencies
			for(auto& rid1 : rm->provider_injections())
				P->requires(G.get(Phase::provided, rid1->rid()));
			for(auto& rid1 : rm->disposer_injections()) {
				D->requires(G.get(Phase::created, rid1->rid()));
				rsevent* u = G.get(Phase::disposed, rid1->rid());
				// this is a "happens before" constraint which is
				// actually a post-requirement of D
				u->requires(D);
			}
			for(size_t i=0; i < rm->number_of_injectors(); ++i) {
				for(auto& rid1 : rm->injector_injections(i))
					I->requires(G.get(Phase::provided, rid1->rid()));
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
		case Phase::managed:
			return "declaration";
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


inline container& providence() {
	static container c;
	return c;
}

template <typename Resource>
inline resource_manager<Resource>* resource_manager<Resource>::get(const Resource& r)
{
	return providence().get(r);
}



} // end namespace cdi
