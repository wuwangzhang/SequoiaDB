# Histogram-Based Statistics Design

This document outlines the design for implementing histogram-based statistics in SequoiaDB, based on MySQL/MariaDB techniques. This implementation will significantly improve query plan selection through more accurate selectivity estimation.

## 1. Overview

The histogram-based statistics implementation will:
- Collect and maintain column-level statistics
- Generate equi-height histograms for data distribution
- Improve selectivity estimation for query predicates
- Support automatic statistics updates
- Integrate with the existing cost-based optimizer

## 2. Architecture

### 2.1 Component Diagram

```
+------------------------+      +------------------------+
| Statistics Collection  |----->| Histogram Generation   |
+------------------------+      +------------------------+
           |                              |
           v                              v
+------------------------+      +------------------------+
| Statistics Storage     |<---->| Statistics Management  |
+------------------------+      +------------------------+
           |                              |
           v                              v
+------------------------+      +------------------------+
| Selectivity Estimation |<---->| Query Optimizer        |
+------------------------+      +------------------------+
```

### 2.2 Key Components

1. **Statistics Collection**: Samples data from collections
2. **Histogram Generation**: Creates equi-height histograms
3. **Statistics Storage**: Persists statistics in system collections
4. **Statistics Management**: Handles updates and expiration
5. **Selectivity Estimation**: Uses histograms for accurate estimates

## 3. Implementation Details

### 3.1 New Files to Create

1. **optHistogram.hpp**
   - Histogram data structures
   - Bucket management interfaces

2. **optHistogram.cpp**
   - Histogram generation algorithms
   - Bucket distribution calculations

3. **optStatCollector.hpp**
   - Statistics collection interfaces
   - Sampling strategies

4. **optStatCollector.cpp**
   - Statistics collection implementation
   - Data sampling and analysis

### 3.2 Existing Files to Modify

1. **optStatUnit.cpp**
   - Integrate histogram-based selectivity estimation
   - Update predicate evaluation methods

2. **optCommon.hpp**
   - Add histogram configuration parameters
   - Define statistics collection thresholds

3. **optPlanNode.cpp**
   - Use improved selectivity estimates in cost calculations
   - Adjust cost model based on histogram data

4. **rtnCommand.cpp**
   - Add commands for statistics collection and management
   - Implement ANALYZE command functionality

## 4. Histogram Data Structure

### 4.1 Equi-Height Histogram

```cpp
// Pseudocode for histogram data structure
class ColumnHistogram {
private:
    string collectionName;
    string fieldName;
    int numBuckets;
    vector<HistogramBucket> buckets;
    UINT64 sampleSize;
    UINT64 totalValues;
    UINT64 nullCount;
    UINT64 distinctValues;
    OID timestamp;
    
public:
    // Methods for histogram operations
    double estimateSelectivity(BSONElement value, ComparisonOperator op);
    double estimateRangeSelectivity(BSONElement low, BSONElement high);
    void serialize(BSONObjBuilder& builder);
    void deserialize(const BSONObj& obj);
};

class HistogramBucket {
private:
    BSONElement lowerBound;
    BSONElement upperBound;
    UINT64 count;
    UINT64 distinctCount;
    
public:
    // Methods for bucket operations
    double getSelectivity(BSONElement value, ComparisonOperator op);
    bool contains(BSONElement value);
};
```

### 4.2 Statistics Storage Schema

```javascript
// System collection for storing column statistics
{
    "_id": ObjectId(),
    "collection": "collectionName",
    "field": "fieldName",
    "type": "histogram",
    "buckets": [
        {
            "lower": <value>,
            "upper": <value>,
            "count": <number>,
            "distinct": <number>
        },
        // More buckets...
    ],
    "sampleSize": <number>,
    "totalValues": <number>,
    "nullCount": <number>,
    "distinctValues": <number>,
    "timestamp": <timestamp>,
    "version": 1
}
```

## 5. Histogram Generation Algorithm

### 5.1 Data Sampling

```cpp
// Pseudocode for data sampling
vector<BSONElement> sampleData(Collection collection, string fieldName, int sampleSize) {
    vector<BSONElement> samples;
    
    if (collection.size() <= sampleSize) {
        // Sample all records
        for (Record record : collection) {
            samples.push_back(record.getField(fieldName));
        }
    } else {
        // Reservoir sampling
        for (int i = 0; i < sampleSize; i++) {
            samples.push_back(collection.getRecord(i).getField(fieldName));
        }
        
        for (int i = sampleSize; i < collection.size(); i++) {
            int j = rand() % (i + 1);
            if (j < sampleSize) {
                samples[j] = collection.getRecord(i).getField(fieldName);
            }
        }
    }
    
    return samples;
}
```

### 5.2 Equi-Height Histogram Creation

```cpp
// Pseudocode for equi-height histogram creation
ColumnHistogram createHistogram(vector<BSONElement> samples, int numBuckets) {
    // Sort samples
    sort(samples.begin(), samples.end());
    
    // Count distinct values
    int distinctValues = countDistinct(samples);
    
    // Determine bucket size
    int valuesPerBucket = samples.size() / numBuckets;
    
    // Create buckets
    vector<HistogramBucket> buckets;
    int currentBucket = 0;
    int currentCount = 0;
    
    BSONElement bucketStart = samples[0];
    
    for (int i = 0; i < samples.size(); i++) {
        currentCount++;
        
        if (currentCount >= valuesPerBucket && currentBucket < numBuckets - 1) {
            // Create a new bucket
            buckets.push_back(HistogramBucket(bucketStart, samples[i], currentCount));
            
            // Start a new bucket
            bucketStart = samples[i+1];
            currentCount = 0;
            currentBucket++;
        }
    }
    
    // Add the last bucket
    buckets.push_back(HistogramBucket(bucketStart, samples[samples.size()-1], currentCount));
    
    return ColumnHistogram(buckets, samples.size(), distinctValues);
}
```

## 6. Selectivity Estimation

### 6.1 Equality Predicate

```cpp
// Pseudocode for equality selectivity estimation
double estimateEqualitySelectivity(ColumnHistogram histogram, BSONElement value) {
    // Find the bucket containing the value
    for (HistogramBucket bucket : histogram.buckets) {
        if (bucket.contains(value)) {
            // Estimate selectivity within the bucket
            return (1.0 / bucket.distinctCount) * (bucket.count / histogram.totalValues);
        }
    }
    
    // Value not in any bucket (outside the range)
    return 0.0;
}
```

### 6.2 Range Predicate

```cpp
// Pseudocode for range selectivity estimation
double estimateRangeSelectivity(ColumnHistogram histogram, 
                               BSONElement low, BSONElement high) {
    double selectivity = 0.0;
    
    // Find buckets that overlap with the range
    for (HistogramBucket bucket : histogram.buckets) {
        if (bucket.upperBound < low || bucket.lowerBound > high) {
            // Bucket outside range
            continue;
        }
        
        if (bucket.lowerBound >= low && bucket.upperBound <= high) {
            // Bucket fully contained in range
            selectivity += bucket.count / histogram.totalValues;
        } else {
            // Bucket partially overlaps range
            double overlapFraction = calculateOverlap(bucket, low, high);
            selectivity += overlapFraction * (bucket.count / histogram.totalValues);
        }
    }
    
    return selectivity;
}
```

## 7. Statistics Management

### 7.1 Automatic Statistics Collection

```cpp
// Pseudocode for automatic statistics collection
void checkAndUpdateStatistics(Collection collection) {
    // Check if statistics exist
    ColumnStatistics stats = getStatistics(collection);
    
    if (!stats || isStale(stats, collection)) {
        // Collect statistics for important fields
        vector<string> fields = identifyImportantFields(collection);
        
        for (string field : fields) {
            vector<BSONElement> samples = sampleData(collection, field, SAMPLE_SIZE);
            ColumnHistogram histogram = createHistogram(samples, NUM_BUCKETS);
            saveStatistics(collection, field, histogram);
        }
    }
}
```

### 7.2 Statistics Invalidation

```cpp
// Pseudocode for statistics invalidation
bool isStale(ColumnStatistics stats, Collection collection) {
    // Check if collection size has changed significantly
    double changeRatio = abs(stats.totalValues - collection.size()) / (double)stats.totalValues;
    
    if (changeRatio > STALE_THRESHOLD) {
        return true;
    }
    
    // Check if statistics are too old
    UINT64 currentTime = getCurrentTimestamp();
    UINT64 statsAge = currentTime - stats.timestamp;
    
    if (statsAge > MAX_STATS_AGE) {
        return true;
    }
    
    return false;
}
```

## 8. Integration with Query Optimizer

### 8.1 Cost Model Integration

```cpp
// Pseudocode for integrating with cost model
void evaluatePredicate(Predicate predicate, ColumnStatistics stats) {
    double selectivity;
    
    if (stats && stats.hasHistogram()) {
        // Use histogram-based estimation
        if (predicate.isEquality()) {
            selectivity = stats.histogram.estimateSelectivity(
                predicate.getValue(), OPERATOR_EQUALS);
        } else if (predicate.isRange()) {
            selectivity = stats.histogram.estimateRangeSelectivity(
                predicate.getLowValue(), predicate.getHighValue());
        } else {
            // Other operators
            selectivity = estimateOtherOperator(predicate, stats);
        }
    } else {
        // Fall back to default estimation
        selectivity = defaultSelectivityEstimation(predicate);
    }
    
    predicate.setSelectivity(selectivity);
}
```

### 8.2 Command Interface

```cpp
// Pseudocode for ANALYZE command
int analyzeCommand(const string& collectionName, const vector<string>& fields) {
    Collection collection = getCollection(collectionName);
    
    if (!collection) {
        return SDB_DMS_NOTEXIST;
    }
    
    for (string field : fields) {
        vector<BSONElement> samples = sampleData(collection, field, SAMPLE_SIZE);
        ColumnHistogram histogram = createHistogram(samples, NUM_BUCKETS);
        saveStatistics(collection, field, histogram);
    }
    
    return SDB_OK;
}
```

## 9. Implementation Plan

1. **Phase 1**: Basic histogram data structures
   - Implement histogram classes
   - Create statistics storage schema
   - Add basic sampling algorithms

2. **Phase 2**: Selectivity estimation
   - Implement estimation algorithms for different operators
   - Integrate with existing predicate evaluation
   - Test accuracy against actual data distributions

3. **Phase 3**: Statistics management
   - Implement automatic collection mechanism
   - Add staleness detection
   - Create command interface for manual collection

4. **Phase 4**: Optimizer integration
   - Integrate with cost model
   - Adjust plan selection based on improved estimates
   - Benchmark query performance improvements

## 10. Expected Performance Improvements

- **Simple Queries**: 10-15% improvement through better plan selection
- **Complex Queries**: 20-30% improvement for queries with multiple predicates
- **Joins**: 15-25% improvement in join order selection
- **Overall**: 20-30% improvement in query plan quality

This implementation will directly address the project goal of improving selectivity estimation and query plan selection, complementing the hash join implementation for overall query performance enhancement.
