# Jamais Vu Program Analysis Pass
`epoch` is a program analysis tool that generates epoch information for a given
binary. `epoch` is implemented on [radare2](https://github.com/radareorg/radare2).
For more help information, please refer to `util/epoch/epoch -h`.

## Output
Instead of re-writing the binary and re-generating the entire set of checkpoints,
we save the epoch information into a stand-alone file that is loaded at runtime.
Each line of the output file represents an epoch separator record. Each record
has two fields, PC and epoch size of the separator, which are separated by space(s).
The PC is represented in hexadecimal. The epoch size is represented in a single
character: `I` stands for *Iteration*, `L` stands for *Loop*.
Since routine epoch separators are detected by the hardware at runtime,
the analysis pass does not mark them.

For example:
```0x400800 L```
represents a loop epoch separator at `0x400800`.

## Known Issue
The latest radare2 (>= 4.4.0) may have memory leak problems on some of statically-linked
SPEC2017 binaries. We use 4.3.0 by default.
