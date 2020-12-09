from sir import Function, BasicBlock
from typing import Tuple, Set


class NaturalLoop:
    __slots__ = ('parent', 'function', 'bbs', 'header', 'backedge', 'exits', 'entries')

    def __init__(self, func: Function, backedge: Tuple[BasicBlock, BasicBlock]):
        self.parent = func
        self.backedge = backedge
        self.bbs: Set[BasicBlock] = {backedge[0], backedge[1]}
        self.header = backedge[1]

        visited = set()
        worklist = [backedge[0]]
        while worklist:
            bb = worklist.pop(0)
            if bb not in visited:
                visited.add(bb)
                for pred in bb.predcessors:
                    if pred != self.header:
                        self.bbs.add(pred)
                        worklist.append(pred)
            else:
                continue

        self.exits = set()
        for bb in self.bbs:
            for succ in bb.successors:
                if succ not in self.bbs:
                    self.exits.add((bb, succ))

        self.entries = {(pred, self.header) for pred in self.header.predcessors
                        if pred not in self.bbs}

    def __hash__(self):
        return hash(tuple(self.backedge))


class LoopsPass:
    def __init__(self, function: Function):
        self.function = function

    def run(self):
        backedges = set()
        for bb in self.function:
            for succ in bb.successors:
                if succ.dominate(bb):
                    backedges.add((bb, succ))

        loops = set()
        for be in backedges:
            loops.add(NaturalLoop(self.function, be))
        self.function._nloops = sorted(loops, key=lambda x: hash(x))
