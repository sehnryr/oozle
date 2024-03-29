fn main() {
    cxx_build::bridge("src/lib.rs")
        .file("src/cpp/lzna.cpp")
        .file("src/cpp/bitknit.cpp")
        .file("src/cpp/bitreader.cpp")
        .file("src/cpp/tans.cpp")
        .file("src/cpp/huffman.cpp")
        .file("src/cpp/kraken.cpp")
        .file("src/cpp/mermaid.cpp")
        .file("src/cpp/leviathan.cpp")
        .file("src/cpp/decompress.cpp")

        // MSVC flags
        .flag_if_supported("/std:c++latest")

        // GCC flags to suppress warnings
        .flag_if_supported("-Wno-conversion-null")
        .flag_if_supported("-Wno-sequence-point")
        .flag_if_supported("-Wno-sign-compare")
        .flag_if_supported("-Wno-shift-negative-value")
        .flag_if_supported("-Wno-unused-variable")
        .flag_if_supported("-Wno-unused-parameter")

        .compile("oozle");

    println!("cargo:rerun-if-changed=src/lib.rs");

    println!("cargo:rerun-if-changed=src/cpp/lzna.cpp");
    println!("cargo:rerun-if-changed=src/cpp/bitknit.cpp");
    println!("cargo:rerun-if-changed=src/cpp/bitreader.cpp");
    println!("cargo:rerun-if-changed=src/cpp/tans.cpp");
    println!("cargo:rerun-if-changed=src/cpp/huffman.cpp");
    println!("cargo:rerun-if-changed=src/cpp/kraken.cpp");
    println!("cargo:rerun-if-changed=src/cpp/mermaid.cpp");
    println!("cargo:rerun-if-changed=src/cpp/leviathan.cpp");
    println!("cargo:rerun-if-changed=src/cpp/decompress.cpp");

    println!("cargo:rerun-if-changed=include/stdafx.h");
    println!("cargo:rerun-if-changed=include/lzna.h");
    println!("cargo:rerun-if-changed=include/bitknit.h");
    println!("cargo:rerun-if-changed=include/bitreader.h");
    println!("cargo:rerun-if-changed=include/tans.h");
    println!("cargo:rerun-if-changed=include/huffman.h");
    println!("cargo:rerun-if-changed=include/kraken.h");
    println!("cargo:rerun-if-changed=include/mermaid.h");
    println!("cargo:rerun-if-changed=include/leviathan.h");
    println!("cargo:rerun-if-changed=include/decompress.h");
}
