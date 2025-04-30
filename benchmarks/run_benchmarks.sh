

ALL_SCENARIOS=false
OUTPUT_DIR="results/latest"
DATASET_SIZE="medium"
CONCURRENCY_LEVELS="1,5,10,20,50"
ITERATIONS=5

while [[ $# -gt 0 ]]; do
  case $1 in
    --all-scenarios)
      ALL_SCENARIOS=true
      shift
      ;;
    --output-dir=*)
      OUTPUT_DIR="${1#*=}"
      shift
      ;;
    --dataset=*)
      DATASET_SIZE="${1#*=}"
      shift
      ;;
    --concurrency=*)
      CONCURRENCY_LEVELS="${1#*=}"
      shift
      ;;
    --iterations=*)
      ITERATIONS="${1#*=}"
      shift
      ;;
    --help)
      echo "Usage: ./run_benchmarks.sh [options]"
      echo "Options:"
      echo "  --all-scenarios       Run all benchmark scenarios"
      echo "  --output-dir=DIR      Output directory for results (default: results/latest)"
      echo "  --dataset=SIZE        Dataset size to use (small, medium, large, default: medium)"
      echo "  --concurrency=LEVELS  Comma-separated list of concurrency levels (default: 1,5,10,20,50)"
      echo "  --iterations=N        Number of iterations for each test (default: 5)"
      echo "  --help                Display this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

mkdir -p "benchmarks/${OUTPUT_DIR}"
mkdir -p "benchmarks/${OUTPUT_DIR}/raw_data"
mkdir -p "benchmarks/${OUTPUT_DIR}/charts"

echo "=== SequoiaDB Query Optimization Benchmark ==="
echo "Date: $(date)"
echo "Dataset: ${DATASET_SIZE}"
echo "Concurrency Levels: ${CONCURRENCY_LEVELS}"
echo "Iterations: ${ITERATIONS}"
echo "Output Directory: ${OUTPUT_DIR}"
echo "================================================"

run_benchmark() {
  local category=$1
  local optimizer=$2
  local dataset=$3
  local concurrency=$4
  
  echo "Running benchmark: ${category} - ${optimizer} - ${dataset} - Concurrency ${concurrency}"
  
  node tests/benchmark_runner.js \
    --category=${category} \
    --optimizer=${optimizer} \
    --dataset=${dataset} \
    --concurrency=${concurrency} \
    --iterations=${ITERATIONS} \
    --output-dir=${OUTPUT_DIR}
  
  echo "Benchmark completed: ${category} - ${optimizer} - ${dataset} - Concurrency ${concurrency}"
}

if [ "$ALL_SCENARIOS" = true ]; then
  CATEGORIES=("simple_joins" "three_way_joins" "four_way_joins" "subquery_joins" "complex_filtering")
  
  OPTIMIZERS=("baseline" "optimized")
  
  if [ "$DATASET_SIZE" = "all" ]; then
    DATASETS=("small" "medium" "large")
  else
    DATASETS=("$DATASET_SIZE")
  fi
  
  IFS=',' read -ra CONCURRENCY_ARRAY <<< "$CONCURRENCY_LEVELS"
  
  for category in "${CATEGORIES[@]}"; do
    for optimizer in "${OPTIMIZERS[@]}"; do
      for dataset in "${DATASETS[@]}"; do
        for concurrency in "${CONCURRENCY_ARRAY[@]}"; do
          run_benchmark "$category" "$optimizer" "$dataset" "$concurrency"
        done
      done
    done
  done
  
  echo "Generating summary report..."
  node tests/generate_report.js --output-dir=${OUTPUT_DIR}
  
  echo "All benchmark scenarios completed."
else
  echo "No specific benchmark scenario specified. Use --all-scenarios to run all scenarios."
  exit 1
fi

echo "Benchmark execution completed. Results are available in benchmarks/${OUTPUT_DIR}"
