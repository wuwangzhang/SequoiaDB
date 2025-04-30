/**
 * join_benchmark.js
 * 
 * Benchmark tests for evaluating join optimization improvements in SequoiaDB
 * Tests focus on complex joins with varying data sizes and concurrency levels
 */

const CONFIG = {
    collections: {
        customers: "customers",
        orders: "orders",
        products: "products",
        orderItems: "orderItems",
        suppliers: "suppliers"
    },
    
    dataSizes: {
        small: {
            customers: 1000,
            orders: 10000,
            products: 1000,
            orderItems: 20000,
            suppliers: 100
        },
        medium: {
            customers: 10000,
            orders: 100000,
            products: 10000,
            orderItems: 200000,
            suppliers: 1000
        },
        large: {
            customers: 100000,
            orders: 1000000,
            products: 100000,
            orderItems: 2000000,
            suppliers: 10000
        }
    },
    
    concurrencyLevels: [1, 5, 10, 20, 50],
    
    iterations: 5,
    
    timeout: 300000 // 5 minutes
};

function createConnection() {
    var conn = new Sdb("localhost", 11810);
    return conn;
}

function dropCollections(conn) {
    try {
        var cs = conn.getCS("benchmark");
        for (var coll in CONFIG.collections) {
            try {
                cs.dropCL(CONFIG.collections[coll]);
            } catch (e) {
            }
        }
    } catch (e) {
        conn.createCS("benchmark");
    }
}

function createSchema(conn, size) {
    var cs = conn.getCS("benchmark");
    
    var customers = cs.createCL(CONFIG.collections.customers);
    customers.createIndex("customer_id_idx", { customer_id: 1 }, true, false);
    
    var orders = cs.createCL(CONFIG.collections.orders);
    orders.createIndex("order_id_idx", { order_id: 1 }, true, false);
    orders.createIndex("customer_id_idx", { customer_id: 1 }, false, false);
    orders.createIndex("order_date_idx", { order_date: 1 }, false, false);
    
    var products = cs.createCL(CONFIG.collections.products);
    products.createIndex("product_id_idx", { product_id: 1 }, true, false);
    products.createIndex("supplier_id_idx", { supplier_id: 1 }, false, false);
    products.createIndex("category_idx", { category: 1 }, false, false);
    
    var orderItems = cs.createCL(CONFIG.collections.orderItems);
    orderItems.createIndex("order_id_idx", { order_id: 1 }, false, false);
    orderItems.createIndex("product_id_idx", { product_id: 1 }, false, false);
    
    var suppliers = cs.createCL(CONFIG.collections.suppliers);
    suppliers.createIndex("supplier_id_idx", { supplier_id: 1 }, true, false);
    
    generateData(conn, size);
}

function generateData(conn, size) {
    var cs = conn.getCS("benchmark");
    var dataSize = CONFIG.dataSizes[size];
    
    var customers = cs.getCL(CONFIG.collections.customers);
    var customerBatch = [];
    for (var i = 0; i < dataSize.customers; i++) {
        customerBatch.push({
            customer_id: i,
            name: "Customer " + i,
            email: "customer" + i + "@example.com",
            address: "Address " + i,
            phone: "555-" + (10000 + i)
        });
        
        if (customerBatch.length >= 1000) {
            customers.bulkInsert(customerBatch);
            customerBatch = [];
        }
    }
    if (customerBatch.length > 0) {
        customers.bulkInsert(customerBatch);
    }
    
    var suppliers = cs.getCL(CONFIG.collections.suppliers);
    var supplierBatch = [];
    for (var i = 0; i < dataSize.suppliers; i++) {
        supplierBatch.push({
            supplier_id: i,
            name: "Supplier " + i,
            contact: "Contact " + i,
            address: "Supplier Address " + i
        });
        
        if (supplierBatch.length >= 1000) {
            suppliers.bulkInsert(supplierBatch);
            supplierBatch = [];
        }
    }
    if (supplierBatch.length > 0) {
        suppliers.bulkInsert(supplierBatch);
    }
    
    var products = cs.getCL(CONFIG.collections.products);
    var productBatch = [];
    var categories = ["Electronics", "Clothing", "Food", "Books", "Home"];
    for (var i = 0; i < dataSize.products; i++) {
        productBatch.push({
            product_id: i,
            name: "Product " + i,
            price: Math.random() * 1000,
            category: categories[i % categories.length],
            supplier_id: i % dataSize.suppliers
        });
        
        if (productBatch.length >= 1000) {
            products.bulkInsert(productBatch);
            productBatch = [];
        }
    }
    if (productBatch.length > 0) {
        products.bulkInsert(productBatch);
    }
    
    var orders = cs.getCL(CONFIG.collections.orders);
    var orderItems = cs.getCL(CONFIG.collections.orderItems);
    var orderBatch = [];
    var orderItemBatch = [];
    
    var startDate = new Date(2020, 0, 1).getTime();
    var endDate = new Date().getTime();
    
    for (var i = 0; i < dataSize.orders; i++) {
        var orderDate = new Date(startDate + Math.random() * (endDate - startDate));
        var customerId = Math.floor(Math.random() * dataSize.customers);
        
        orderBatch.push({
            order_id: i,
            customer_id: customerId,
            order_date: orderDate,
            status: ["Pending", "Shipped", "Delivered"][Math.floor(Math.random() * 3)],
            total_amount: 0 // Will be updated after order items are generated
        });
        
        if (orderBatch.length >= 1000) {
            orders.bulkInsert(orderBatch);
            orderBatch = [];
        }
        
        var itemCount = 1 + Math.floor(Math.random() * 5);
        var totalAmount = 0;
        
        for (var j = 0; j < itemCount; j++) {
            var productId = Math.floor(Math.random() * dataSize.products);
            var quantity = 1 + Math.floor(Math.random() * 5);
            var price = 10 + Math.random() * 990; // Random price between 10 and 1000
            
            totalAmount += price * quantity;
            
            orderItemBatch.push({
                order_id: i,
                product_id: productId,
                quantity: quantity,
                price: price
            });
            
            if (orderItemBatch.length >= 1000) {
                orderItems.bulkInsert(orderItemBatch);
                orderItemBatch = [];
            }
        }
        
        orders.update({ order_id: i }, { $set: { total_amount: totalAmount } });
    }
    
    if (orderBatch.length > 0) {
        orders.bulkInsert(orderBatch);
    }
    
    if (orderItemBatch.length > 0) {
        orderItems.bulkInsert(orderItemBatch);
    }
}

const QUERIES = {
    simpleJoin: function(conn) {
        var cs = conn.getCS("benchmark");
        return cs.exec("SELECT o.order_id, o.order_date, c.name AS customer_name " +
                      "FROM orders o JOIN customers c ON o.customer_id = c.customer_id " +
                      "WHERE o.total_amount > 500 " +
                      "ORDER BY o.order_date DESC " +
                      "LIMIT 100");
    },
    
    threeWayJoin: function(conn) {
        var cs = conn.getCS("benchmark");
        return cs.exec("SELECT o.order_id, c.name AS customer_name, p.name AS product_name, oi.quantity, oi.price " +
                      "FROM orders o " +
                      "JOIN customers c ON o.customer_id = c.customer_id " +
                      "JOIN orderItems oi ON o.order_id = oi.order_id " +
                      "JOIN products p ON oi.product_id = p.product_id " +
                      "WHERE o.status = 'Delivered' AND p.category = 'Electronics' " +
                      "ORDER BY o.order_date DESC " +
                      "LIMIT 100");
    },
    
    fourWayJoinWithAggregation: function(conn) {
        var cs = conn.getCS("benchmark");
        return cs.exec("SELECT c.name AS customer_name, p.category, SUM(oi.quantity * oi.price) AS total_spent " +
                      "FROM customers c " +
                      "JOIN orders o ON c.customer_id = o.customer_id " +
                      "JOIN orderItems oi ON o.order_id = oi.order_id " +
                      "JOIN products p ON oi.product_id = p.product_id " +
                      "WHERE o.order_date > '2022-01-01' " +
                      "GROUP BY c.name, p.category " +
                      "ORDER BY total_spent DESC " +
                      "LIMIT 100");
    },
    
    joinWithSubquery: function(conn) {
        var cs = conn.getCS("benchmark");
        return cs.exec("SELECT c.name AS customer_name, " +
                      "(SELECT COUNT(*) FROM orders o WHERE o.customer_id = c.customer_id) AS order_count, " +
                      "(SELECT SUM(oi.quantity * oi.price) FROM orders o JOIN orderItems oi ON o.order_id = oi.order_id " +
                      " WHERE o.customer_id = c.customer_id) AS total_spent " +
                      "FROM customers c " +
                      "WHERE c.customer_id IN (SELECT DISTINCT o.customer_id FROM orders o WHERE o.status = 'Delivered') " +
                      "ORDER BY total_spent DESC " +
                      "LIMIT 100");
    },
    
    joinWithComplexFiltering: function(conn) {
        var cs = conn.getCS("benchmark");
        return cs.exec("SELECT o.order_id, c.name AS customer_name, p.name AS product_name, " +
                      "s.name AS supplier_name, oi.quantity, oi.price " +
                      "FROM orders o " +
                      "JOIN customers c ON o.customer_id = c.customer_id " +
                      "JOIN orderItems oi ON o.order_id = oi.order_id " +
                      "JOIN products p ON oi.product_id = p.product_id " +
                      "JOIN suppliers s ON p.supplier_id = s.supplier_id " +
                      "WHERE o.order_date BETWEEN '2022-01-01' AND '2022-12-31' " +
                      "AND p.category IN ('Electronics', 'Books') " +
                      "AND oi.quantity > 2 " +
                      "ORDER BY o.order_date DESC " +
                      "LIMIT 100");
    }
};

function runBenchmark(size, enableOptimizer) {
    var results = {
        size: size,
        optimizerEnabled: enableOptimizer,
        queries: {}
    };
    
    var conn = createConnection();
    
    if (enableOptimizer) {
        conn.execute("SET OPTIMIZER_ENABLE_HASH_JOIN = true");
        conn.execute("SET OPTIMIZER_ENABLE_HISTOGRAM = true");
    } else {
        conn.execute("SET OPTIMIZER_ENABLE_HASH_JOIN = false");
        conn.execute("SET OPTIMIZER_ENABLE_HISTOGRAM = false");
    }
    
    for (var queryName in QUERIES) {
        var queryFunc = QUERIES[queryName];
        var queryResults = [];
        
        for (var i = 0; i < CONFIG.iterations; i++) {
            var startTime = new Date().getTime();
            var memoryBefore = conn.execute("ADMIN GET MEMORY USAGE")[0].totalMemory;
            
            try {
                var result = queryFunc(conn);
                var endTime = new Date().getTime();
                var memoryAfter = conn.execute("ADMIN GET MEMORY USAGE")[0].totalMemory;
                
                queryResults.push({
                    iteration: i,
                    executionTime: endTime - startTime,
                    resultCount: result.length,
                    memoryUsage: memoryAfter - memoryBefore,
                    success: true
                });
            } catch (e) {
                var endTime = new Date().getTime();
                queryResults.push({
                    iteration: i,
                    executionTime: endTime - startTime,
                    error: e.message,
                    success: false
                });
            }
        }
        
        var executionTimes = queryResults.filter(r => r.success).map(r => r.executionTime);
        var memoryUsages = queryResults.filter(r => r.success).map(r => r.memoryUsage);
        
        results.queries[queryName] = {
            iterations: queryResults,
            stats: {
                avgExecutionTime: executionTimes.reduce((a, b) => a + b, 0) / executionTimes.length,
                minExecutionTime: Math.min(...executionTimes),
                maxExecutionTime: Math.max(...executionTimes),
                avgMemoryUsage: memoryUsages.reduce((a, b) => a + b, 0) / memoryUsages.length,
                successRate: queryResults.filter(r => r.success).length / queryResults.length
            }
        };
    }
    
    conn.close();
    return results;
}

function runThroughputTest(size, concurrencyLevel, enableOptimizer) {
    var results = {
        size: size,
        concurrencyLevel: concurrencyLevel,
        optimizerEnabled: enableOptimizer,
        queries: {}
    };
    
    var setupConn = createConnection();
    if (enableOptimizer) {
        setupConn.execute("SET GLOBAL OPTIMIZER_ENABLE_HASH_JOIN = true");
        setupConn.execute("SET GLOBAL OPTIMIZER_ENABLE_HISTOGRAM = true");
    } else {
        setupConn.execute("SET GLOBAL OPTIMIZER_ENABLE_HASH_JOIN = false");
        setupConn.execute("SET GLOBAL OPTIMIZER_ENABLE_HISTOGRAM = false");
    }
    setupConn.close();
    
    for (var queryName in QUERIES) {
        var queryFunc = QUERIES[queryName];
        var startTime = new Date().getTime();
        var totalQueries = 0;
        var successfulQueries = 0;
        
        var workers = [];
        for (var i = 0; i < concurrencyLevel; i++) {
            workers.push(createConnection());
        }
        
        var runUntil = startTime + 60000; // 1 minute
        var promises = [];
        
        for (var i = 0; i < concurrencyLevel; i++) {
            promises.push(runWorker(workers[i], queryFunc, runUntil));
        }
        
        var workerResults = Promise.all(promises);
        
        for (var i = 0; i < workerResults.length; i++) {
            totalQueries += workerResults[i].totalQueries;
            successfulQueries += workerResults[i].successfulQueries;
        }
        
        var endTime = new Date().getTime();
        var durationSeconds = (endTime - startTime) / 1000;
        
        results.queries[queryName] = {
            durationSeconds: durationSeconds,
            totalQueries: totalQueries,
            successfulQueries: successfulQueries,
            queriesPerSecond: totalQueries / durationSeconds,
            successRate: successfulQueries / totalQueries
        };
        
        for (var i = 0; i < workers.length; i++) {
            workers[i].close();
        }
    }
    
    return results;
}

function runWorker(conn, queryFunc, runUntil) {
    var result = {
        totalQueries: 0,
        successfulQueries: 0
    };
    
    while (new Date().getTime() < runUntil) {
        result.totalQueries++;
        try {
            queryFunc(conn);
            result.successfulQueries++;
        } catch (e) {
        }
    }
    
    return result;
}

function main() {
    var results = {
        timestamp: new Date().toISOString(),
        latencyTests: [],
        throughputTests: []
    };
    
    var conn = createConnection();
    
    for (var size in CONFIG.dataSizes) {
        print("Setting up " + size + " dataset...");
        dropCollections(conn);
        createSchema(conn, size);
        
        print("Running latency tests for " + size + " dataset...");
        results.latencyTests.push(runBenchmark(size, false)); // Without optimizer
        results.latencyTests.push(runBenchmark(size, true));  // With optimizer
        
        print("Running throughput tests for " + size + " dataset...");
        for (var i = 0; i < CONFIG.concurrencyLevels.length; i++) {
            var concurrencyLevel = CONFIG.concurrencyLevels[i];
            print("  Concurrency level: " + concurrencyLevel);
            
            results.throughputTests.push(runThroughputTest(size, concurrencyLevel, false)); // Without optimizer
            results.throughputTests.push(runThroughputTest(size, concurrencyLevel, true));  // With optimizer
        }
    }
    
    conn.close();
    
    var resultsJson = JSON.stringify(results, null, 2);
    print(resultsJson);
    
    return results;
}

main();
