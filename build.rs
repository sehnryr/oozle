fn main() {
    cxx_build::bridge("src/lib.rs")
        .file("src/cpp/lzna.cpp")
        .file("src/cpp/bitknit.cpp")
        .file("src/cpp/kraken.cpp")
        .compile("oozle");

    println!("cargo:rerun-if-changed=src/lib.rs");

    println!("cargo:rerun-if-changed=src/cpp/lzna.cpp");
    println!("cargo:rerun-if-changed=src/cpp/bitknit.cpp");
    println!("cargo:rerun-if-changed=src/cpp/kraken.cpp");

    println!("cargo:rerun-if-changed=include/stdafx.h");
    println!("cargo:rerun-if-changed=include/lzna.h");
    println!("cargo:rerun-if-changed=include/bitknit.h");
    println!("cargo:rerun-if-changed=include/kraken.h");
}
