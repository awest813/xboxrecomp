from tools.recomp.scheduler_hints import HINT, instrument


def test_poll_backedge_and_entry_are_safe_points():
    code = """void sub_00010000(void)
{
loc_00010000: ;
    eax = MEM32(0x20000);
    if (eax) goto loc_00010010;
    goto loc_00010000;
loc_00010010: ;
    esp += 4; return;
}
"""
    result = instrument(code)
    assert result.count(HINT) == 2
    assert "loc_00010000: ;\n" + HINT in result
    assert "loc_00010010: ;\n" + HINT not in result
    assert instrument(result) == result
    assert result.replace(HINT + "\n", "") == code


def test_switch_backedge_is_seen_but_commented_dead_branch_is_not():
    code = """void helper(void)
{
loc_00010000: ;
    /* goto loc_00010000; - dead code */
loc_00010010: ;
    if (_jt == 0x10010) goto loc_00010010;
}
"""
    result = instrument(code)
    assert result.count(HINT) == 2
    assert "loc_00010000: ;\n" + HINT not in result
    assert "loc_00010010: ;\n" + HINT in result
