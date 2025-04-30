/**
 * config.js
 * 
 * Configuration for SequoiaDB query optimization benchmarks
 */

module.exports = {
    connection: {
        host: "localhost",
        port: 11810,
        user: "",
        password: ""
    },
    
    datasets: {
        small: {
            customers: 1000,
            orders: 5000,
            products: 1000,
            order_items: 15000,
            suppliers: 100,
            categories: 50
        },
        medium: {
            customers: 10000,
            orders: 50000,
            products: 10000,
            order_items: 150000,
            suppliers: 1000,
            categories: 200
        },
        large: {
            customers: 100000,
            orders: 500000,
            products: 100000,
            order_items: 1500000,
            suppliers: 10000,
            categories: 1000
        }
    },
    
    concurrencyLevels: [1, 5, 10, 20, 50],
    
    iterations: 5,
    
    optimizerConfigs: [
        {
            name: "baseline",
            description: "Original SequoiaDB Optimizer",
            enableSubqueryFlattening: false,
            enableHistogramStats: false,
            enableHashJoin: false
        },
        {
            name: "optimized",
            description: "MySQL/MariaDB-Inspired Optimizer",
            enableSubqueryFlattening: true,
            enableHistogramStats: true,
            enableHashJoin: true
        }
    ],
    
    queryCategories: [
        {
            name: "simple_joins",
            description: "Simple joins between two collections"
        },
        {
            name: "three_way_joins",
            description: "Three-way joins with filtering"
        },
        {
            name: "four_way_joins",
            description: "Four-way joins with aggregation"
        },
        {
            name: "subquery_joins",
            description: "Joins with subqueries"
        },
        {
            name: "complex_filtering",
            description: "Joins with complex filtering conditions"
        }
    ],
    
    outputDirs: {
        rawData: "../results/raw_data",
        charts: "../results/charts",
        reports: "../results"
    },
    
    targets: {
        queryLatency: 30, // 30% improvement
        throughput: 20,   // 20% improvement
        optimizationTime: 5,  // <5% increase
        memoryFootprint: 10   // <10% increase
    }
};
