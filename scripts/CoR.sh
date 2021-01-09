#! /bin/bash

if [ -z $GEM5_ROOT ]; then
    echo "ENV variable GEM5_ROOT is not set" >&2
    exit 2
fi

if [ -z $WORKLOADS_ROOT ]; then
    echo "ENV variable WORKLOADS_ROOT is not set" >&2
    exit 2
fi

unset BENCHMARK SIMPT SUFFIX

INSCOUNT=50000000 # default inst count is 50M
WARMUP=1000000    # default warmup is 1M

CLEAR_OUTPUT=true
DRYRUN=false
THREATMODEL=Futuristic
HW="Fence-All"
DET=Buffer
CHECK_THREAT=Execute
LIFT_ON_CLEAR=false
SB_HW=Bloom
ELEM_CNT=128

usage() {
    echo "Usage: CoR.sh [ -e | --elem-cnt COUNT ]
                [ -i | --max-insts INST ] [ -w | --warmup-insts INST ]
                [ --threat THREAT_MODEL ] [ --hw PROTECTION_MECHANISM ]
                [ --lift-on-clear ] [ --SB-struct SB_STRUCTURE ] [ --dry-run ]
                [ --no-clear ] [ -h | --help ]
                BENCHMARK SIMPT_ID STUDY_NAME CONFIG_NAME"
    echo ""
    echo "positional arguments:"
    echo "  BENCHMARK                name of the benchmark application"
    echo "  SIMPT_ID                 Simpoint checkpoint ID"
    echo "  STUDY_NAME               name of the study, used for output dir"
    echo "  CONFIG_NAME              name of the config, used for output dir"
    echo ""
    echo "optional arguments:"
    echo "  -e, --elem-cnt ELEMCNT   projected element count of bloom filter"
    echo "  --threat THREAT_MODEL    threat model used during simulation"
    echo "                           {Unsafe, Spectre, Futuristic (default)}"
    echo "  --hw HW                  protection mechanism"
    echo "                           {Unsafe, Fence (stall mem inst only), \
Fence-All (stall all types of inst, default)}"
    echo "  --lift-on-clear          lift fenced instructions when SB is cleared"
    echo "  --SB-struct STRUCTURE    SB hardware implementation"
    echo "                           {Ideal, Bloom (default)}"
    echo "  -i, --max-insts INST     maximum number of simulated instructions"
    echo "  -w, --warmup-insts INST  number of warmup instructions"
    echo "  --dry-run                dry run without running gem5"
    echo "  --no-clear               do not clear output directory"
    echo "  -h, --help               show this help message and exit"
}


PARSED_ARGUMENTS=$(getopt -a -n CoR -o e:hi:w: --long \
help,threat:,hw:,lift-on-clear,SB-struct:,max-insts:,warmup-insts:,\
dry-run,no-clear,elem-cnt: -- "$@")

if [ $? != 0 ]; then
    usage
    exit 1
fi

eval set -- "$PARSED_ARGUMENTS"
while :
do
    case "$1" in
        --threat)
            echo "Using threat model: $2" >&2
            THREATMODEL=$2; shift 2;
            ;;
        --hw)
            echo "Using HW: $2" >&2
            HW=$2; shift 2;
            ;;
        --lift-on-clear)
            echo "Lift all fences when the replay handle retires and clears SB" >&2
            LIFT_ON_CLEAR=true; shift;
            SUFFIX="$SUFFIX --liftOnClear"
            ;;
        --SB-struct)
            echo "Using SB structure: $2" >&2
            SB_HW=$2; shift 2;
            ;;
        -e | --elem-cnt)
            echo "Set projected element count to $2" >&2
            ELEM_CNT=$2; shift 2;
            ;;
        -i | --max-insts)
            echo "Set maximum instruction # to $2" >&2
            INSCOUNT=$2; shift 2;
            ;;
        -w | --warmup-insts)
            echo "Set warmup instruction # to $2" >&2
            WARMUP=$2; shift 2;
            ;;
        --)
            shift; break
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        --dry-run)
            echo "Dry-run mode" >&2
            DRYRUN=true; shift;
            ;;
        --no-clear)
            CLEAR_OUTPUT=false; shift;
            ;;
        *) 
            echo "Unexpected option: $1" >&2
            usage
            exit 2
            ;;
    esac
done

BENCHMARK=$1
SIMPT=$2
STUDY=$3
CONFIG=$4

if [ -z $BENCHMARK ] || [ -z $SIMPT ] || [ -z $STUDY ] || [ -z $CONFIG ] || [ ! -z $5 ]; then
    echo "Error: need to provide BENCHMARK, SIMPT_ID, and STUDY_NAME" >&2
    usage
    exit 2
fi

OUTPUT_DIR=$GEM5_ROOT/output/$STUDY/$CONFIG/$BENCHMARK/$SIMPT
RUN_DIR=$WORKLOADS_ROOT/run/$BENCHMARK
SIMPT_DIR=$WORKLOADS_ROOT/ckpt/$BENCHMARK

SCRIPT_STDOUT=$OUTPUT_DIR/simulation.out
SCRIPT_STDERR=$OUTPUT_DIR/simulation.err

BENCH_STDOUT=$OUTPUT_DIR/bench.out
BENCH_STDERR=$OUTPUT_DIR/bench.err

EXEC=$GEM5_ROOT/build/X86_MESI_Two_Level/gem5.fast

if [ -d "$OUTPUT_DIR" ] && [ $CLEAR_OUTPUT = true ]; then
    echo "Cleaning $OUTPUT_DIR" >&2
    rm -rf $OUTPUT_DIR/*
else
    mkdir -p $OUTPUT_DIR
fi

if [ $DRYRUN = true ]; then
    echo $OUTPUT_DIR
    exit 0
fi

echo "*******************************************"
echo "Running $BENCHMARK checkpoint #$SIMPT for $INSCOUNT instructions"
echo "Threat model: $THREATMODEL"
echo "Hardware: $HW"
echo "Detection: $DET"
echo "Check Threat: $CHECK_THREAT"
echo "Projected Element Count: $ELEM_CNT"
echo "SB Hardware: $SB_HW"
echo "Checkpoint direcotory: $SIMPT_DIR"
echo "Output directory: $OUTPUT_DIR"
echo "*******************************************"

git show HEAD | head -n 1 >  $OUTPUT_DIR/version

cd $RUN_DIR
echo Simulation started at $(date)
echo ""

if [ $(hostname) != "iacoma-perf" ]; then
    module () {
        eval $($LMOD_CMD bash "$@") && eval $(${LMOD_SETTARG_CMD:-:} -s sh)
    }
    export LD_LIBRARY_PATH=$HOME/bin
    module load gcc/7.2.0
fi

$EXEC --outdir=$OUTPUT_DIR \
$GEM5_ROOT/configs/example/se.py --benchmark=$BENCHMARK \
--bench-stdout=$BENCH_STDOUT --bench-stderr=$BENCH_STDERR \
--checkpoint-dir=$SIMPT_DIR --simpt-ckpt=$SIMPT \
--checkpoint-restore=1 --at-instruction \
--num-cpus=1 --mem-size=4GB --cpu-type=DerivO3CPU \
--l1d_assoc=8 --l1i_assoc=4 \
--l2_assoc=16 --num-l2caches=1 --needsTSO --ruby \
--network=simple --topology=Mesh_XY --mesh-rows=1 \
--threatModel=$THREATMODEL --HWName=$HW \
--replayDetScheme=$DET --replayDetThreat=$CHECK_THREAT \
--sbHWStruct=$SB_HW \
--maxinsts=$INSCOUNT --warmup-insts=$WARMUP \
$SUFFIX 2>&1 >$SCRIPT_STDOUT | tee $SCRIPT_STDERR

EXIT_CODE=${PIPESTATUS[0]}

echo ""
echo Simulation ended at $(date) with $EXIT_CODE

exit $EXIT_CODE
