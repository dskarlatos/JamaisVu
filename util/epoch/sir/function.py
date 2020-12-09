import logging
import networkx as nx

from .basicblock import BasicBlock
from .exceptions import InlinedFunctionError, CorruptedCFG, UnresolvedIndirectJump
from .exceptions import UnhandledOutputError, WildJumpTargetError, MultipleCFGEntriesError
from r2pipe import open_async
from typing import Iterator


class Function:
    __slots__ = ('address', 'name', 'end', 'cfg', 'pdg', 'BBs', 'bb_table', '__dict__')

    def __init__(self, r2: open_async.open, r2obj: dict):
        self.address = r2obj['offset']
        self.name = r2obj['name']
        self.end = r2obj['maxbound']

        try:
            r2.cmd(f's {self.name}')
            graph = r2.cmdj('agj')
            assert(len(graph) == 1)
            blocks = graph[0]['blocks']
            if graph[0]['name'] != self.name:
                raise InlinedFunctionError
        except (AssertionError, KeyError):
            err_msg = f'Unexpected radare2 output at function {self.name} ({self.address:#x})'
            logging.error(err_msg)
            raise UnhandledOutputError(err_msg)
        else:
            self.BBs = [BasicBlock(self, b) for b in blocks]
            self.bb_table = {bb.address: bb for bb in self}
            self._resolve_wild_jumps(r2)
            self._build_cfg()
            # check entry bb
            entries = [bb for bb, i in tuple(self.cfg.in_degree) if i == 0]
            if len(entries) != 1 or entries[0] != self.entry_bb:
                raise CorruptedCFG(f'Malformed CFG: {self.name} ({self.address:#x})')

            # check unresolved indirect jump
            for bb in self:
                if bb.ctrl_insn.is_indirect_jmp:
                    if len(list(bb.successors)) == 0:
                        raise UnresolvedIndirectJump

    def _resolve_wild_jumps(self, r2: open_async.open):
        # try to resolve wild jumps (i.e., jumps that do not land inside the function)
        has_wild_jump = True
        while has_wild_jump:
            wild_jumps = []
            for bb in self:
                for succ in bb._successor_addresses:
                    if succ not in self.bb_table:
                        wild_jumps.append(succ)
            for jump in wild_jumps:
                try:
                    r2.cmd(f's {jump:#x}')
                    graph = r2.cmdj('agj')
                    assert(len(graph) == 1)
                    blocks = graph[0]['blocks']
                except (AssertionError, KeyError):
                    logging.error(f'Unexpected radare2 output at function {self.name} ({self.address:#x})')
                    raise UnhandledOutputError(f'Unexpected radare2 output: {self.name} ({self.address:#x})')
                else:
                    for bb in blocks:
                        if bb['offset'] not in self.bb_table:
                            b = BasicBlock(self, bb)
                            self.BBs.append(b)
                            self.bb_table[b.address] = b
            has_wild_jump = len(wild_jumps)
            for jump in wild_jumps:
                if jump not in self.bb_table: # this jump cannot be resolved
                    has_wild_jump = False
        return has_wild_jump

    def _build_cfg(self):
        self.cfg = nx.DiGraph()
        self.cfg.add_nodes_from(self.BBs)
        for bb in self:
            for succ in bb._successor_addresses:
                try:
                    target = self.bb_table[succ]
                except KeyError:
                    logging.error(f'{bb} tries to jump to {succ:#x}, which isn\'t in {self}')
                    raise WildJumpTargetError(f'{bb} tries to jump to {succ:#x}, which isn\'t in {self}')
                else:
                    self.cfg.add_edge(bb, target, weight=len(bb))

        # check entries
        entries = [bb for bb in self.cfg.nodes if len(list(self.cfg.predecessors(bb))) == 0]
        if len(entries) != 1:
            raise MultipleCFGEntriesError(self.name)

    @property
    def entry_bb(self):
        return self.bb_table[self.address]

    @property
    def dom_tree(self) -> nx.DiGraph:
        return self._dom_tree

    @property
    def p_dom_tree(self) -> nx.DiGraph:
        return self._p_dom_tree

    @property
    def bb_pdom_trees(self) -> dict:
        return self._bb_pdom_tree

    @property
    def natural_loops(self):
        return self._nloops

    def __iter__(self) -> Iterator[BasicBlock]:
        return iter(self.BBs)

    def __getitem__(self, item) -> BasicBlock:
        return self.BBs[item]

    def __len__(self) -> int:
        return len(self.BBs)

    def __str__(self):
        return f'{self.name} ({self.address:#x})'

    def __repr__(self):
        return f'Function({self.name}, ({self.address:#x})'

    def __eq__(self, other: 'Function'):
        return hash(self) == hash(other)

    def __hash__(self):
        return hash((self.name, self.address))
