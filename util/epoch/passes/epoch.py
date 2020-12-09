import sys

from sir import Function


class EpochPass:
    def __init__(self, function: Function, file=sys.stdout):
        self.function = function
        self._file = file
        self.epochs = set()

    def run(self):
        loops = self.function.natural_loops
        self.epochs = set()
        for loop in loops:
            self.epochs.add((loop.backedge[0].ctrl_insn.pc, 'I'))
            for ex in loop.exits:
                self.epochs.add((ex[1].address, 'L'))
            for ex in loop.entries:
                self.epochs.add((ex[0].ctrl_insn.pc, 'L'))

        for e in self.epochs:
            print(f'{e[0]:#x} {e[1]}', file=self._file)
