use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let version = fs::read_to_string(manifest.join("version.txt"))
        .ok()
        .and_then(|s| {
            s.lines()
                .find_map(|l| l.strip_prefix("VERSION_STRING=").map(str::to_owned))
        })
        .unwrap_or_else(|| "dev".to_string());
    println!("cargo:rustc-env=BUILD_VERSION={version}");
    println!("cargo:rerun-if-changed=version.txt");
}
