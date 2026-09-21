"""acpi_patch: ASL-source patch pipeline for the Custom_Bootloader tables.

Layout:
- model.py          patch data model (LinePatch / BlockPatch / reports)
- engine.py         matching, application, verification, diagnostics
- patches_ssdt4.py  SSDT4 line patches (data only)
- patches_dsdt.py   DSDT method rewrites (data only)
- fragments/*.asl   verbatim replacement bodies spliced into the DSDT
- claims.py         machine-checked provenance for comment claims
- audit.py          comment-claim audit (single ASL channel; the C in-place
                    fallback channel was removed 2026-09-12)

Entry point: tools/generate_patched_tables.py (thin shim kept at the
historical path for tools/build_patched_tables.py and build.bat).
"""

__version__ = "2.0"
