# SequoiaDB Query Optimization Framework Analysis

This document provides a comprehensive analysis of SequoiaDB's current query optimization framework, focusing on key components that could benefit from MySQL/MariaDB optimization techniques integration.

## 1. Cost Model Implementation

SequoiaDB implements a cost-based optimizer with the following characteristics:

### 1.1 Cost Parameters (optCommon.hpp)
- **I/O Cost Parameters**:
  - `OPT_SEQ_SCAN_IO_COST`: Cost for sequential scan I/O operations
  - `OPT_RANDOM_SCAN_IO_COST`: Cost for random scan I/O operations
  - `OPT_IO_CPU_RATE`: Ratio between I/O and CPU costs

- **CPU Cost Parameters**:
  - `OPT_RECORD_CPU_COST`: CPU cost for processing a record
  - `OPT_IDX_CPU_COST`: CPU cost for processing an index entry
  - `OPT_PRED_DEFAULT_CPU_COST`: Default CPU cost for predicate evaluation

- **Selectivity Parameters**:
  - `OPT_PRED_DEFAULT_SELECTIVITY`: Default selectivity (1.0)
  - `OPT_PRED_EQ_DEF_SELECTIVITY`: Default equality predicate selectivity (0.1)
  - `OPT_PRED_RANGE_DEF_SELECTIVITY`: Default range predicate selectivity (0.3)
  - `OPT_PRED_THRESHOLD_SELECTIVITY`: Threshold for considering index scan (0.3)

### 1.2 Cost Calculation (optPlanNode.cpp)
- **Cost Components**:
  - `_estStartCost`: Initial setup cost
  - `_estRunCost`: Execution cost
  - `_estTotalCost`: Combined start and run costs
  - `_estIOCost`: I/O-related cost
  - `_estCPUCost`: CPU processing cost

- **Table Scan Cost Calculation**:
  ```cpp
  _estIOCost = _evalScanIOCost(OPT_SEQ_SCAN_IO_COST, _readPages);
  _estCPUCost = (OPT_RECORD_CPU_COST + _mthCPUCost) * _readRecords;
  ```

- **Index Scan Cost Calculation**:
  ```cpp
  _estIOCost = _evalScanIOCost(OPT_RANDOM_SCAN_IO_COST, _idxReadPages + _readPages);
  _estCPUCost = (_idxReadRecords * (OPT_IDX_CPU_COST + _predCPUCost)) +
                (_readRecords * (OPT_RECORD_CPU_COST + (_needMatch ? _mthCPUCost : 0)));
  ```

## 2. Statistics Collection and Management

SequoiaDB's statistics framework is implemented in optStatUnit.cpp with the following components:

### 2.1 Statistics Types
- **Collection Statistics** (`_optCollectionStat`):
  - Total records count
  - Data pages and size
  - Index pages and size

- **Index Statistics** (`_optIndexStat`):
  - Index pages and levels
  - Key pattern information
  - Uniqueness property

### 2.2 Selectivity Estimation
- **Predicate Evaluation**:
  - Equality predicates: Uses power function of base selectivity
  - Range predicates: Uses different selectivity calculation
  - Compound predicates: Combines individual predicate selectivities

- **Default Selectivity Functions**:
  - `_optEvalDefETSel`: Equality predicate selectivity
  - `_optEvalDefGTSel`: Greater-than predicate selectivity
  - `_optEvalDefLTSel`: Less-than predicate selectivity
  - `_optEvalDefRangeSel`: Range predicate selectivity

### 2.3 Statistics Expiration
- Uses page count and size changes to determine when statistics are stale
- Implements thresholds for small, large, and quick step expiration

## 3. Access Plan Creation and Selection

The access plan creation process is implemented in optAccessPlan.cpp:

### 3.1 Plan Types
- **Table Scan Plans**: Full collection scans
- **Index Scan Plans**: Using indexes for data access
- **Parameterized Plans**: Reusable plans with parameter placeholders

### 3.2 Plan Selection Criteria
- Cost-based selection comparing estimated total costs
- Considers sort requirements and index coverage
- Implements plan caching for similar queries

### 3.3 Plan Caching
- Uses query shape and parameter values for cache key generation
- Implements cache invalidation based on collection changes
- Supports parameterized plans for better cache utilization

## 4. Join Implementation

SequoiaDB primarily implements nested loop joins in qgmOptiNLJoin.cpp:

### 4.1 Join Types
- **Inner Join**: Standard join returning matching records
- **Left Outer Join**: Returns all records from left side with matching right records
- **Right Outer Join**: Implemented as left outer join with swapped inputs

### 4.2 Join Algorithms
- **Nested Loop Join**: Default implementation
- **Hash Join**: Basic support through hints system
  ```cpp
  if (0 == ossStrncmp(itr->value.begin(), QGM_HINT_HASHJOIN, itr->value.size()))
  ```

### 4.3 Join Condition Handling
- Supports equality and inequality conditions
- Implements condition variable creation for join execution
- Allows join reordering for inner joins

## 5. Areas for Potential Improvement

Based on the analysis, the following areas could benefit from MySQL/MariaDB optimization techniques:

### 5.1 Cost Model Refinement
- Current cost parameters are static and not dynamically adjusted
- Limited consideration of memory costs in the optimization process
- Opportunity to implement MySQL's configurable cost parameters

### 5.2 Statistics Collection Enhancement
- Limited histogram support for accurate selectivity estimation
- No automatic statistics collection mechanism
- Could benefit from MySQL/MariaDB's histogram-based statistics

### 5.3 Join Optimization
- Limited join algorithms (primarily nested loop)
- Basic hash join implementation through hints
- No sophisticated join order optimization
- Could adopt MySQL's Block Nested Loop or MariaDB's Hash Join implementations

### 5.4 Query Rewriting
- Limited query transformation capabilities
- No subquery flattening or view merging
- Could implement MySQL/MariaDB's query rewriting rules

### 5.5 Plan Caching
- Basic plan caching mechanism
- No adaptive plan selection based on execution statistics
- Could benefit from MySQL's query plan management features
