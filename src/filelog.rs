use alloc::format;
use core::cell::UnsafeCell;

use log::{Level, Log};
use uefi::boot;
use uefi::cstr16;
use uefi::proto::media::file::{Directory, File, FileAttribute, FileHandle, FileMode, RegularFile};
use uefi::CStr16;

const LOG_PATH: &CStr16 = cstr16!(r"\EFI\BOOT\unlock.log");
const FLUSH_EVERY_LINES: usize = 16;
const FLUSH_MARKERS: &[&str] = &[
    "PHASE",
    "RESULT",
    "POST-",
    "FINAL",
    "RECONNECT",
    "DETACH",
    "CHAIN",
    "SELECTOR",
    "HANDOFF",
    "WATCHDOG",
    "AUTH-SCAN end",
    "=== ",
];

struct FileLogger {
    file: UnsafeCell<Option<RegularFile>>,
    lines: UnsafeCell<usize>,
}

unsafe impl Sync for FileLogger {}
unsafe impl Send for FileLogger {}

static LOGGER: FileLogger = FileLogger {
    file: UnsafeCell::new(None),
    lines: UnsafeCell::new(0),
};

impl Log for FileLogger {
    fn enabled(&self, metadata: &log::Metadata) -> bool {
        metadata.level() <= Level::Info
    }

    fn log(&self, record: &log::Record) {
        if !self.enabled(record.metadata()) {
            return;
        }
        append(&format!(
            "[{:5}] {}: {}\r\n",
            record.level(),
            record.target(),
            record.args()
        ));
    }

    fn flush(&self) {}
}

fn append(line: &str) {
    unsafe {
        let n = *LOGGER.lines.get() + 1;
        *LOGGER.lines.get() = n;
        let flush = n % FLUSH_EVERY_LINES == 0 || FLUSH_MARKERS.iter().any(|m| line.contains(m));
        if let Some(file) = (*LOGGER.file.get()).as_mut() {
            let ok = file.write(line.as_bytes()).is_ok() && (!flush || file.flush().is_ok());
            if !ok {
                *LOGGER.file.get() = None;
            }
        }
    }
}

pub fn write(message: &str) {
    append(&format!("{message}\r\n"));
}

pub fn init() {
    let file = open_fresh_log();
    unsafe {
        *LOGGER.file.get() = file;
    }
    let _ = log::set_logger(&LOGGER);
    log::set_max_level(log::LevelFilter::Info);
}

pub fn finish() {
    unsafe {
        let taken = (*LOGGER.file.get()).take();
        drop(taken);
    }
}

fn open_fresh_log() -> Option<RegularFile> {
    let mut fs = boot::get_image_file_system(boot::image_handle()).ok()?;
    let mut root: Directory = fs.open_volume().ok()?;

    if let Ok(handle) = root.open(LOG_PATH, FileMode::ReadWrite, FileAttribute::empty()) {
        if let Some(reg) = handle.into_regular_file() {
            let _ = reg.delete();
        }
    }

    let handle: FileHandle = root
        .open(
            LOG_PATH,
            FileMode::CreateReadWrite,
            FileAttribute::empty(),
        )
        .ok()?;
    handle.into_regular_file()
}
