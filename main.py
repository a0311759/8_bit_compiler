# It is a Code that stimulates 8 bit cpu working 


import sys
import re

class PipelineRegister:
    def __init__(self):
        self.instruction = None

class CPUSimulator:
    def __init__(self):
        self.registers = {f'R{i}': 0 for i in range(8)}  # R0–R7
        self.pipeline = [PipelineRegister() for _ in range(3)]
        # control-flow flags for simple IF/ELSE behaviour (single-line blocks)
        self.skip_else = False     # IF was true → skip ELSE block when ELSE arrives
        self.else_expected = False # IF was false → next ELSE should be honoured

    # ---------------- Helpers ----------------
    def _parse_value(self, token):
        """Return integer value represented by token (register, quoted char, or literal int)."""
        if token is None:
            return 0
        t = token.strip()
        if not t:
            return 0
        # register?
        if t.upper().startswith('R') and t.upper() in self.registers:
            return self.registers[t.upper()]
        # quoted char?
        if t.startswith('"') and t.endswith('"'):
            s = t[1:-1]
            return ord(s[0]) if s else 0
        # integer literal
        try:
            return int(t)
        except ValueError:
            raise ValueError(f"Cannot parse value: {token}")

    # ---------------- Decode ----------------
    def decode(self, instruction):
        if not instruction:
            return None

        instruction = instruction.strip()
        m = re.match(r'^(\S+)(?:\s+(.*))?$', instruction)
        if not m:
            return None
        op = m.group(1).upper()
        rest = (m.group(2) or '').strip()

        if op == 'IF':
            mm = re.match(r'^(\w+)\s*(==|!=|<=|>=|<|>)\s*(.+)$', rest)
            if not mm:
                raise ValueError("Invalid IF syntax. Use: IF <reg> <op> <value>")
            reg = mm.group(1)
            operator = mm.group(2)
            value = mm.group(3).strip()
            return (op, reg, operator, value)

        if op == 'WRITE':
            if not rest:
                raise ValueError("WRITE requires operands: WRITE <reg>, <value>")
            if ',' in rest:
                dest, src = [s.strip() for s in rest.split(',', 1)]
            else:
                parts = rest.split(None, 1)
                if len(parts) == 1:
                    dest, src = parts[0], ''
                else:
                    dest, src = parts[0], parts[1]
            return (op, dest, src)

        if op in ('ADD', 'SUB', 'MUL', 'DIV'):
            if not rest:
                raise ValueError(f"{op} requires operands: {op} dest, src1, src2")
            tokens = [t.strip() for t in re.split(r'\s*,\s*', rest) if t.strip()]
            if len(tokens) == 1:
                tokens = rest.split()
            if len(tokens) != 3:
                raise ValueError(f"{op} requires 3 operands: dest, src1, src2")
            return (op, tokens[0], tokens[1], tokens[2])

        if op in ('PRINT', 'INPUT'):
            if not rest:
                raise ValueError(f"{op} requires an operand")
            return (op, rest.strip())

        # generic fallback: split by commas first, else by whitespace
        tokens = [t.strip() for t in re.split(r'\s*,\s*', rest) if t.strip()]
        if not tokens:
            return (op,)
        return tuple([op] + tokens)

    # ---------------- Execute ----------------
    def execute(self):
        instr = self.pipeline[1].instruction
        if not instr:
            return

        op = instr[0]
        operands = instr[1:]

        # ---- WRITE
        if op == 'WRITE':
            if len(operands) < 2:
                raise ValueError("WRITE requires a register and a value")
            dest = operands[0].upper()
            src_token = operands[1]
            # allow register copy, quoted char, or immediate
            if src_token.startswith('"') and src_token.endswith('"'):
                text = src_token[1:-1]
                value = ord(text[0]) if text else 0
            elif src_token.upper().startswith('R') and src_token.upper() in self.registers:
                value = self.registers[src_token.upper()]
            else:
                value = int(src_token)
            # schedule write in writeback stage
            self.pipeline[2].instruction = ('WRITE', dest, value)

        # ---- ALU
        elif op in ('ADD', 'SUB', 'MUL', 'DIV'):
            if len(operands) != 3:
                raise ValueError(f"{op} requires dest, src1, src2")
            dest = operands[0].upper()
            s1 = operands[1].upper()
            s2 = operands[2].upper()
            val1 = self.registers.get(s1, 0) if s1 in self.registers else int(operands[1])
            val2 = self.registers.get(s2, 0) if s2 in self.registers else int(operands[2])
            if op == 'ADD':
                result = val1 + val2
            elif op == 'SUB':
                result = val1 - val2
            elif op == 'MUL':
                result = val1 * val2
            elif op == 'DIV':
                result = val1 // val2 if val2 != 0 else 0
            self.pipeline[2].instruction = ('WRITE', dest, result)

        # ---- PRINT
        elif op == 'PRINT':
            reg = operands[0].upper()
            self.pipeline[2].instruction = ('PRINT', reg)

        # ---- INPUT (immediate write to register)
        elif op == 'INPUT':
            reg = operands[0].upper()
            try:
                value = int(input(f"Enter value for {reg}: "))
            except Exception:
                value = 0
            # INPUT writes immediately (bypasses pipeline writeback for simplicity)
            self.registers[reg] = value
            # ensure nothing goes to writeback stage
            self.pipeline[2].instruction = None

        # ---- IF
        elif op == 'IF':
            if len(operands) != 3:
                raise ValueError("IF requires: IF <reg> <op> <value>")
            reg_token, operator, value_token = operands
            reg_val = self._parse_value(reg_token)
            cmp_val = self._parse_value(value_token)
            cond = False
            if operator == '==':
                cond = (reg_val == cmp_val)
            elif operator == '!=':
                cond = (reg_val != cmp_val)
            elif operator == '>':
                cond = (reg_val > cmp_val)
            elif operator == '>=':
                cond = (reg_val >= cmp_val)
            elif operator == '<':
                cond = (reg_val < cmp_val)
            elif operator == '<=':
                cond = (reg_val <= cmp_val)
            else:
                raise ValueError(f"Invalid operator: {operator}")

            # IF semantics (single-instruction blocks):
            # - If IF is True -> allow the immediate next instruction to run,
            #   but set skip_else so a later ELSE will be skipped.
            # - If IF is False -> skip the immediate next instruction, and mark else_expected
            #   so that an ELSE (if present) will enable its following instruction.
            if cond:
                self.skip_else = True
                self.else_expected = False
            else:
                # skip the next instruction currently in pipeline[0]
                if self.pipeline[0] is not None:
                    self.pipeline[0].instruction = None
                self.else_expected = True
                self.skip_else = False

            self.pipeline[2].instruction = None  # IF itself does not writeback

        # ---- ELSE
        elif op == 'ELSE':
            # If previous IF was True -> we should skip the block following ELSE
            if self.skip_else:
                if self.pipeline[0] is not None:
                    self.pipeline[0].instruction = None
                self.skip_else = False
                self.else_expected = False
            # If previous IF was False -> ELSE is active and the next instruction should run
            elif self.else_expected:
                self.else_expected = False
                # allow pipeline[0] as-is
            else:
                # ELSE without matching IF: treat as no-op and skip following instruction
                if self.pipeline[0] is not None:
                    self.pipeline[0].instruction = None

            self.pipeline[2].instruction = None

        else:
            raise ValueError(f"Unknown operation: {op}")

    # ---------------- Writeback ----------------
    def writeback(self):
        instr = self.pipeline[2].instruction
        if not instr:
            return

        op = instr[0]

        if op == 'WRITE':
            _, reg, value = instr
            self.registers[reg.upper()] = value

        elif op == 'PRINT':
            _, reg = instr
            val = self.registers.get(reg.upper(), 0)
            # Show character if ASCII printable
            if 32 <= val <= 126:
                print(chr(val), end="")
            else:
                print(val, end="")

        self.pipeline[2].instruction = None

    # ---------------- Pipeline Step ----------------
    def step(self, instruction):
        # Advance pipeline: writeback stage first
        self.writeback()
        # shift pipeline registers
        self.pipeline[2].instruction = self.pipeline[1].instruction
        self.pipeline[1].instruction = self.pipeline[0].instruction
        # decode new incoming instruction into pipeline[0]
        decoded = self.decode(instruction)
        self.pipeline[0].instruction = decoded
        # execute stage works on pipeline[1]
        self.execute()

    # ---------------- Run from file ----------------
    def run_from_file(self, filename):
        with open(filename, 'r') as f:
            for line in f:
                line = line.strip()
                # Ignore comments and blank lines
                if not line or line.startswith('#'):
                    continue
                if '#' in line:  # remove inline comment
                    line = line.split('#', 1)[0].strip()
                if line:
                    self.step(line)

        # Drain pipeline: allow any lingering instructions to writeback
        for _ in range(3):
            self.writeback()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python main.py <program.test_ins>")
    else:
        cpu = CPUSimulator()
        filename = sys.argv[1]
        cpu.run_from_file(filename)
        print("\n\nFinal Registers:", cpu.registers)
          
