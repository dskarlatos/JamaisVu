# Jamais Vu Program Analysis Pass
`epoch` is a program analysis tool that generates epoch information for a given
binary. `epoch` is based on [radare2](https://github.com/radareorg/radare2).
For more help information, please refer to `./epoch -h`.

## Arguments
`epoch` takes as input the path to the binary under analysis. It also
takes an optional argument for the output filename. If the output name
is omitted, analysis results will be printed to STDOUT.

## Output
Instead of re-writing the binary, which needs to re-generate the entire
set of checkpoints for new binaries,
we save the epoch information into a stand-alone file that is loaded at runtime.
Each line of the output file represents an epoch separator record. Each record
has two fields, PC and epoch size of the separator, which are separated by space(s).
The PC is represented in hexadecimal. The epoch size is represented in a single
character: `I` stands for `Iteration`, `L` stands for `Loop`.
Since routine epoch separators are detected by Jamais Vu hardware at runtime,
the analysis pass does not mark them.

For example:
```0x400800 L```
represents a loop epoch separator at `0x400800`.

## Known Issue(s)
The latest radare2 (>= 4.4.0) may have memory leak problems on some statically-linked
SPEC2017 binaries. We use 4.3.0 by default.

## Performance
The analysis tool is implemented in Python and it is single-threaded. It might
take one or two hours to run on a large statically-linked binary.
Using [PyPy](https://www.pypy.org/) as your Python interpreter can reduce the analysis time.
Our Docker image uses PyPy 3.7 by default.
