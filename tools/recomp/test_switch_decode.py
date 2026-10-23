"""Embedded tables must recover complete, non-overlapping instruction streams."""

import struct

import pytest

from tools.recomp import config
from tools.recomp.disasm import Disassembler, Operand
from tools.recomp.lifter import Lifter


BASE = 0x10000


@pytest.fixture
def image_config(monkeypatch):
    # Restore the process-wide memory map after each synthetic image.
    monkeypatch.setattr(config, "_SECTIONS", [
        config.Section(".text", BASE, 0x100, 0, 0x100, True)])


def test_downward_table_includes_zero_index(image_config):
    image = bytearray(0x100)
    table = BASE + 0x60
    targets = [BASE + 0x80 + i for i in range(8)]
    # Seven negative indexes and index zero, preceded/followed by non-pointers.
    struct.pack_into("<8I", image, 0x44, *targets)
    lifter = Lifter(xbe_data=bytes(image))
    lifter.func_start, lifter.func_end = BASE, BASE + len(image)
    operand = Operand(type="mem", mem_index="ecx", mem_scale=4,
                      mem_disp=table, mem_size=4)

    assert lifter._analyze_switch_table([operand]) == targets


def test_resync_replaces_stale_instruction_starts():
    # Linear decode: mov eax, imm32 at +0; inc esi at +5; add al, imm8 at +6.
    # Actual arm at +1: mov eax, [ebp+8]; mov eax, [esi+4]; ret.
    image = bytes.fromhex("B8 8B 45 08 8B 46 04 C3")
    decoded = Disassembler().disassemble_function(
        image, BASE, BASE + len(image), resync={BASE + 1})

    assert [(i.address - BASE, i.mnemonic) for i in decoded] == [
        (1, "mov"), (4, "mov"), (7, "ret")]
    assert all(a.end_address <= b.address for a, b in zip(decoded, decoded[1:]))


def test_skipped_first_entry_does_not_truncate_long_table(image_config):
    image = bytearray(0x100)
    targets = [BASE + 0x80 + i for i in range(15)]
    # Index zero is unused. XNET's checksum uses all fifteen other arms.
    struct.pack_into("<15I", image, 0x24, *targets)
    lifter = Lifter(xbe_data=bytes(image))
    lifter.func_start, lifter.func_end = BASE, BASE + len(image)
    operand = Operand(type="mem", mem_index="ebx", mem_scale=4,
                      mem_disp=BASE + 0x20, mem_size=4)

    assert lifter._analyze_switch_table([operand]) == targets


def test_probe_near_end_of_image_is_bounded(image_config):
    image = struct.pack("<I", BASE + 1)
    lifter = Lifter(xbe_data=image)
    lifter.func_start, lifter.func_end = BASE, BASE + len(image)
    operand = Operand(type="mem", mem_index="eax", mem_scale=4,
                      mem_disp=BASE, mem_size=4)

    assert lifter._analyze_switch_table([operand]) == []
