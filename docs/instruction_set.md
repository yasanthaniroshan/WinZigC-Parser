# Stack Machine Instruction Set for WinZigC

The following is the stack machine instruction set which we generate for programs in WinZigC. 

The instruction-set is presented as pseudo-code here:

```
PC := 1;
Next:
case Code[PC] of
    save n:         stack[n] := stack[top--]
    load n:         stack[++top] := stack[n]
    negate:         stack[top] := -stack[top]
    not:            stack[top] := not stack[top]
    add:            t := stack[top--]; stack[top] := stack[top] + t;
    subtract:       t := stack[top--]; stack[top] := stack[top] - t;
    equal:          t := stack[top--]; stack[top] := stack[top] == t;
    lessequal:      t := stack[top--]; stack[top] := stack[top] <= t;
    read:           stack[++top] := getinteger(input)
    print:          putinteger(stack[top--])
    lit n:          stack[++top] := n
    goto n:         PC := n; goto Next
    iffalse n:      if stack[top--] == 0 then PC := n; goto Next endif
    iftrue n:       if stack[top--] == 1 then PC := n; goto Next endif
    stop:           halt
    // added instructions
    multiply:       t := stack[top--]; stack[top] := stack[top] * t;
    divide:         t := stack[top--]; stack[top] := stack[top] / t;
    mod:            t := stack[top--]; stack[top] := stack[top] mod t;
    and:            t := stack[top--]; stack[top] := stack[top] AND t;
    or:             t := stack[top--]; stack[top] := stack[top] OR t;
    lessthan:       t := stack[top--]; stack[top] := stack[top] < t;
    greaterthan:    t := stack[top--]; stack[top] := stack[top] > t;
    greaterequal:   t := stack[top--]; stack[top] := stack[top] >= t;
    notequal:       t := stack[top--]; stack[top] := stack[top] != t;
    call n:         // push (PC+1, fp) onto internal Call Stack; PC := n; goto Next
    return:         // r := stack[top--];                  -- function result
                    // (addr, oldfp) := pop Call Stack;
                    // top := fp - 1;                       -- discard this frame (incl. args)
                    // fp := oldfp;                         -- restore caller's frame
                    // stack[++top] := r;                   -- leave result on caller's stack
                    // PC := addr; goto Next
    lits:           stack[++top] := string
    prints:         putstring(stack[top--])
    // frame-relative addressing (function parameters and locals)
    enter argc:     fp := top - argc + 1   -- caller's pushed args become this frame's first slots
    reserve nvars:  top := top + nvars     -- reserve slots for non-parameter locals
    save_local i:   stack[fp + i] := stack[top--]
    load_local i:   stack[++top] := stack[fp + i]

end;
++PC;
goto Next;
```

## Variable addressing: globals vs. function locals

Variables live in two distinct address spaces, distinguished by the opcode used to
access them — never by the numeric operand alone (a global and a local can share
the same number).

- **Globals** (program scope) occupy fixed absolute slots `0 .. G-1` at the bottom
  of the stack and are accessed with `save` / `load`. The operand stack grows above
  this reserved region.
- **Function parameters and locals** are addressed *relative to the current frame
  pointer* `fp` with `save_local` / `load_local` (i.e. `stack[fp + i]`). Each
  function numbers its own params and locals from `0`, so these indices reset per
  function.

### Calling convention

`fp` marks the base of the currently executing function's activation frame.

1. The **caller** evaluates the arguments left-to-right, pushing them onto the
   operand stack, then issues `call`.
2. `call` saves the return address **and** the caller's `fp` so the callee can
   rebase without losing the caller's frame.
3. The **callee prologue** runs `enter argc`, which sets `fp = top - argc + 1` —
   the arguments the caller pushed *become* the frame's first `argc` slots (the
   parameters, indices `0 .. argc-1`). `reserve nvars` then lifts the operand stack
   above the function's non-parameter locals (indices `argc .. argc+nvars-1`).
4. `return` pops the result, discards the whole frame (`top := fp - 1`, freeing the
   arguments too), restores the caller's `fp`, and pushes the result back — so the
   call site ends with exactly one value (the result) where the arguments had been.

Because each `call` gives the callee a fresh `fp` *above* the caller's live operand
stack, simultaneously-live nested calls — and recursion — never collide: every
invocation gets its own frame rather than one permanently-reserved slot per variable.