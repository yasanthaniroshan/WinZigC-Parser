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
    def __init__(self, code, num_globals=0, start=0, stack_size=1000):
        # `code` is the fully resolved instruction list (labels already turned into
        # 1-based indices, globals into slots, string labels into text) produced by
        # `assemble()`. The execution engine itself is unchanged.
        self.code = code
        self.stack = [0] * stack_size
        self.call_stack = [] # for tracking function calls

        # The bottom `num_globals` slots (declared in the .data section) are
        # reserved for global variables; the operand stack must start ABOVE them.
        self.top = num_globals - 1
        # Frame pointer: base of the current function's activation frame. Globals
        # (save/load) are absolute slots; function params/locals (save_local/
        # load_local) are stack[fp + index]. `fp` is set on every `enter`, saved on
        # `call`, restored on `return`. The first free slot above the global region
        # is the initial base for the main program's nested calls.
        self.fp = num_globals
        self.pc = start # execution begins at the `main` label
        self.input_ptr = 0

    def push(self, value):
        self.top += 1
        if self.top >= len(self.stack):
            # Out of stack slots: almost always unbounded or too-deep recursion.
            # Raise a clear message instead of a bare IndexError from the list.
            raise RuntimeError(
                "Stack overflow: exceeded {} stack slots "
                "(too-deep or unbounded recursion?)".format(len(self.stack)))
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
                    # `lit` always carries an integer operand (the parser hands it
                    # to us as a string). Push it as an int so values flowing
                    # through save/load keep comparing correctly in iffalse/iftrue.
                    n = instr[1]
                    self.push(int(n))

                elif op == Op.GOTO:
                    n = instr[1]
                    self.pc = int(n) - 1
                    continue

                elif op == Op.IFFALSE:
                    n = instr[1]
                    if int(self.pop()) == 0:
                        self.pc = int(n) - 1
                        continue

                elif op == Op.IFTRUE:
                    n = instr[1]
                    if int(self.pop()) == 1:
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



JUMP_OPS = ("goto", "iffalse", "iftrue", "call")
MEM_OPS = ("save", "load")


def assemble(asm_file):
    """Two-pass assembler for the sectioned, labeled .asm format.

    The file is organised into sections:
        .data    - one global variable per line ("name: 0")
        .rodata  - string literals (".LC0: \"text\"")
        .text    - code, with `.globl`, `label:` lines and instructions
    Pass 1 collects section contents and records the index of every code label.
    Pass 2 resolves operands back to the plain numeric form the engine runs:
    jump/call targets -> 1-based instruction index, save/load -> global slot,
    lits -> the literal text. Returns (code, num_globals, start_index).
    """
    section = None
    data_names = []      # global variable names, in declaration order
    rodata = {}          # .LC label -> string text
    raw = []             # ("label", name) | ("instr", op, operand) in .text order

    with open(asm_file) as f:
        for line in f:
            line = line.split("#", 1)[0].strip()  # strip comments / whitespace
            if not line:
                continue
            if line in (".data", ".rodata", ".text"):
                section = line
                continue
            if line.startswith(".globl"):
                continue  # entry point is the `main` label; nothing to do
            if section == ".data":
                data_names.append(line.split(":", 1)[0].strip())
                continue
            if section == ".rodata":
                label, _, text = line.partition(":")
                text = text.strip()
                if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
                    text = text[1:-1]
                rodata[label.strip()] = text
                continue
            # .text
            if line.endswith(":"):
                raw.append(("label", line[:-1]))
                continue
            parts = line.split()
            op = parts[0]
            operand = " ".join(parts[1:]) if len(parts) > 1 else None
            raw.append(("instr", op, operand))

    # Pass 1: flatten instructions and record label -> instruction index.
    code = []
    labels = {}
    for item in raw:
        if item[0] == "label":
            labels[item[1]] = len(code)
        else:
            code.append([item[1], item[2]])

    slot = {name: i for i, name in enumerate(data_names)}

    # Pass 2: resolve operands to the numeric form the engine expects.
    resolved = []
    for op, operand in code:
        if op in JUMP_OPS:
            resolved.append((op, str(labels[operand] + 1)))  # engine does int(n) - 1
        elif op in MEM_OPS:
            resolved.append((op, str(slot[operand])))
        elif op == "lits":
            resolved.append((op, rodata.get(operand, operand)))
        else:
            resolved.append((op, operand))

    return resolved, len(data_names), labels.get("main", 0)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 machine.py [program.asm]")
        sys.exit(1)
    program, num_globals, start = assemble(sys.argv[1])
    machine = StackMachine(program, num_globals, start)
    try:
        machine.run()
    except (RuntimeError, ZeroDivisionError) as e:
        # Report runtime faults (stack overflow, bad PC, division by zero, ...)
        # as a clean one-line message rather than a Python traceback.
        print("Runtime error: {}".format(e), file=sys.stderr)
        sys.exit(1)