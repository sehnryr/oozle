fn main() {
    cxx_build::bridge("src/lib.rs")
        .file("src/cpp/lzna.cpp")
        .file("src/cpp/bitknit.cpp")
        .file("src/cpp/mermaid.cpp")
        .file("src/cpp/leviathan.cpp")
        .file("src/cpp/decompressor.cpp")
        .compile("oozle");

    println!("cargo:rerun-if-changed=src/lib.rs");

    println!("cargo:rerun-if-changed=src/cpp/lzna.cpp");
    println!("cargo:rerun-if-changed=src/cpp/bitknit.cpp");
    println!("cargo:rerun-if-changed=src/cpp/mermaid.cpp");
    println!("cargo:rerun-if-changed=src/cpp/leviathan.cpp");
    println!("cargo:rerun-if-changed=src/cpp/decompressor.cpp");

    println!("cargo:rerun-if-changed=include/stdafx.h");
    println!("cargo:rerun-if-changed=include/lzna.h");
    println!("cargo:rerun-if-changed=include/bitknit.h");
    println!("cargo:rerun-if-changed=include/mermaid.h");
    println!("cargo:rerun-if-changed=include/leviathan.h");
    println!("cargo:rerun-if-changed=include/decompressor.h");
}
