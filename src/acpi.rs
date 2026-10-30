use alloc::format;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use core::ptr;

use uefi::boot::{self, AllocateType};
use uefi::fs::FileSystem;
use uefi::mem::memory_map::MemoryType;
use uefi::proto::media::fs::SimpleFileSystem;
use uefi::system;
use uefi::table::cfg::ConfigTableEntry;
use uefi::CString16;

use crate::filelog as log;

pub(crate) const SIG_DSDT: u32 = u32::from_le_bytes(*b"DSDT");
pub(crate) const SIG_BGRT: u32 = u32::from_le_bytes(*b"BGRT");
pub(crate) const SIG_XSDT: u32 = u32::from_le_bytes(*b"XSDT");
pub(crate) const SIG_RSDT: u32 = u32::from_le_bytes(*b"RSDT");
const SIG_FACP: u32 = u32::from_le_bytes(*b"FACP");

pub(crate) const HDR_LEN: usize = 36;
const MAX_TABLES: usize = 16;
const LOW_MEMORY_LIMIT: u64 = 0x10_0000;
const HIGH_MEMORY_LIMIT: u64 = 0x40_0000_0000;

pub(crate) unsafe fn rd_u32(p: *const u8, off: usize) -> u32 {
    ptr::read_unaligned(p.add(off).cast::<u32>())
}

pub(crate) unsafe fn rd_u64(p: *const u8, off: usize) -> u64 {
    ptr::read_unaligned(p.add(off).cast::<u64>())
}

pub(crate) unsafe fn wr_u8(p: *mut u8, off: usize, v: u8) {
    ptr::write_unaligned(p.add(off), v);
}

pub(crate) unsafe fn wr_u16(p: *mut u8, off: usize, v: u16) {
    ptr::write_unaligned(p.add(off).cast::<u16>(), v);
}

pub(crate) unsafe fn wr_u32(p: *mut u8, off: usize, v: u32) {
    ptr::write_unaligned(p.add(off).cast::<u32>(), v);
}

pub(crate) unsafe fn wr_u64(p: *mut u8, off: usize, v: u64) {
    ptr::write_unaligned(p.add(off).cast::<u64>(), v);
}

pub(crate) unsafe fn table_signature(t: *const u8) -> u32 {
    rd_u32(t, 0)
}

pub(crate) unsafe fn table_length(t: *const u8) -> u32 {
    rd_u32(t, 4)
}

pub(crate) unsafe fn is_valid(t: *const u8) -> bool {
    let addr = t as usize as u64;
    !t.is_null() && addr & 3 == 0 && addr >= LOW_MEMORY_LIMIT && addr <= HIGH_MEMORY_LIMIT
}

fn checksum8(p: *const u8, len: usize) -> u8 {
    let mut sum: u8 = 0;
    for i in 0..len {
        sum = sum.wrapping_add(unsafe { *p.add(i) });
    }
    0u8.wrapping_sub(sum)
}

pub(crate) unsafe fn fix_checksum(t: *mut u8) {
    let len = table_length(t) as usize;
    wr_u8(t, 9, 0);
    let sum = checksum8(t, len);
    wr_u8(t, 9, sum);
}

pub(crate) unsafe fn fix_rsdp_checksums(r: *mut u8) {
    wr_u8(r, 8, 0);
    let sum = checksum8(r, 20);
    wr_u8(r, 8, sum);

    let mut len = rd_u32(r, 20) as usize;
    if len < 36 {
        len = 36;
    }
    wr_u8(r, 32, 0);
    let sum2 = checksum8(r, len);
    wr_u8(r, 32, sum2);
}

pub(crate) unsafe fn rsdp_xsdt(r: *const u8) -> u64 {
    rd_u64(r, 24)
}

pub(crate) unsafe fn rsdp_rsdt(r: *const u8) -> u32 {
    rd_u32(r, 16)
}

pub fn find_rsdp() -> Option<*mut u8> {
    system::with_config_table(|entries| {
        entries
            .iter()
            .find(|e| e.guid == ConfigTableEntry::ACPI2_GUID)
            .map(|e| e.address.cast_mut().cast::<u8>())
    })
}

fn find_table_by_signature(rsdp: *mut u8, signature: u32) -> Option<*mut u8> {
    unsafe {
        let xsdt = rsdp_xsdt(rsdp);
        let (root, entry_size, expected) = if xsdt != 0 {
            (xsdt as usize as *mut u8, 8usize, SIG_XSDT)
        } else {
            let rsdt = rsdp_rsdt(rsdp);
            if rsdt == 0 {
                return None;
            }
            (rsdt as usize as *mut u8, 4usize, SIG_RSDT)
        };

        if !is_valid(root) || table_signature(root) != expected {
            return None;
        }

        let total = table_length(root) as usize;
        if total < HDR_LEN {
            return None;
        }

        let count = (total - HDR_LEN) / entry_size;
        for i in 0..count {
            let off = HDR_LEN + i * entry_size;
            let addr = if entry_size == 8 {
                rd_u64(root, off) as usize
            } else {
                rd_u32(root, off) as usize
            };
            if addr == 0 {
                continue;
            }
            let table = addr as *mut u8;
            if !is_valid(table) {
                continue;
            }
            if table_signature(table) == signature {
                return Some(table);
            }
        }
        None
    }
}

unsafe fn contains_bytes(t: *const u8, len: usize, needle: &[u8]) -> bool {
    if len < needle.len() {
        return false;
    }
    let hay = core::slice::from_raw_parts(t, len);
    hay.windows(needle.len()).any(|w| w == needle)
}

fn apply_dsdt_override(rsdp: *mut u8, new_table: *mut u8, table_len: usize) -> bool {
    let Some(fadt) = find_table_by_signature(rsdp, SIG_FACP) else {
        log::write("[Acpi] FACP not found for DSDT replacement");
        return false;
    };

    unsafe {
        let fadt_len = table_length(fadt) as usize;
        let old_addr = if fadt_len >= 148 {
            let addr = rd_u64(fadt, 140);
            if addr != 0 {
                addr
            } else if fadt_len >= 44 {
                rd_u32(fadt, 40) as u64
            } else {
                0
            }
        } else if fadt_len >= 44 {
            rd_u32(fadt, 40) as u64
        } else {
            0
        };

        if fadt_len >= 148 {
            wr_u64(fadt, 140, new_table as u64);
        }
        if fadt_len >= 44 {
            wr_u32(fadt, 40, new_table as u32);
        }
        fix_checksum(fadt);

        log::write(&format!("[Acpi] old DSDT @ 0x{old_addr:016X}"));
        log::write(&format!("[Acpi] new DSDT @ 0x{:016X}", new_table as usize));
        log::write(&format!("[Acpi] new DSDT len: {table_len}"));
        log::write("[Acpi] === DSDT override ACTIVE ===");
    }
    true
}

fn apply_ssdt_override_or_inject(rsdp: *mut u8, new_table: *mut u8) -> bool {
    unsafe {
        let xsdt_addr = rsdp_xsdt(rsdp);
        if xsdt_addr == 0 {
            log::write("[Acpi] XSDT invalid for SSDT update");
            return false;
        }
        let xsdt = xsdt_addr as usize as *mut u8;
        if !is_valid(xsdt) || table_signature(xsdt) != SIG_XSDT {
            log::write("[Acpi] XSDT invalid for SSDT update");
            return false;
        }

        let rsdt_addr = rsdp_rsdt(rsdp);
        let rsdt = rsdt_addr as usize as *mut u8;

        let new_len = table_length(new_table) as usize;
        let new_sig = table_signature(new_table);
        let new_id = rd_u64(new_table, 16);
        let new_rev = rd_u32(new_table, 24);
        let new_has_npcf = contains_bytes(new_table, new_len, b"NPCF");

        let count = (table_length(xsdt) as usize - HDR_LEN) / 8;
        for i in 0..count {
            let off = HDR_LEN + i * 8;
            let old_addr = rd_u64(xsdt, off);
            if old_addr == 0 {
                continue;
            }
            let old = old_addr as usize as *mut u8;
            if !is_valid(old) {
                continue;
            }
            if table_signature(old) != new_sig
                || rd_u64(old, 16) != new_id
                || rd_u32(old, 24) != new_rev
            {
                continue;
            }
            if new_has_npcf && !contains_bytes(old, table_length(old) as usize, b"NPCF") {
                continue;
            }

            wr_u64(xsdt, off, new_table as u64);
            fix_checksum(xsdt);

            if new_table as u64 <= 0xFFFF_FFFF
                && !rsdt.is_null()
                && is_valid(rsdt)
                && table_signature(rsdt) == SIG_RSDT
            {
                let rcount = (table_length(rsdt) as usize - HDR_LEN) / 4;
                for k in 0..rcount {
                    let roff = HDR_LEN + k * 4;
                    if rd_u32(rsdt, roff) as u64 == old_addr {
                        wr_u32(rsdt, roff, new_table as u32);
                        break;
                    }
                }
                fix_checksum(rsdt);
            }

            log::write(&format!("[Acpi] replaced SSDT old @ 0x{old_addr:016X}"));
            log::write(&format!("[Acpi] replaced SSDT new @ 0x{:016X}", new_table as usize));
            log::write("[Acpi] === SSDT table override ACTIVE (in-place XSDT match) ===");
            return true;
        }

        let old_len = table_length(xsdt) as usize;
        let expanded_len = old_len + 8;
        let pages = (expanded_len + 0xFFF) / 0x1000;
        let new_xsdt = match boot::allocate_pages(
            AllocateType::MaxAddress(0xFFFF_FFFF),
            MemoryType::ACPI_RECLAIM,
            pages,
        ) {
            Ok(p) => p.as_ptr(),
            Err(_) => {
                log::write("[Acpi] failed to allocate memory for enlarged XSDT");
                return false;
            }
        };

        ptr::copy_nonoverlapping(xsdt, new_xsdt, old_len);
        wr_u64(new_xsdt, old_len, new_table as u64);
        wr_u32(new_xsdt, 4, expanded_len as u32);
        fix_checksum(new_xsdt);

        wr_u64(rsdp, 24, new_xsdt as u64);
        fix_rsdp_checksums(rsdp);

        log::write(&format!("[Acpi] injected new SSDT @ 0x{:016X}", new_table as usize));
        log::write("[Acpi] === SSDT table injected into XSDT successfully ===");
        true
    }
}

fn read_file(fs: &mut FileSystem, path: &str) -> Option<Vec<u8>> {
    let path = CString16::try_from(path).ok()?;
    let data = fs.read(path.as_ref()).ok()?;
    if data.is_empty() {
        None
    } else {
        Some(data)
    }
}

fn has_aml_extension(name: &str) -> bool {
    let bytes = name.as_bytes();
    if bytes.len() < 4 {
        return false;
    }
    let tail = &bytes[bytes.len() - 4..];
    tail[0] == b'.' && tail[1].eq_ignore_ascii_case(&b'a') && tail[2].eq_ignore_ascii_case(&b'm')
        && tail[3].eq_ignore_ascii_case(&b'l')
}

fn is_duplicate(tables: &[FoundTable], data: &[u8]) -> bool {
    if data.len() < HDR_LEN {
        return false;
    }
    unsafe {
        let sig = table_signature(data.as_ptr());
        let id = rd_u64(data.as_ptr(), 16);
        tables.iter().any(|existing| {
            existing.data.len() == data.len()
                && table_signature(existing.data.as_ptr()) == sig
                && rd_u64(existing.data.as_ptr(), 16) == id
        })
    }
}

struct FoundTable {
    name: String,
    data: Vec<u8>,
    from_dir: bool,
}

fn scan_volume(fs: &mut FileSystem, tables: &mut Vec<FoundTable>) {
    const DIRECT: [&str; 6] = [
        "EFI\\Custom\\dsdt_mod.aml",
        "\\EFI\\Custom\\dsdt_mod.aml",
        "EFI\\Custom\\ssdt4_mod.aml",
        "\\EFI\\Custom\\ssdt4_mod.aml",
        "EFI\\Custom\\ssdt_mod.aml",
        "\\EFI\\Custom\\ssdt_mod.aml",
    ];

    for candidate in DIRECT {
        if tables.len() >= MAX_TABLES {
            break;
        }
        if let Some(data) = read_file(fs, candidate) {
            if is_duplicate(&tables, &data) {
                continue;
            }
            tables.push(FoundTable {
                name: candidate.to_string(),
                data,
                from_dir: false,
            });
        }
    }

    if tables.len() >= MAX_TABLES {
        return;
    }

    let Ok(dir_path) = CString16::try_from("\\EFI\\Custom") else {
        return;
    };
    let Ok(iter) = fs.read_dir(dir_path.as_ref()) else {
        return;
    };

    for entry in iter.flatten() {
        if tables.len() >= MAX_TABLES {
            break;
        }
        if entry.is_directory() {
            continue;
        }
        let name = format!("{}", entry.file_name());
        if !has_aml_extension(&name) {
            continue;
        }
        let path = format!("\\EFI\\Custom\\{name}");
        if let Some(data) = read_file(fs, &path) {
            if is_duplicate(&tables, &data) {
                continue;
            }
            tables.push(FoundTable {
                name,
                data,
                from_dir: true,
            });
        }
    }
}

fn scan_tables() -> Vec<FoundTable> {
    let mut tables = Vec::new();
    if let Ok(sfs) = boot::get_image_file_system(boot::image_handle()) {
        let mut fs = FileSystem::new(sfs);
        scan_volume(&mut fs, &mut tables);
    }
    if tables.is_empty() {
        if let Ok(handles) = boot::find_handles::<SimpleFileSystem>() {
            for handle in handles {
                if tables.len() >= MAX_TABLES {
                    break;
                }
                if let Ok(sfs) = boot::open_protocol_exclusive::<SimpleFileSystem>(handle) {
                    let mut fs = FileSystem::new(sfs);
                    scan_volume(&mut fs, &mut tables);
                }
            }
        }
    }
    tables
}

fn collect_tables() -> Vec<(String, Vec<u8>)> {
    let found = scan_tables();
    for entry in &found {
        if entry.from_dir {
            log::write(&format!("[Acpi] Found table via dir enum: {}", entry.name));
        } else {
            log::write(&format!("[Acpi] Found table: {}", entry.name));
            log::write(&format!("[Acpi] File size: {}", entry.data.len()));
        }
    }
    found.into_iter().map(|e| (e.name, e.data)).collect()
}

fn verify_applied(rsdp: *mut u8, dsdt: Option<*mut u8>, ssdts: &[*mut u8]) {
    unsafe {
        if let Some(new_dsdt) = dsdt {
            match find_table_by_signature(rsdp, SIG_FACP) {
                Some(fadt) => {
                    let len = table_length(fadt) as usize;
                    let x_dsdt = if len >= 148 { rd_u64(fadt, 140) } else { 0 };
                    let ok = x_dsdt == new_dsdt as u64;
                    log::write(&format!(
                        "[Acpi] VERIFY DSDT: FACP.X_DSDT=0x{x_dsdt:016X} expected=0x{:016X} {}",
                        new_dsdt as usize,
                        if ok { "OK" } else { "FAILED (write did not stick)" }
                    ));
                }
                None => log::write("[Acpi] VERIFY DSDT: FACP missing after override"),
            }
        }

        if ssdts.is_empty() {
            return;
        }

        let xsdt = rsdp_xsdt(rsdp) as usize as *mut u8;
        if !is_valid(xsdt) || table_signature(xsdt) != SIG_XSDT {
            log::write("[Acpi] VERIFY SSDT: XSDT invalid after override");
            return;
        }

        let count = (table_length(xsdt) as usize - HDR_LEN) / 8;
        for &table in ssdts {
            let mut found = false;
            for i in 0..count {
                if rd_u64(xsdt, HDR_LEN + i * 8) == table as u64 {
                    found = true;
                    break;
                }
            }
            log::write(&format!(
                "[Acpi] VERIFY SSDT @ 0x{:016X}: {}",
                table as usize,
                if found { "OK" } else { "FAILED (pointer not in XSDT)" }
            ));
        }
    }
}

pub fn override_from_esp() {
    let tables = collect_tables();
    if tables.is_empty() {
        log::write("[Acpi] no override tables in ESP \\EFI\\Custom - passthrough");
        return;
    }
    log::write(&format!("[Acpi] found override tables count: {}", tables.len()));

    let Some(rsdp) = find_rsdp() else {
        log::write("[Acpi] ACPI 2.0 RSDP not found - skip");
        return;
    };

    let mut applied = 0usize;
    let mut applied_dsdt: Option<*mut u8> = None;
    let mut applied_ssdts: Vec<*mut u8> = Vec::new();
    for (name, data) in &tables {
        log::write(&format!("[Acpi] processing custom table: {name}"));

        if data.len() < HDR_LEN {
            log::write("[Acpi] invalid table header - skip");
            continue;
        }

        let table_len = unsafe { table_length(data.as_ptr()) } as usize;
        if table_len < HDR_LEN || table_len > data.len() {
            log::write("[Acpi] invalid table header - skip");
            continue;
        }

        let pages = (table_len + 0xFFF) / 0x1000;
        let new_table = match boot::allocate_pages(
            AllocateType::MaxAddress(0xFFFF_FFFF),
            MemoryType::ACPI_RECLAIM,
            pages,
        ) {
            Ok(p) => p.as_ptr(),
            Err(_) => {
                log::write("[Acpi] allocate reclaim pages failed - skip");
                continue;
            }
        };

        unsafe {
            ptr::copy_nonoverlapping(data.as_ptr(), new_table, table_len);
            fix_checksum(new_table);
        }

        let signature = unsafe { table_signature(new_table) };
        let ok = if signature == SIG_DSDT {
            apply_dsdt_override(rsdp, new_table, table_len)
        } else {
            apply_ssdt_override_or_inject(rsdp, new_table)
        };
        if ok {
            applied += 1;
            if signature == SIG_DSDT {
                applied_dsdt = Some(new_table);
            } else {
                applied_ssdts.push(new_table);
            }
        }
    }

    verify_applied(rsdp, applied_dsdt, &applied_ssdts);
    log::write(&format!("[Acpi] total override tables applied: {applied}"));
}
