/// # oozle
/// 
/// A Rust library for decompressing Kraken, Mermaid, Selkie, Leviathan,
/// LZNA and Bitknit compressed buffers.

#[cxx::bridge]
mod ffi {
    // C++ types and signatures exposed to Rust.
    unsafe extern "C++" {
        include!("oozle/include/kraken.h");

        unsafe fn Kraken_Decompress(
            src: *const u8,
            src_len: usize,
            dst: *mut u8,
            dst_len: usize,
        ) -> i32;
    }
}

/// Decompresses a Kraken, Mermaid, Selkie, Leviathan, LZNA or Bitknit
/// compressed buffer.
/// 
/// # Arguments
/// 
/// * `src` - The compressed buffer.
/// * `dst` - The decompressed buffer.
/// 
/// # Returns
/// 
/// * `Ok(len)` - The length of the decompressed buffer.
/// * `Err(())` - The decompression failed.
pub unsafe fn decompress(src: &[u8], dst: &mut [u8]) -> Result<usize, ()> {
    let len = ffi::Kraken_Decompress(
        src.as_ptr(), 
        src.len(), 
        dst.as_mut_ptr(), 
        dst.len(),
    );

    if len < 0 {
        Err(())
    } else {
        Ok(len as usize)
    }
}
