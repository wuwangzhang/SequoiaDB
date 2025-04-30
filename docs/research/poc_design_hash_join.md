# Hash Join Implementation Design

This document outlines the design for implementing a full-featured hash join algorithm in SequoiaDB, based on MySQL/MariaDB techniques. This implementation will significantly improve join performance for equality conditions.

## 1. Overview

The hash join implementation will:
- Automatically select hash join for suitable queries (equality joins)
- Build a hash table on the smaller relation
- Probe the hash table with records from the larger relation
- Support memory management for large hash tables
- Integrate with existing query optimization framework

## 2. Architecture

### 2.1 Component Diagram

```
+------------------------+      +------------------------+
| Query Parser           |      | Query Optimizer        |
+------------------------+      +------------------------+
           |                              |
           v                              v
+------------------------+      +------------------------+
| Join Type Selection    |----->| Hash Join Operator     |
+------------------------+      +------------------------+
                                          |
                                          v
                                +------------------------+
                                | Hash Table Management  |
                                +------------------------+
                                          |
                                          v
                                +------------------------+
                                | Join Execution         |
                                +------------------------+
```

### 2.2 Key Components

1. **Join Type Selection**: Determines when to use hash join vs. nested loop join
2. **Hash Join Operator**: Implements the hash join algorithm
3. **Hash Table Management**: Handles memory allocation and spilling to disk
4. **Join Execution**: Executes the join operation and returns results

## 3. Implementation Details

### 3.1 New Files to Create

1. **qgmOptiHashJoin.hpp**
   - Class definition for hash join operator
   - Interface with query optimizer

2. **qgmOptiHashJoin.cpp**
   - Implementation of hash join algorithm
   - Hash table building and probing
   - Memory management

3. **hashJoinExecutor.hpp**
   - Runtime execution interface
   - Result handling

4. **hashJoinExecutor.cpp**
   - Runtime execution implementation
   - Performance monitoring

### 3.2 Existing Files to Modify

1. **qgmBuilder.cpp**
   - Add hash join operator creation
   - Modify join type selection logic

2. **optCommon.hpp**
   - Add hash join cost parameters
   - Define hash join memory thresholds

3. **optPlanNode.cpp**
   - Add cost estimation for hash joins
   - Compare with nested loop join costs

4. **rtnQueryOptions.hpp**
   - Add hash join hint support
   - Define hash join configuration options

## 4. Hash Join Algorithm

### 4.1 Build Phase

```cpp
// Pseudocode for build phase
HashTable buildHashTable(Collection smallerCollection, JoinCondition condition) {
    HashTable hashTable(estimateSize(smallerCollection));
    
    for (Record record : smallerCollection) {
        Key key = extractKey(record, condition.leftField);
        hashTable.insert(key, record);
    }
    
    return hashTable;
}
```

### 4.2 Probe Phase

```cpp
// Pseudocode for probe phase
ResultSet probeHashTable(HashTable hashTable, Collection largerCollection, 
                         JoinCondition condition) {
    ResultSet results;
    
    for (Record record : largerCollection) {
        Key key = extractKey(record, condition.rightField);
        RecordList matches = hashTable.lookup(key);
        
        for (Record match : matches) {
            results.add(combineRecords(match, record));
        }
    }
    
    return results;
}
```

### 4.3 Memory Management

```cpp
// Pseudocode for memory management
class HashTable {
private:
    size_t memoryLimit;
    bool spilledToDisk;
    
public:
    void insert(Key key, Record record) {
        if (currentMemoryUsage() > memoryLimit) {
            spillToDisk();
        }
        // Insert into in-memory or disk-based structure
    }
    
    void spillToDisk() {
        // Partition hash table and write to disk
        spilledToDisk = true;
    }
    
    RecordList lookup(Key key) {
        if (spilledToDisk) {
            // Read from appropriate disk partition
        } else {
            // Lookup in memory
        }
    }
};
```

## 5. Join Type Selection

The decision to use hash join will be based on:

1. **Join Condition Type**: Equality conditions only
2. **Table Sizes**: Hash join preferred for larger tables
3. **Memory Availability**: Consider available memory for hash table
4. **Cost Estimation**: Compare estimated costs with nested loop join

```cpp
// Pseudocode for join type selection
JoinType selectJoinType(Collection left, Collection right, JoinCondition condition) {
    if (!condition.isEquality()) {
        return NESTED_LOOP_JOIN;
    }
    
    double hashJoinCost = estimateHashJoinCost(left, right, condition);
    double nestedLoopCost = estimateNestedLoopCost(left, right, condition);
    
    if (hashJoinCost < nestedLoopCost) {
        return HASH_JOIN;
    } else {
        return NESTED_LOOP_JOIN;
    }
}
```

## 6. Cost Model Integration

The hash join cost model will be integrated with the existing cost-based optimizer:

```cpp
// Pseudocode for hash join cost estimation
double estimateHashJoinCost(Collection left, Collection right, JoinCondition condition) {
    // Determine smaller and larger relations
    Collection smaller = (left.size() < right.size()) ? left : right;
    Collection larger = (left.size() < right.size()) ? right : left;
    
    // Build phase cost
    double buildCost = smaller.size() * HASH_BUILD_CPU_COST;
    
    // Probe phase cost
    double probeCost = larger.size() * HASH_PROBE_CPU_COST;
    
    // I/O cost if hash table spills to disk
    double ioCost = 0;
    if (estimateHashTableSize(smaller) > AVAILABLE_MEMORY) {
        ioCost = (smaller.size() + larger.size()) * HASH_SPILL_IO_COST;
    }
    
    return buildCost + probeCost + ioCost;
}
```

## 7. Implementation Plan

1. **Phase 1**: Basic in-memory hash join implementation
   - Create hash join operator classes
   - Implement build and probe phases
   - Add basic join type selection

2. **Phase 2**: Cost model integration
   - Implement cost estimation for hash joins
   - Integrate with optimizer framework
   - Add automatic join type selection

3. **Phase 3**: Memory management
   - Implement memory monitoring
   - Add disk spilling for large hash tables
   - Optimize for different memory configurations

4. **Phase 4**: Testing and optimization
   - Benchmark against nested loop join
   - Optimize hash function and memory usage
   - Fine-tune cost parameters

## 8. Expected Performance Improvements

- **Small-to-Medium Joins**: 2-5x faster than nested loop join
- **Large Joins**: 5-10x faster than nested loop join
- **Memory-Constrained Environments**: 1.5-3x faster with disk spilling
- **Overall**: 30-50% improvement for join-heavy workloads

This implementation will directly address the project goal of achieving a 30%+ improvement in query latency for complex joins.
