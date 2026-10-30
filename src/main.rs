#![no_std]
#![no_main]

extern crate alloc;

mod acpi;
mod booticon;
mod filelog;
#[cfg(feature = "tx")]
mod nv;
#[cfg(feature = "tx")]
#[allow(dead_code)]
mod pci;
#[cfg(feature = "tx")]
mod tx;
#[cfg(feature = "tx")]
mod tx_booter;
#[cfg(feature = "tx")]
mod tx_detach;
#[cfg(feature = "tx")]
mod tx_fwsec;
#[cfg(feature = "tx")]
mod tx_portal;

use alloc::vec::Vec;
use core::time::Duration;

use uefi::boot::{self, LoadImageSource};
use uefi::prelude::*;
use uefi::proto::device_path::build::{self, DevicePathBuilder};
use uefi::proto::device_path::DevicePath;
use uefi::proto::loaded_image::LoadedImage;
use uefi::proto::BootPolicy;
use uefi::{CStr16, CString16};

const CANDIDATES: [&str; 2] = [
    "\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
    "EFI\\Microsoft\\Boot\\bootmgfw.efi",
];

#[entry]
fn main() -> Status {
    filelog::init();
    filelog::write("=== BOOTLOADER STARTED ===");
    filelog::write(concat!("[I] Build: ", env!("BUILD_VERSION")));

    match boot::set_watchdog_timer(0, 0x10000, None) {
        Ok(()) => filelog::write("[I] WATCHDOG: disabled"),
        Err(_) => filelog::write("[-] WATCHDOG: disable failed"),
    }

    acpi::override_from_esp();

    match acpi::find_rsdp() {
        Some(rsdp) => booticon::patch_and_draw(rsdp),
        None => filelog::write("[-] BGRT: RSDP not found, skip boot logo patch"),
    }

    #[cfg(feature = "tx")]
    nv::unlock();

    filelog::write("[+] Chainloading Windows Boot Manager from ESP...");
    let status = match chainload() {
        Ok(()) => Status::SUCCESS,
        Err(status) => {
            filelog::write("[-] Error: Failed to load Windows Boot Manager.");
            boot::stall(Duration::from_secs(10));
            status
        }
    };
    filelog::finish();
    status
}

fn chainload() -> Result<(), Status> {
    let image = boot::image_handle();

    let device = {
        let loaded =
            boot::open_protocol_exclusive::<LoadedImage>(image).map_err(|e| e.status())?;
        loaded.device().ok_or(Status::NOT_FOUND)?
    };

    let device_path = {
        let path = boot::open_protocol_exclusive::<DevicePath>(device).map_err(|e| e.status())?;
        path.to_boxed()
    };

    for candidate in CANDIDATES {
        filelog::write("--------------------------------------");
        filelog::write(candidate);

        let Ok(file_path) = CString16::try_from(candidate) else {
            continue;
        };

        let mut buffer: Vec<u8> = Vec::new();
        let Some(full_path) = build_full_path(&mut buffer, &device_path, file_path.as_ref()) else {
            continue;
        };

        match boot::load_image(
            image,
            LoadImageSource::FromDevicePath {
                device_path: full_path,
                boot_policy: BootPolicy::ExactMatch,
            },
        ) {
            Ok(handle) => {
                filelog::write("[+] LoadImage succeeded!");
                return boot::start_image(handle).map_err(|e| e.status());
            }
            Err(_) => filelog::write("[D] LoadImage failed"),
        }
    }

    Err(Status::NOT_FOUND)
}

fn build_full_path<'a>(
    buffer: &'a mut Vec<u8>,
    device_path: &DevicePath,
    file_name: &CStr16,
) -> Option<&'a DevicePath> {
    let mut builder = DevicePathBuilder::with_vec(buffer);
    for node in device_path.node_iter() {
        builder = builder.push(&node).ok()?;
    }
    builder
        .push(&build::media::FilePath { path_name: file_name })
        .ok()?
        .finalize()
        .ok()
}
