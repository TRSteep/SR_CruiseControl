"""Development tool; needs pefile 2024.8.26 and capstone 5.x. No game assets emitted."""
import argparse
import hashlib
from pathlib import Path
import sys

parser = argparse.ArgumentParser()
parser.add_argument('exe', type=Path)
parser.add_argument('--dependency-dir', type=Path)
args = parser.parse_args()
if args.dependency_dir:
    sys.path.insert(0, str(args.dependency_dir))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_GRP_JUMP, CS_GRP_CALL
from capstone.x86_const import X86_OP_MEM, X86_REG_RIP, X86_OP_IMM

expected = 'a3cb442ec138eb2eb7e335e5c80e376766550740e49e233ac9eace035e182285'
raw = args.exe.read_bytes()
if hashlib.sha256(raw).hexdigest() != expected:
    raise SystemExit('Generator requires the reviewed Epic 44.1 reference EXE.')
pe = pefile.PE(data=raw)
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
functions = {e.struct.BeginAddress: e.struct.EndAddress for e in pe.DIRECTORY_ENTRY_EXCEPTION}
symbols = [('create', 0x10243a0), ('attach', 0x1b61880), ('get', 0x1b5d4b0),
           ('set', 0x1b5d870), ('invoke', 0x1b5dcb0), ('release', 0x1b5d0f0),
           ('empty', 0x1b61560), ('retain', 0x1b5d070), ('advance', 0x19960b0), ('destroy', 0x19a1730)]
lines = ['#pragma once', '// Generated from reviewed Epic 44.1. RIP displacements and relative branches are masked.',
         '// Candidate signatures identify code; they do not prove ABI compatibility on another build.',
         'namespace srcompat {', 'struct Symbol { const char* name; const char* pattern; };',
         'inline constexpr Symbol symbols[] = {']
for name, rva in symbols:
    end = functions[rva]
    instructions = list(md.disasm(pe.get_data(rva, min(128, end-rva)), pe.OPTIONAL_HEADER.ImageBase+rva))
    pattern = []
    for ins in instructions:
        data = list(ins.bytes)
        if any(op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP for op in ins.operands):
            data[ins.disp_offset:ins.disp_offset+ins.disp_size] = [None]*ins.disp_size
        if ins.group(CS_GRP_CALL) or ins.group(CS_GRP_JUMP):
            data[ins.imm_offset:ins.imm_offset+ins.imm_size] = [None]*ins.imm_size
        for op in ins.operands:
            if op.type == X86_OP_IMM and pe.OPTIONAL_HEADER.ImageBase <= op.imm < pe.OPTIONAL_HEADER.ImageBase+pe.OPTIONAL_HEADER.SizeOfImage:
                data[ins.imm_offset:ins.imm_offset+ins.imm_size] = [None]*ins.imm_size
        pattern.extend(data)
        if len(pattern) >= 64 or ins.mnemonic == 'ret':
            break
    if sum(b is not None for b in pattern) < 32:
        raise SystemExit('Insufficient fixed bytes: '+name)
    text = ' '.join('??' if b is None else f'{b:02X}' for b in pattern)
    lines.append(f'    {{"{name}","{text}"}},')
lines += ['};', '}']
Path(__file__).with_name('SymbolPatterns.h').write_text('\n'.join(lines)+'\n', encoding='utf-8')
print('Generated', len(symbols), 'bounded code signatures; validate uniqueness with the diagnostic.')
