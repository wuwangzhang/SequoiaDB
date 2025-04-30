# SequoiaDB Benchmark Environment Setup

This document describes how to set up the environment for running the SequoiaDB query optimization benchmarks.

## Prerequisites

- SequoiaDB server installed and running
- SequoiaDB client tools installed
- Node.js (version 12 or higher) for running the benchmark scripts
- At least 8GB of RAM for running the benchmarks
- At least 20GB of free disk space for test data

## Environment Configuration

1. Configure SequoiaDB server with the following settings:

```
# Increase memory allocation for query execution
sdb.conf.modify("maxCacheSize", 2048)
sdb.conf.modify("sortBufferSize", 256)

# Configure optimizer settings
sdb.conf.modify("enableOptimizer", true)
sdb.conf.modify("optimizerTraceLevel", 1)

# Configure statistics collection
sdb.conf.modify("autoStats", true)
sdb.conf.modify("statsSampleRate", 0.1)
```

2. Restart the SequoiaDB server to apply the configuration changes:

```
sdb.shutdown()
sdb.startup()
```

## Test Data Generation

The benchmark scripts include data generation functions that will create the necessary test collections with appropriate data. The following collections will be created:

- `customers`: Customer information
- `orders`: Order information linked to customers
- `products`: Product information
- `order_items`: Order line items linking orders to products
- `suppliers`: Supplier information linked to products
- `categories`: Product categories

The data generation process will create three dataset sizes:

- Small: ~1,000 customers, ~5,000 orders, ~1,000 products
- Medium: ~10,000 customers, ~50,000 orders, ~10,000 products
- Large: ~100,000 customers, ~500,000 orders, ~100,000 products

To generate the test data, run:

```
cd benchmarks/tests
node data_generator.js --size=small
node data_generator.js --size=medium
node data_generator.js --size=large
```

## Benchmark Configuration

The benchmark configuration is stored in `benchmarks/tests/config.js`. You can modify this file to adjust the benchmark parameters:

```javascript
module.exports = {
    // Database connection settings
    connection: {
        host: "localhost",
        port: 11810,
        user: "",
        password: ""
    },
    
    // Test dataset sizes
    datasets: {
        small: {
            customers: 1000,
            orders: 5000,
            products: 1000
        },
        medium: {
            customers: 10000,
            orders: 50000,
            products: 10000
        },
        large: {
            customers: 100000,
            orders: 500000,
            products: 100000
        }
    },
    
    // Concurrency levels for throughput tests
    concurrencyLevels: [1, 5, 10, 20, 50],
    
    // Number of iterations for each test
    iterations: 5,
    
    // Optimizer configurations to test
    optimizerConfigs: [
        {
            name: "baseline",
            enableSubqueryFlattening: false,
            enableHistogramStats: false
        },
        {
            name: "optimized",
            enableSubqueryFlattening: true,
            enableHistogramStats: true
        }
    ]
};
```

## Monitoring Setup

To collect detailed performance metrics during benchmark execution, the following monitoring tools are used:

1. **CPU and Memory Monitoring**: The benchmark scripts include built-in monitoring of CPU and memory usage during query execution.

2. **Query Plan Capture**: The benchmark scripts capture the query execution plans for analysis.

3. **Execution Time Measurement**: The benchmark scripts measure query execution time, optimization time, and total query processing time.

## Running the Benchmarks

After setting up the environment and generating the test data, you can run the benchmarks using the `run_benchmarks.sh` script:

```
cd benchmarks
./run_benchmarks.sh --all-scenarios --output-dir results/latest
```

See the `run_benchmarks.sh` documentation for more options and details.

## Troubleshooting

If you encounter issues running the benchmarks, check the following:

1. Ensure the SequoiaDB server is running and accessible
2. Verify that the test data has been generated correctly
3. Check the SequoiaDB server logs for any errors
4. Ensure sufficient system resources (memory, disk space) are available
5. Verify that the benchmark configuration is correct
