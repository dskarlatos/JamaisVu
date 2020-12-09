import logging

from .exceptions import UnhandledOutputError, CapstoneDecodeError
from typing import List
from typing import Iterator
from collections import namedtuple
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CsInsn


Instruction = namedtuple('Instruction', 'pc is_indirect_jmp')


class BasicBlock:
    __slots__ = ('address', 'parent', 'jump', 'fail', 'cases', 'insns')

    def __init__(self, func, r2obj: dict):
        self.parent = func
        try:
            self.address = r2obj['offset']
            self.jump = r2obj.get('jump', 0)
            self.fail = r2obj.get('fail', 0)
            cases = r2obj.get('switchop', dict()).get('cases', dict())
            self.cases = {c['jump'] for c in cases}

            self.insns = []
            for op in r2obj['ops']:
                md = Cs(CS_ARCH_X86, CS_MODE_64)
                md.detail = True
                _addr = op['offset']
                _insns = list(md.disasm(BasicBlock.to_bytes(op['bytes']), _addr))
                if len(_insns) != 1:
                    raise CapstoneDecodeError(f'Decoder error at {_addr:#x}')
                else:
                    _insn: CsInsn = _insns[0]
                    _reads, _ = _insn.regs_access()
                    indirect = _insn.mnemonic == 'jmp' and len(_reads) > 0
                    self.insns.append(Instruction(_addr, indirect))
        except KeyError:
            err_msg = f'Unexpected radare2 output at Basic Block {self.address:#x}'
            logging.error(err_msg)
            raise UnhandledOutputError(err_msg)
    
    @staticmethod
    def to_bytes(bytes_) -> bytes:
        assert(len(bytes_) % 2 == 0)
        return b''.join((int(bytes_[i:i+2], base=16).to_bytes(1, 'little')
                             for i in range(0, len(bytes_), 2)))

    @property
    def is_entry(self) -> bool:
        return self.address == self.function.address

    @property
    def is_branch(self) -> bool:
        return len(list(self.successors)) > 1

    @property
    def _successor_addresses(self) -> List[int]:
        res = self.cases
        if self.jump:
            res.add(self.jump)
        if self.fail:
            res.add(self.fail)
        return res

    @property
    def ctrl_insn(self):
        return self.insns[-1]

    @property
    def predcessors(self):
        return self.function.cfg.predecessors(self)

    @property
    def successors(self):
        return self.function.cfg.successors(self)

    @property
    def function(self):
        self.parent
        return self.parent

    @property
    def bb_table(self):
        return self.function.bb_table

    def dominate(self, other: 'BasicBlock'):
        return other in self.function._dominances[self]

    @property
    def dominance(self):
        return self.function._dominances[self]

    @property
    def p_dominance(self):
        return self.function._p_dominances[self]

    @property
    def dom_frontier(self):
        return self.function._dom_frontiers[self]

    @property
    def p_dom_frontier(self):
        return self.function._p_dom_frontiers[self]

    def dominates(self, other: 'BasicBlock', post=False, properly=False):
        '''
        returns whether "self" dominates "other"
        '''
        if properly and other == self:
            return False
        else:
            if post:
                return other in self.p_dominance
            else:
                return other in self.dominance

    def __iter__(self) -> Iterator[Instruction]:
        return iter(self.insns)

    def __getitem__(self, item) -> Instruction:
        return self.insns[item]

    def __len__(self):
        return len(self.insns)

    def __str__(self):
        return f'BB<{self.address:#x}, {len(self)}, {self.function.name}>'

    def __repr__(self):
        return f'BasicBlock({self.address:#x}, {len(self)}, {self.function.name})'

    def __eq__(self, other: 'BasicBlock'):
        return hash(self) == hash(other)

    def __hash__(self):
        return hash((self.address, self.function, len(self)))
