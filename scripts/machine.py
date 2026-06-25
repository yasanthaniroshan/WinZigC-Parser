import sys,enum

class Op(enum.Enum):
    SAVE = "save"
    LOAD = "load"
    NEGATE = "negate"
    NOT = "not"
    ADD = "add"
    SUBTRACT = "subtract"
    EQUAL = "equal"
    READ = "read"
    PRINT = "print"
    LIT = "lit"
    GOTO = "goto"
    IFFALSE = "iffalse"
    IFTRUE = "iftrue"
    STOP = "stop"
    # Added
    MULTIPLY = "multiply"
    DIVIDE = "divide"
    MOD = "mod"
    AND = "and"
    OR = "or"
    LESSTHAN = "lessthan"
    LESSEQUAL = "lessequal"
    GREATER = "greater"
    GREATEREQUAL = "greaterequal"
    NOTEQUAL = "notequal"
    CALL = "call"
    RETURN = "return"
    LITS = "lits"
    PRINTS = "prints"
    # Frame-relative addressing (function locals)
    ENTER = "enter"           # fp := top - argc + 1  (incoming args become the first frame slots)
    RESERVE = "reserve"       # top += nvars          (reserve space for non-param locals)
    SAVE_LOCAL = "save_local" # stack[fp + i] := pop()
    LOAD_LOCAL = "load_local" # push(stack[fp + i])

    

class StackMachine:
    def __init__(self, code, stack_size=1000):
        self.code = code
        self.stack = [0] * stack_size
        self.call_stack = [] # for tracking function calls

        # Variables live at the bottom of the stack at the absolute addresses
        # assigned by the code generator (referenced by `save n` / `load n`).
        # The operand stack must start ABOVE this region, otherwise pushed
        # operands clobber variables (and vice-versa). Reserve one slot per
        # variable address by starting `top` at the highest address used.
        max_addr = -1
        for instr in code:
            if instr[0] in ("save", "load") and instr[1] is not None:
                try:
                    max_addr = max(max_addr, int(instr[1]))
                except ValueError:
                    pass
        self.top = max_addr # slots 0..max_addr are reserved for GLOBAL variables
        # Frame pointer: base of the current function's activation frame. Globals
        # (save/load) stay absolute; function params/locals (save_local/load_local)
        # are addressed as stack[fp + index]. `fp` is set on every `enter`, saved
        # on `call`, and restored on `return`. The first free slot above the global
        # region is the initial base for the main program's nested calls.
        self.fp = max_addr + 1
        self.pc = 0
        self.input_ptr = 0

    def push(self, value):
        self.top += 1
        self.stack[self.top] = value

    def pop(self):
        value = self.stack[self.top]
        self.top -= 1
        return value

    def read_integer(self):
        data = input()
        return int(data)

    def print_integer(self):
        print(self.pop())
    
    def print_string(self):
        print(self.pop())

    def run(self):
        while True:
            try:
                instr = self.code[self.pc]

                op = instr[0]
                op = Op(op) if isinstance(op, str) else op
             
                if op == Op.SAVE:
                    n = instr[1]
                    self.stack[int(n)] = self.pop()

                elif op == Op.LOAD:
                    n = instr[1]
                    self.push(self.stack[int(n)])

                elif op == Op.NEGATE:
                    self.stack[self.top] = -int(self.stack[self.top])

                elif op == Op.NOT:
                    self.stack[self.top] = 0 if int(self.stack[self.top]) else 1

                elif op == Op.ADD:
                    t = int(self.pop())
                    self.stack[self.top] = int(self.stack[self.top]) + t

                elif op == Op.SUBTRACT:
                    t = int(self.pop())
                    self.stack[self.top] = int(self.stack[self.top]) - t

                elif op == Op.EQUAL:
                    t = int(self.pop())
                    self.stack[self.top] = 1 if int(self.stack[self.top]) == t else 0

                elif op == Op.READ:
                    self.push(self.read_integer())

                elif op == Op.PRINT:
                    self.print_integer()

                elif op == Op.LIT:
                    n = instr[1]
                    self.push(n)

                elif op == Op.GOTO:
                    n = instr[1]
                    self.pc = int(n) - 1
                    continue

                elif op == Op.IFFALSE:
                    n = instr[1]
                    if self.pop() == 0:
                        self.pc = int(n) - 1
                        continue

                elif op == Op.IFTRUE:
                    n = instr[1]
                    if self.pop() == 1:
                        self.pc = int(n) - 1
                        continue

                elif op == Op.STOP:
                    break

                elif op == Op.MULTIPLY:
                    t = int(self.pop())
                    self.stack[self.top] = int(self.stack[self.top]) * t
                
                elif op == Op.DIVIDE:
                    t = int(self.pop())
                    if t == 0:
                        raise ZeroDivisionError
                    self.stack[self.top] = int(self.stack[self.top]) // t
                
                elif op == Op.MOD:
                    t = int(self.pop())
                    self.stack[self.top] = int(self.stack[self.top]) % t
                
                elif op == Op.AND:
                    t = int(self.pop())
                    self.stack[self.top] = int(self.stack[self.top]) and t
                
                elif op == Op.OR:
                    t = int(self.pop())
                    self.stack[self.top] = int(self.stack[self.top]) or t
                
                elif op == Op.LESSTHAN:
                    t = int(self.pop())
                    self.stack[self.top] = 1 if int(self.stack[self.top]) < t else 0

                elif op == Op.LESSEQUAL:
                    t = int(self.pop())
                    self.stack[self.top] = 1 if int(self.stack[self.top]) <= t else 0

                elif op == Op.GREATER:
                    t = int(self.pop())
                    self.stack[self.top] = 1 if int(self.stack[self.top]) > t else 0

                elif op == Op.GREATEREQUAL:
                    t = int(self.pop())
                    self.stack[self.top] = 1 if int(self.stack[self.top]) >= t else 0

                elif op == Op.NOTEQUAL:
                    t = int(self.pop())
                    self.stack[self.top] = 1 if int(self.stack[self.top]) != t else 0
                
                elif op == Op.CALL:
                    n = instr[1] # second element is the instruction address of funciton
                    # Save the return address AND the caller's frame pointer so the
                    # callee's `enter`/`return` can rebase without losing the caller.
                    self.call_stack.append((self.pc + 1, self.fp))
                    self.pc = int(n) - 1
                    continue # don't increment PC

                elif op == Op.ENTER:
                    # Incoming arguments (pushed by the caller) occupy the top `argc`
                    # operand slots. They become the first slots of this frame.
                    argc = int(instr[1])
                    self.fp = self.top - argc + 1

                elif op == Op.RESERVE:
                    # Reserve slots for the function's non-parameter locals, lifting
                    # the operand stack above the whole frame.
                    self.top += int(instr[1])

                elif op == Op.SAVE_LOCAL:
                    i = int(instr[1])
                    self.stack[self.fp + i] = self.pop()

                elif op == Op.LOAD_LOCAL:
                    i = int(instr[1])
                    self.push(self.stack[self.fp + i])

                elif op == Op.RETURN:
                    if not self.call_stack:
                        raise RuntimeError("Return called with empty call stack")
                    retval = self.pop()                  # function result sits on top
                    ret_addr, old_fp = self.call_stack.pop()
                    self.top = self.fp - 1               # discard the whole frame (incl. args)
                    self.fp = old_fp                     # restore the caller's frame
                    self.push(retval)                    # leave result on caller's stack
                    self.pc = ret_addr
                    continue # do not increment

                elif op == Op.LITS:
                    s = instr[1]
                    self.push(s)

                elif op == Op.PRINTS:
                    self.print_string()

                self.pc += 1
            except IndexError:
                raise RuntimeError("Program counter out of bounds")
            
            except ValueError:
                raise RuntimeError("Invalid instruction: {}".format(instr))
            
            except ZeroDivisionError:
                raise ZeroDivisionError("Zero division")



if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 machine.py [program.asm]")
        sys.exit(1)
    asm_file = sys.argv[1]
    program = []
    with open(asm_file) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            op = parts[0]
            # args = tuple(x for x in parts[1:]) if len(parts) > 1 else ()
            args = " ".join(parts[1:]) if len(parts) > 1 else None
            program.append((op, args))
    machine = StackMachine(program)
    machine.run()