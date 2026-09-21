# fragments/

ASL replacement bodies spliced verbatim into `dsdt_patched.dsl` by the
BlockPatch definitions in `../patches_dsdt.py`.

Rules:

- The file contents land in the DSDT **byte for byte** - there is no
  re-indentation, no header injection, no trailing-newline fixup.
- The trailing blank lines at the end of each file are intentional: they
  reproduce the inter-method spacing of the original generator (two blank
  lines before the next `Method (` header).
- A fragment that replaces `Method (FNQS ...)` must start with that exact
  method header line (the engine splices from the start marker line up to,
  not including, the end marker line).

After editing a fragment, run:

    python tools/generate_patched_tables.py --dry-run   # counts/anchors still match
    python tools/build_patched_tables.py                # compile + AML round-trip gate

The round-trip gate checks the marker sequences declared in each BlockPatch
(`roundtrip_present` / `roundtrip_absent`) against the disassembled AML in
`build/roundtrip/`, so a fragment edit that breaks the expected behavior
fails the build instead of silently shipping.
