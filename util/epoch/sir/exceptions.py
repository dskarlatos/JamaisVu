class ASAError(Exception): pass

class Radare2Error(Exception): pass

class UnhandledOutputError(Radare2Error): pass

class WildJumpTargetError(Radare2Error): pass

class MalformedOutputError(Radare2Error): pass

class InlinedFunctionError(ASAError): pass

class MultipleCFGEntriesError(Radare2Error): pass

class CapstoneDecodeError(Radare2Error): pass

class UnresolvedIndirectJump(Radare2Error): pass

class CorruptedCFG(Radare2Error): pass
