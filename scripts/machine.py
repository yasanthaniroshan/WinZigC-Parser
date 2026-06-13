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

    

class StackMachine:
    def __init__(self, code, stack_size=1000):
        self.code = code
        self.stack = [0] * stack_size
        self.top = -1
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

    def run(self):
        while True:
            try:
                instr = self.code[self.pc]

                op = instr[0]
                op = Op(op) if isinstance(op, str) else op
             
                if op == Op.SAVE:
                    n = instr[1]
                    self.stack[n] = self.pop()

                elif op == Op.LOAD:
                    n = instr[1]
                    self.push(self.stack[n])

                elif op == Op.NEGATE:
                    self.stack[self.top] = -self.stack[self.top]

                elif op == Op.NOT:
                    self.stack[self.top] = 0 if self.stack[self.top] else 1

                elif op == Op.ADD:
                    t = self.pop()
                    self.stack[self.top] += t

                elif op == Op.SUBTRACT:
                    t = self.pop()
                    self.stack[self.top] -= t

                elif op == Op.EQUAL:
                    t = self.pop()
                    self.stack[self.top] = 1 if self.stack[self.top] == t else 0

                elif op == Op.READ:
                    self.push(self.read_integer())

                elif op == Op.PRINT:
                    self.print_integer()

                elif op == Op.LIT:
                    n = instr[1]
                    self.push(n)

                elif op == Op.GOTO:
                    n = instr[1]
                    self.pc = n - 1
                    continue

                elif op == Op.IFFALSE:
                    n = instr[1]
                    if self.pop() == 0:
                        self.pc = n - 1
                        continue

                elif op == Op.IFTRUE:
                    n = instr[1]
                    if self.pop() == 1:
                        self.pc = n - 1
                        continue

                elif op == Op.STOP:
                    break

                self.pc += 1
            except IndexError:
                raise RuntimeError("Program counter out of bounds")
            
            except ValueError:
                raise RuntimeError("Invalid instruction: {}".format(instr))



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
            args = tuple(int(x) for x in parts[1:]) if len(parts) > 1 else ()
            program.append((op, *args))
    machine = StackMachine(program)
    machine.run()