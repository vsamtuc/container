# TO DO

## Immediate
  - add checking for cycles in the resource table

  - add resource<...> API for put()

  - add convenience API for injection etc.

  - Fix unwrap_inject to allow resource types to be passed
    explicitly

  - fix the code, to handle storing stuff as std::decay_t<>, and getting
     const T&  (think of a string)

  - rename provision_map class to `container`


## Next steps

  - Qualifiers for the resource table affecting provision at runtime.
  	The idea is to share configuration between multiple resources that
  	are "similar". Somehow, this needs to pass the resource to the provider,
  	injectors, etc. 

  - scope determination done at runtime: resource<I , void> types, 
  	but must have qualifier provide the scope!


## Future
  - support multithreading (maybe not too hard)

  - faster qualifiers equality (cache?)

  - User-defined qualifier matching (currently we only support equality).
    Ideas:
    - a `query` class based on matching, as follows:
      Each qual `q` can have a predicate `q.matches(other)`.
      Then, for  `Q` and `Other` qual sets, the predicate
      Q.matches(O) is "forall q in Q, exists o in Other : q.matches(o)
      and forall o in O, exists q in Q: q.matches(o)" 

  - what is the purpose of 'class resource_manager'? All the info is in
    'class contextual'
