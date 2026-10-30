use alloc::vec::Vec;
use core::ptr;

use uefi::boot::{self, AllocateType};
use uefi::mem::memory_map::MemoryType;
use uefi::proto::console::gop::{BltOp, BltPixel, BltRegion, GraphicsOutput};
use uefi::system;

use crate::acpi::{self, fix_checksum, is_valid, rd_u32, rd_u64, table_length, table_signature, wr_u32, wr_u64, HDR_LEN, SIG_BGRT, SIG_RSDT, SIG_XSDT};
use crate::filelog as log;

const LOGO: &[u8] = include_bytes!("../tools/logo.bmp");
const BMP_HEADER_LEN: usize = 54;
const BGRT_LEN: usize = 56;

fn u16_at(data: &[u8], off: usize) -> Option<u16> {
    let bytes = data.get(off..off + 2)?;
    Some(u16::from_le_bytes([bytes[0], bytes[1]]))
}

fn u32_at(data: &[u8], off: usize) -> Option<u32> {
    let bytes = data.get(off..off + 4)?;
    Some(u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]))
}

struct Bmp {
    width: u32,
    height: u32,
    bit_count: u16,
    compression: u32,
    off_bits: u32,
}

impl Bmp {
    fn parse(data: &[u8]) -> Option<Self> {
        if data.len() < BMP_HEADER_LEN {
            return None;
        }
        if u16_at(data, 0)? != 0x4D42 {
            return None;
        }
        Some(Self {
            width: u32_at(data, 18)?,
            height: u32_at(data, 22)?,
            bit_count: u16_at(data, 28)?,
            compression: u32_at(data, 30)?,
            off_bits: u32_at(data, 10)?,
        })
    }
}

fn find_bgrt(xsdt: *mut u8) -> Option<(usize, u64)> {
    unsafe {
        let total = table_length(xsdt) as usize;
        if total < HDR_LEN {
            return None;
        }
        let count = (total - HDR_LEN) / 8;
        for i in 0..count {
            let off = HDR_LEN + i * 8;
            let addr = rd_u64(xsdt, off);
            if addr == 0 {
                continue;
            }
            let table = addr as usize as *const u8;
            if is_valid(table) && table_signature(table) == SIG_BGRT {
                return Some((off, addr));
            }
        }
        None
    }
}

fn sync_rsdt(rsdp: *mut u8, old_addr: u64, new_addr: u64) {
    unsafe {
        if new_addr > 0xFFFF_FFFF {
            return;
        }
        let rsdt = acpi::rsdp_rsdt(rsdp) as usize as *mut u8;
        if rsdt.is_null() || !is_valid(rsdt) || table_signature(rsdt) != SIG_RSDT {
            return;
        }
        let count = (table_length(rsdt) as usize - HDR_LEN) / 4;
        for i in 0..count {
            let off = HDR_LEN + i * 4;
            if rd_u32(rsdt, off) as u64 == old_addr {
                wr_u32(rsdt, off, new_addr as u32);
                break;
            }
        }
        fix_checksum(rsdt);
    }
}

fn create_or_update_bgrt(
    rsdp: *mut u8,
    bmp_addr: u64,
    offset_x: u32,
    offset_y: u32,
) -> Option<*mut u8> {
    unsafe {
        let xsdt_addr = acpi::rsdp_xsdt(rsdp);
        if xsdt_addr == 0 {
            log::write("[-] BGRT: Invalid RSDP or missing XSDT address.");
            return None;
        }
        let xsdt = xsdt_addr as usize as *mut u8;
        if !is_valid(xsdt) || table_signature(xsdt) != SIG_XSDT {
            log::write("[-] BGRT: Invalid XSDT pointer or signature.");
            return None;
        }

        if let Some((off, old_addr)) = find_bgrt(xsdt) {
            let old = old_addr as usize as *mut u8;
            let bgrt_len = table_length(old) as usize;
            let pages = (bgrt_len + 0xFFF) / 0x1000;
            let new = match boot::allocate_pages(
                AllocateType::MaxAddress(0xFFFF_FFFF),
                MemoryType::ACPI_RECLAIM,
                pages,
            ) {
                Ok(p) => p.as_ptr(),
                Err(_) => {
                    log::write("[-] BGRT: Failed to allocate shadow copy of existing BGRT");
                    return None;
                }
            };

            ptr::copy_nonoverlapping(old, new, bgrt_len);
            acpi::wr_u16(new, 36, 1);
            acpi::wr_u8(new, 38, 1);
            acpi::wr_u8(new, 39, 0);
            wr_u64(new, 40, bmp_addr);
            wr_u32(new, 48, offset_x);
            wr_u32(new, 52, offset_y);
            fix_checksum(new);

            wr_u64(xsdt, off, new as u64);
            fix_checksum(xsdt);
            sync_rsdt(rsdp, old_addr, new as u64);

            log::write("[+] BGRT: Existing BGRT shadow-copied and redirected with new logo.");
            return Some(new);
        }

        log::write("[+] BGRT: No existing BGRT table in XSDT. Creating new BGRT table...");

        let bgrt = match boot::allocate_pages(
            AllocateType::MaxAddress(0xFFFF_FFFF),
            MemoryType::ACPI_RECLAIM,
            1,
        ) {
            Ok(p) => p.as_ptr(),
            Err(_) => {
                log::write("[-] BGRT: Failed to allocate memory for BGRT table");
                return None;
            }
        };

        ptr::write_bytes(bgrt, 0, BGRT_LEN);
        wr_u32(bgrt, 0, SIG_BGRT);
        wr_u32(bgrt, 4, BGRT_LEN as u32);
        acpi::wr_u8(bgrt, 8, 1);
        acpi::wr_u8(bgrt, 9, 0);
        ptr::copy_nonoverlapping(b"Mtblx*".as_ptr(), bgrt.add(10), 6);
        wr_u64(bgrt, 16, u64::from_le_bytes(*b"HackBGRT"));
        wr_u32(bgrt, 24, 1);
        wr_u32(bgrt, 28, 0x454E4F4E);
        wr_u32(bgrt, 32, 1);
        acpi::wr_u16(bgrt, 36, 1);
        acpi::wr_u8(bgrt, 38, 1);
        acpi::wr_u8(bgrt, 39, 0);
        wr_u64(bgrt, 40, bmp_addr);
        wr_u32(bgrt, 48, offset_x);
        wr_u32(bgrt, 52, offset_y);
        fix_checksum(bgrt);

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
                log::write("[-] BGRT: Failed to allocate memory for expanded XSDT");
                return None;
            }
        };

        ptr::copy_nonoverlapping(xsdt, new_xsdt, old_len);
        wr_u64(new_xsdt, old_len, bgrt as u64);
        wr_u32(new_xsdt, 4, expanded_len as u32);
        fix_checksum(new_xsdt);

        wr_u64(rsdp, 24, new_xsdt as u64);
        acpi::fix_rsdp_checksums(rsdp);

        let rsdt_addr = acpi::rsdp_rsdt(rsdp);
        if rsdt_addr != 0 {
            let rsdt = rsdt_addr as usize as *mut u8;
            if is_valid(rsdt) && table_signature(rsdt) == SIG_RSDT {
                let old_rsdt_len = table_length(rsdt) as usize;
                let new_rsdt_len = old_rsdt_len + 4;
                let rsdt_pages = (new_rsdt_len + 0xFFF) / 0x1000;
                if let Ok(p) = boot::allocate_pages(
                    AllocateType::MaxAddress(0xFFFF_FFFF),
                    MemoryType::ACPI_RECLAIM,
                    rsdt_pages,
                ) {
                    let new_rsdt = p.as_ptr();
                    ptr::copy_nonoverlapping(rsdt, new_rsdt, old_rsdt_len);
                    wr_u32(new_rsdt, old_rsdt_len, bgrt as u32);
                    wr_u32(new_rsdt, 4, new_rsdt_len as u32);
                    fix_checksum(new_rsdt);
                    wr_u32(rsdp, 16, new_rsdt as u32);
                    acpi::fix_rsdp_checksums(rsdp);
                    log::write("[+] BGRT: RSDT extended with new BGRT entry.");
                }
            }
        }

        log::write("[+] BGRT: Successfully injected new ACPI BGRT table into XSDT.");
        Some(bgrt)
    }
}

fn draw_logo(
    gop: &mut GraphicsOutput,
    bmp: &Bmp,
    offset_x: u32,
    offset_y: u32,
    bgrt: Option<*mut u8>,
) {
    system::with_stdout(|out| {
        let _ = out.enable_cursor(false);
        let _ = out.clear();
    });

    if bmp.compression != 0 || (bmp.bit_count != 24 && bmp.bit_count != 32) {
        log::write("[-] BGRT: Only uncompressed 24/32-bit BMP is supported for GOP Blt.");
        return;
    }

    let width = bmp.width as usize;
    let height = bmp.height as usize;
    let bytes_per_pixel = (bmp.bit_count / 8) as usize;
    let row_size = (width * bytes_per_pixel + 3) / 4 * 4;
    let off_bits = bmp.off_bits as usize;
    if width == 0
        || height == 0
        || off_bits + row_size * height > LOGO.len()
    {
        log::write("[-] BGRT: Embedded BMP dimensions are invalid.");
        return;
    }

    let mut pixels: Vec<BltPixel> = Vec::new();
    if pixels.try_reserve_exact(width * height).is_err() {
        log::write("[-] BGRT: Failed to allocate blt buffer.");
        return;
    }
    pixels.resize(width * height, BltPixel::new(0, 0, 0));
    for row in 0..height {
        let src_row = off_bits + (height - 1 - row) * row_size;
        for col in 0..width {
            let p = src_row + col * bytes_per_pixel;
            pixels[row * width + col] = BltPixel::new(LOGO[p + 2], LOGO[p + 1], LOGO[p]);
        }
    }

    match gop.blt(BltOp::BufferToVideo {
        buffer: &pixels,
        src: BltRegion::Full,
        dest: (offset_x as usize, offset_y as usize),
        dims: (width, height),
    }) {
        Ok(()) => log::write("[+] BGRT: GOP Blt drawn successfully to screen."),
        Err(_) => {
            log::write("[-] BGRT: GOP Blt failed");
            if let Some(bgrt) = bgrt {
                unsafe {
                    acpi::wr_u8(bgrt, 38, 0);
                    fix_checksum(bgrt);
                }
            }
        }
    }
}

pub fn patch_and_draw(rsdp: *mut u8) {
    let Some(bmp) = Bmp::parse(LOGO) else {
        log::write("[-] BGRT: Invalid or missing embedded BMP data.");
        return;
    };

    log::write("[+] BGRT: Starting BGRT hijack and GOP display...");

    let Ok(handles) = boot::find_handles::<GraphicsOutput>() else {
        log::write("[-] BGRT: Locate GOP failed");
        return;
    };
    let Some(handle) = handles.first().copied() else {
        log::write("[-] BGRT: Locate GOP failed");
        return;
    };
    let Ok(mut gop) = boot::open_protocol_exclusive::<GraphicsOutput>(handle) else {
        log::write("[-] BGRT: Locate GOP failed");
        return;
    };
    let (screen_w, screen_h) = gop.current_mode_info().resolution();

    let offset_x = if (screen_w as u32) > bmp.width {
        (screen_w as u32 - bmp.width) / 2
    } else {
        0
    };
    let offset_y = if (screen_h as u32) > bmp.height {
        (screen_h as u32 - bmp.height) / 2
    } else {
        0
    };

    let pages = (LOGO.len() + 0xFFF) / 0x1000;
    let bmp_addr = match boot::allocate_pages(
        AllocateType::MaxAddress(0xFFFF_FFFF),
        MemoryType::ACPI_RECLAIM,
        pages,
    ) {
        Ok(p) => p.as_ptr(),
        Err(_) => {
            log::write("[-] BGRT: Failed to allocate ACPI Reclaim memory");
            return;
        }
    };

    unsafe {
        ptr::copy_nonoverlapping(LOGO.as_ptr(), bmp_addr, LOGO.len());
    }

    let bgrt = create_or_update_bgrt(rsdp, bmp_addr as u64, offset_x, offset_y);
    if bgrt.is_none() {
        log::write("[-] BGRT: CreateOrUpdateBgrt failed");
    }

    draw_logo(&mut gop, &bmp, offset_x, offset_y, bgrt);
}
