Use flecs 4 syntax


This guide will help migrating Flecs v3 code bases over to Flecs v4. While the guide attempts to be as complete as possible, v4 is a big release, so some things are inevitably missed. If you see something that you think belongs in the migration guide, feel free to create a PR!

Note that this is not a comprehensive list of things that changed in v4. This document only intends to document the breaking changes between v3 and v4.

Queries
The three query implementations in v3 (filter, query, rule) have been merged into a single query API in v4. This means that any usage of filter and rule should now be replaced with query.

Additionally the following things have changed:

Query fields now start from 0 instead of 1(!)
ecs_term_id_t is now called ecs_term_ref_t.
Term ref flags (such as self, up) are now applied with a bitwise OR mask to ecs_term_ref_t::id.
The trav field has been moved from ecs_term_ref_t to ecs_term_t.
When query traversal flags such as up are provided, the traversal relationship defaults to ChildOf (this used to be IsA).
If no traversal flags are provided, the default is still self|up(IsA) for inheritable components.
The ecs_query_desc_t::instanced member no longer exists. Instancing must be specified with a query flag (.flags = EcsQueryIsInstanced).
Stack-allocated query (filter) objects are no longer supported.
It is no longer possible to provide user-allocated term arrays. The max number of terms is now 32 and can be configured with FLECS_TERM_COUNT_MAX.
The ecs_query_changed function has been split up into a function that only accepts a query (ecs_query_changed) and one that only accepts an iterator (ecs_iter_changed).
The ecs_query_skip function has been renamed to ecs_iter_skip.
The ecs_query_set_group function has been renamed to ecs_iter_set_group.
The values in the ecs_iter_t::columns array have changed, and may change again in the near future. Applications should not directly depend on it.
The EcsParent convenience constant is gone, and can now be replaced with just EcsUp.
ecs_query_desc_t::group_by_id has been renamed to ecs_query_desc_t::group_by
ecs_query_desc_t::group_by has been renamed to ecs_query_desc_t::group_by_callback
ecs_query_desc_t::order_by_component has been renamed to ecs_query_desc_t::order_by
ecs_query_desc_t::order_by has been renamed to ecs_query_desc_t::order_by_callback
ecs_query_desc_t::filter no longer exists, its fields (where applicable) have been moved to ecs_query_desc_t.
The ecs_query_next_table and ecs_query_populate functions have been removed.
The ecs_query_table_count and ecs_query_empty_table_count functions have been replaced with ecs_query_count, which now returns a struct.
The ecs_query_t struct is now public, which means that many of the old accessor functions (like ecs_query_get_ctx, ecs_query_get_binding_ctx) are no longer necessary.
The subquery feature has been removed.
ecs_query_fini() won't be called automatically for uncached queries on world finalization.
