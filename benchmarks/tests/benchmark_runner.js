/**
 * benchmark_runner.js
 * 
 * Runner script for executing join optimization benchmarks
 * and collecting performance metrics
 */

load("join_benchmark.js");

const CONFIG = {
    resultsFile: "../results/benchmark_results.json",
    
    testConfigs: [
        { size: "small", enableOptimizer: false, description: "Baseline - Small Dataset" },
        { size: "small", enableOptimizer: true, description: "Optimized - Small Dataset" },
        { size: "medium", enableOptimizer: false, description: "Baseline - Medium Dataset" },
        { size: "medium", enableOptimizer: true, description: "Optimized - Medium Dataset" }
    ],
    
    concurrencyLevels: [1, 5, 10],
    
    iterations: 3
};

function runBenchmarks() {
    print("Starting benchmark tests...");
    
    var results = {
        timestamp: new Date().toISOString(),
        environment: getEnvironmentInfo(),
        latencyTests: [],
        throughputTests: []
    };
    
    print("Running latency tests...");
    for (var i = 0; i < CONFIG.testConfigs.length; i++) {
        var config = CONFIG.testConfigs[i];
        print("  Running " + config.description);
        
        var latencyResult = runLatencyBenchmark(
            config.size, 
            config.enableOptimizer, 
            CONFIG.iterations
        );
        
        latencyResult.description = config.description;
        results.latencyTests.push(latencyResult);
    }
    
    print("Running throughput tests...");
    for (var i = 0; i < CONFIG.testConfigs.length; i++) {
        var config = CONFIG.testConfigs[i];
        
        for (var j = 0; j < CONFIG.concurrencyLevels.length; j++) {
            var concurrencyLevel = CONFIG.concurrencyLevels[j];
            print("  Running " + config.description + " with concurrency " + concurrencyLevel);
            
            var throughputResult = runThroughputBenchmark(
                config.size, 
                concurrencyLevel, 
                config.enableOptimizer
            );
            
            throughputResult.description = config.description + " (Concurrency: " + concurrencyLevel + ")";
            results.throughputTests.push(throughputResult);
        }
    }
    
    calculateImprovementMetrics(results);
    
    saveResults(results);
    
    print("Benchmark tests completed.");
    return results;
}

function getEnvironmentInfo() {
    var conn = createConnection();
    var versionInfo = conn.execute("ADMIN GET VERSION")[0];
    var configInfo = conn.execute("ADMIN GET CONFIG")[0];
    conn.close();
    
    return {
        version: versionInfo.version,
        buildTime: versionInfo.buildTime,
        cpuCores: configInfo.cpuCores,
        memory: configInfo.memory
    };
}

function calculateImprovementMetrics(results) {
    results.improvements = {
        queryLatency: {},
        throughput: {},
        optimizationTime: {},
        memoryFootprint: {}
    };
    
    for (var i = 0; i < results.latencyTests.length; i += 2) {
        var baseline = results.latencyTests[i];
        var optimized = results.latencyTests[i + 1];
        
        if (!baseline || !optimized) continue;
        
        var sizeKey = baseline.size;
        results.improvements.queryLatency[sizeKey] = {};
        results.improvements.optimizationTime[sizeKey] = {};
        results.improvements.memoryFootprint[sizeKey] = {};
        
        for (var queryName in baseline.queries) {
            if (!optimized.queries[queryName]) continue;
            
            var baselineTime = baseline.queries[queryName].stats.avgExecutionTime;
            var optimizedTime = optimized.queries[queryName].stats.avgExecutionTime;
            
            var baselineMemory = baseline.queries[queryName].stats.avgMemoryUsage;
            var optimizedMemory = optimized.queries[queryName].stats.avgMemoryUsage;
            
            var latencyImprovement = ((baselineTime - optimizedTime) / baselineTime) * 100;
            var memoryIncrease = ((optimizedMemory - baselineMemory) / baselineMemory) * 100;
            
            results.improvements.queryLatency[sizeKey][queryName] = latencyImprovement;
            results.improvements.memoryFootprint[sizeKey][queryName] = memoryIncrease;
        }
    }
    
    for (var i = 0; i < results.throughputTests.length; i += 2) {
        var baseline = results.throughputTests[i];
        var optimized = results.throughputTests[i + 1];
        
        if (!baseline || !optimized) continue;
        
        var sizeKey = baseline.size + "_" + baseline.concurrencyLevel;
        results.improvements.throughput[sizeKey] = {};
        
        for (var queryName in baseline.queries) {
            if (!optimized.queries[queryName]) continue;
            
            var baselineThroughput = baseline.queries[queryName].queriesPerSecond;
            var optimizedThroughput = optimized.queries[queryName].queriesPerSecond;
            
            var throughputImprovement = ((optimizedThroughput - baselineThroughput) / baselineThroughput) * 100;
            
            results.improvements.throughput[sizeKey][queryName] = throughputImprovement;
        }
    }
    
    results.summary = {
        avgQueryLatencyImprovement: calculateAverageImprovement(results.improvements.queryLatency),
        avgThroughputImprovement: calculateAverageImprovement(results.improvements.throughput),
        avgMemoryFootprintIncrease: calculateAverageImprovement(results.improvements.memoryFootprint),
        targetsMet: {
            queryLatency: false,
            throughput: false,
            memoryFootprint: false
        }
    };
    
    results.summary.targetsMet.queryLatency = results.summary.avgQueryLatencyImprovement >= 30;
    results.summary.targetsMet.throughput = results.summary.avgThroughputImprovement >= 20;
    results.summary.targetsMet.memoryFootprint = results.summary.avgMemoryFootprintIncrease <= 10;
}

function calculateAverageImprovement(improvementObj) {
    var total = 0;
    var count = 0;
    
    for (var sizeKey in improvementObj) {
        for (var queryName in improvementObj[sizeKey]) {
            total += improvementObj[sizeKey][queryName];
            count++;
        }
    }
    
    return count > 0 ? total / count : 0;
}

function saveResults(results) {
    var resultsJson = JSON.stringify(results, null, 2);
    
    print("Results would be saved to: " + CONFIG.resultsFile);
    print(resultsJson);
}

runBenchmarks();
