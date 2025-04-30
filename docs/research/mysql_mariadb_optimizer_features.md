# MySQL/MariaDB Optimizer Features Analysis

This document summarizes key optimizer features from MySQL and MariaDB that could be integrated into SequoiaDB's query optimization framework.

## 1. Cost-Based Optimization

### 1.1 MySQL Cost Model
- **Configurable Cost Parameters**:
  - Stored in `server_cost` and `engine_cost` tables
  - Dynamically adjustable via `FLUSH OPTIMIZER_COSTS`
  - Includes disk/memory operations, key comparisons, row evaluations

### 1.2 MariaDB Cost Model
- **Engine-Independent Cost Model**:
  - Separates cost calculation from storage engine implementation
  - Supports different cost models for different storage engines
  - Provides cost hints for specific query patterns

## 2. Statistics Collection

### 2.1 MySQL Histogram Statistics
- **Column Statistics**:
  - Stored in `column_statistics` table
  - Supports equi-height histograms
  - Used for accurate selectivity estimation
  - Created with `ANALYZE TABLE ... UPDATE HISTOGRAM` statement

### 2.2 MariaDB Statistics Types
- **Engine-Independent Table Statistics**
- **Histogram-Based Statistics**:
  - Configurable via `histogram_size` and `histogram_type`
  - Supports different histogram types (equi-height, singleton)
- **Index Statistics**:
  - Detailed cardinality information for indexes
  - Used for index selection and join order optimization
- **InnoDB Persistent Statistics**:
  - Stored in system tables for persistence across restarts
  - Automatically updated based on data changes
- **Slow Query Log Extended Statistics**:
  - Collects execution statistics for query optimization

## 3. Join Optimization

### 3.1 MySQL Join Algorithms
- **Nested-Loop Join**:
  - Traditional row-by-row processing
  - Efficient for small result sets
- **Hash Join** (replaced Block Nested-Loop in MySQL 8.0.18+):
  - Builds hash table on smaller table
  - Probes hash table for matches
  - Significantly faster for large joins

### 3.2 MariaDB Join Optimization
- **Hash Join**:
  - Requires explicit enabling via `join_cache_level` parameter
  - Optimized for equality conditions
- **Join Cardinality Estimation**:
  - Uses `hash_join_cardinality` flag for better estimates
  - Considers table statistics for join order selection

### 3.3 Join Order Optimization
- **Dynamic Programming Algorithm**:
  - Evaluates different join orders based on cost
  - Considers table statistics and join conditions
  - Implements heuristics for large join queries

## 4. Subquery Optimization

### 4.1 MariaDB Subquery Techniques
- **Semi-join Subquery Optimizations**:
  - DuplicateWeedout strategy
  - FirstMatch strategy
  - LooseScan strategy
- **Table Pullout Optimization**:
  - Converts subqueries to joins when possible
- **Condition Pushdown Into IN subqueries**:
  - Pushes conditions into subqueries for earlier filtering
- **Subquery Cache**:
  - Caches subquery results for repeated execution
- **EXISTS-to-IN Optimization**:
  - Converts EXISTS predicates to IN for better performance

## 5. Index Selection

### 5.1 Index Merge
- **Union Algorithm**:
  - Combines results from multiple indexes using OR conditions
- **Intersection Algorithm**:
  - Combines results from multiple indexes using AND conditions
- **Sort-union Algorithm**:
  - Special case for complex OR conditions

### 5.2 Optimizer Hints
- **Index Hints**:
  - `USE INDEX`, `FORCE INDEX`, `IGNORE INDEX`
  - Influences index selection without changing query
- **Join Order Hints**:
  - `JOIN_ORDER`, `JOIN_PREFIX`, `JOIN_SUFFIX`
  - Controls join order for specific queries

## 6. Query Rewriting

### 6.1 Common Techniques
- **Subquery Flattening**:
  - Converts subqueries to joins when possible
  - Eliminates unnecessary nested queries
- **View Merging**:
  - Incorporates view definitions into main query
  - Allows optimization across view boundaries
- **Predicate Pushdown**:
  - Moves filtering conditions closer to data sources
  - Reduces intermediate result sizes
- **JOIN Simplification**:
  - Eliminates unnecessary joins
  - Simplifies complex join conditions

## 7. Memory Management

### 7.1 MySQL Memory Management
- **Join Buffer Size**:
  - Configurable via `join_buffer_size`
  - Affects performance of join operations
- **Sort Buffer**:
  - Configurable via `sort_buffer_size`
  - Used for ORDER BY and GROUP BY operations

### 7.2 MariaDB Memory Management
- **Query Cache**:
  - Stores complete result sets for identical queries
  - Configurable via `query_cache_size` and related parameters
- **Join Buffer Pool**:
  - Shared memory pool for join operations
  - More efficient memory utilization than per-join buffers

## 8. Execution Plan Management

### 8.1 MySQL Plan Management
- **Optimizer Trace**:
  - Detailed information about optimization process
  - Helps identify optimization issues
- **EXPLAIN Formats**:
  - Traditional, JSON, and TREE formats
  - Provides detailed execution plan information

### 8.2 MariaDB Plan Management
- **Extended EXPLAIN**:
  - Shows additional optimization information
  - Includes statistics and cost estimates
- **Visual EXPLAIN**:
  - Graphical representation of execution plans
  - Easier to understand complex query plans
