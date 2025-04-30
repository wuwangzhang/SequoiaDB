/**
 * generate_report.js
 * 
 * Generates a comprehensive benchmark report from raw data
 */

const fs = require('fs');
const path = require('path');
const config = require('./config');

const args = process.argv.slice(2);
const outputDirArg = args.find(arg => arg.startsWith('--output-dir='));
const outputDir = outputDirArg ? outputDirArg.split('=')[1] : 'results/latest';

console.log(`Generating report for benchmark results in ${outputDir}`);

const rawDataDir = path.join(__dirname, '..', outputDir, 'raw_data');
const chartsDir = path.join(__dirname, '..', outputDir, 'charts');
const reportPath = path.join(__dirname, '..', outputDir, 'optimizer_improvements.md');

function readRawData() {
    const files = fs.readdirSync(rawDataDir);
    const rawData = {};
    
    files.forEach(file => {
        if (file.endsWith('.json')) {
            const content = fs.readFileSync(path.join(rawDataDir, file), 'utf8');
            const data = JSON.parse(content);
            const parts = file.replace('.json', '').split('_');
            
            const category = parts[0];
            const optimizer = parts[1];
            const dataset = parts[2];
            const concurrency = parts[3] ? parseInt(parts[3]) : 1;
            
            if (!rawData[category]) rawData[category] = {};
            if (!rawData[category][optimizer]) rawData[category][optimizer] = {};
            if (!rawData[category][optimizer][dataset]) rawData[category][optimizer][dataset] = {};
            
            rawData[category][optimizer][dataset][concurrency] = data;
        }
    });
    
    return rawData;
}

function calculateImprovements(rawData) {
    const improvements = {
        queryLatency: {},
        throughput: {},
        optimizationTime: {},
        memoryFootprint: {}
    };
    
    Object.keys(rawData).forEach(category => {
        if (!rawData[category].baseline || !rawData[category].optimized) return;
        
        improvements.queryLatency[category] = {};
        improvements.throughput[category] = {};
        improvements.optimizationTime[category] = {};
        improvements.memoryFootprint[category] = {};
        
        Object.keys(rawData[category].baseline).forEach(dataset => {
            if (!rawData[category].optimized[dataset]) return;
            
            improvements.queryLatency[category][dataset] = {};
            improvements.throughput[category][dataset] = {};
            improvements.optimizationTime[category][dataset] = {};
            improvements.memoryFootprint[category][dataset] = {};
            
            Object.keys(rawData[category].baseline[dataset]).forEach(concurrency => {
                if (!rawData[category].optimized[dataset][concurrency]) return;
                
                const baseline = rawData[category].baseline[dataset][concurrency];
                const optimized = rawData[category].optimized[dataset][concurrency];
                
                const baselineLatency = baseline.averageQueryTime;
                const optimizedLatency = optimized.averageQueryTime;
                const latencyImprovement = ((baselineLatency - optimizedLatency) / baselineLatency) * 100;
                improvements.queryLatency[category][dataset][concurrency] = latencyImprovement;
                
                const baselineThroughput = baseline.queriesPerSecond;
                const optimizedThroughput = optimized.queriesPerSecond;
                const throughputImprovement = ((optimizedThroughput - baselineThroughput) / baselineThroughput) * 100;
                improvements.throughput[category][dataset][concurrency] = throughputImprovement;
                
                const baselineOptTime = baseline.averageOptimizationTime;
                const optimizedOptTime = optimized.averageOptimizationTime;
                const optTimeIncrease = ((optimizedOptTime - baselineOptTime) / baselineOptTime) * 100;
                improvements.optimizationTime[category][dataset][concurrency] = optTimeIncrease;
                
                const baselineMemory = baseline.averageMemoryUsage;
                const optimizedMemory = optimized.averageMemoryUsage;
                const memoryIncrease = ((optimizedMemory - baselineMemory) / baselineMemory) * 100;
                improvements.memoryFootprint[category][dataset][concurrency] = memoryIncrease;
            });
        });
    });
    
    return improvements;
}

function calculateAverageImprovements(improvements) {
    const averages = {
        queryLatency: 0,
        throughput: 0,
        optimizationTime: 0,
        memoryFootprint: 0
    };
    
    let counts = {
        queryLatency: 0,
        throughput: 0,
        optimizationTime: 0,
        memoryFootprint: 0
    };
    
    Object.keys(improvements.queryLatency).forEach(category => {
        Object.keys(improvements.queryLatency[category]).forEach(dataset => {
            Object.keys(improvements.queryLatency[category][dataset]).forEach(concurrency => {
                averages.queryLatency += improvements.queryLatency[category][dataset][concurrency];
                counts.queryLatency++;
            });
        });
    });
    
    Object.keys(improvements.throughput).forEach(category => {
        Object.keys(improvements.throughput[category]).forEach(dataset => {
            Object.keys(improvements.throughput[category][dataset]).forEach(concurrency => {
                averages.throughput += improvements.throughput[category][dataset][concurrency];
                counts.throughput++;
            });
        });
    });
    
    Object.keys(improvements.optimizationTime).forEach(category => {
        Object.keys(improvements.optimizationTime[category]).forEach(dataset => {
            Object.keys(improvements.optimizationTime[category][dataset]).forEach(concurrency => {
                averages.optimizationTime += improvements.optimizationTime[category][dataset][concurrency];
                counts.optimizationTime++;
            });
        });
    });
    
    Object.keys(improvements.memoryFootprint).forEach(category => {
        Object.keys(improvements.memoryFootprint[category]).forEach(dataset => {
            Object.keys(improvements.memoryFootprint[category][dataset]).forEach(concurrency => {
                averages.memoryFootprint += improvements.memoryFootprint[category][dataset][concurrency];
                counts.memoryFootprint++;
            });
        });
    });
    
    if (counts.queryLatency > 0) averages.queryLatency /= counts.queryLatency;
    if (counts.throughput > 0) averages.throughput /= counts.throughput;
    if (counts.optimizationTime > 0) averages.optimizationTime /= counts.optimizationTime;
    if (counts.memoryFootprint > 0) averages.memoryFootprint /= counts.memoryFootprint;
    
    return averages;
}

function generateReport(rawData, improvements, averages) {
    const templatePath = path.join(__dirname, '..', 'results', 'optimizer_improvements.md');
    let template = fs.readFileSync(templatePath, 'utf8');
    
    template = template.replace(/TBD/g, 'N/A');
    
    template = template.replace(/Query Latency Reduction \(Complex Joins\)\s*\|\s*30%\+\s*\|\s*N\/A\s*\|\s*N\/A/, 
                              `Query Latency Reduction (Complex Joins) | 30%+ | ${averages.queryLatency.toFixed(2)}% | ${averages.queryLatency >= 30 ? '✅' : '❌'}`);
    
    template = template.replace(/Throughput Improvement \(Concurrent Workloads\)\s*\|\s*20%\+\s*\|\s*N\/A\s*\|\s*N\/A/, 
                              `Throughput Improvement (Concurrent Workloads) | 20%+ | ${averages.throughput.toFixed(2)}% | ${averages.throughput >= 20 ? '✅' : '❌'}`);
    
    template = template.replace(/Optimization Time Overhead\s*\|\s*<5% increase\s*\|\s*N\/A\s*\|\s*N\/A/, 
                              `Optimization Time Overhead | <5% increase | ${averages.optimizationTime.toFixed(2)}% | ${averages.optimizationTime <= 5 ? '✅' : '❌'}`);
    
    template = template.replace(/Memory Footprint\s*\|\s*<10% increase\s*\|\s*N\/A\s*\|\s*N\/A/, 
                              `Memory Footprint | <10% increase | ${averages.memoryFootprint.toFixed(2)}% | ${averages.memoryFootprint <= 10 ? '✅' : '❌'}`);
    
    template = template.replace(/Query latency reduction for complex joins: \[TBD\]% \(target: 30%\+\)/, 
                              `Query latency reduction for complex joins: ${averages.queryLatency.toFixed(2)}% (target: 30%+)`);
    
    template = template.replace(/Throughput improvement for concurrent workloads: \[TBD\]% \(target: 20%\+\)/, 
                              `Throughput improvement for concurrent workloads: ${averages.throughput.toFixed(2)}% (target: 20%+)`);
    
    template = template.replace(/Optimization time overhead: \[TBD\]% \(target: <5%\)/, 
                              `Optimization time overhead: ${averages.optimizationTime.toFixed(2)}% (target: <5%)`);
    
    template = template.replace(/Memory footprint increase: \[TBD\]% \(target: <10%\)/, 
                              `Memory footprint increase: ${averages.memoryFootprint.toFixed(2)}% (target: <10%)`);
    
    fs.writeFileSync(reportPath, template);
    
    console.log(`Report generated at ${reportPath}`);
}

function main() {
    try {
        const rawData = readRawData();
        
        const improvements = calculateImprovements(rawData);
        
        const averages = calculateAverageImprovements(improvements);
        
        generateReport(rawData, improvements, averages);
        
        console.log('Report generation completed successfully.');
    } catch (err) {
        console.error('Error generating report:', err);
        process.exit(1);
    }
}

main();
