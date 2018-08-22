# TO DO

## Immediate
  - unit testing with non-copyable resources (unique_ptr)
  - add checking for cycles in the resource table
  - Fix unwrap_inject to allow resource types to be passed
    explicitly
  - Add "New" scope, for things like access to attributes of
    resource instances


## Next steps
  - two-phase construction
  - faster qualifiers equality (cache?)
  - qualifiers for the resource table affecting provision at runtime

## Future
  - support multithreading (maybe too hard?)
  - User-defined qualifier matching (currently we only support equality).
    Ideas:
    - a `query` class based on matching, as follows:
      Each qual `q` can have a predicate `q.matches(other)`.
      Then, for  `Q` and `Other` qual sets, the predicate
      Q.matches(O) is "forall q in Q, exists o in Other : q.matches(o)
      and forall o in O, exists q in Q: q.matches(o)" 

