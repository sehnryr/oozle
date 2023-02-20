fn main() {
    cxx_build::bridge("src/lib.rs")
        .file("src/lzna.cpp")
        .file("src/bitknit.cpp")
        .file("src/kraken.cpp")
        .compile("oozle");

    println!("cargo:rerun-if-changed=src/lib.rs");

    println!("cargo:rerun-if-changed=src/lzna.cpp");
    println!("cargo:rerun-if-changed=src/bitknit.cpp");
    println!("cargo:rerun-if-changed=src/kraken.cpp");

    println!("cargo:rerun-if-changed=include/stdafx.h");
    println!("cargo:rerun-if-changed=include/lzna.h");
    println!("cargo:rerun-if-changed=include/kraken.h");
}
