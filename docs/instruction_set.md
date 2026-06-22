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
end;
++PC;
goto Next;
```