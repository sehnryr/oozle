use crate as oozle;

#[test]
fn test_decompress() {
    let compressed = include_bytes!("../test_data/compressed");
    let decompressed_size = u32::from_le_bytes(compressed[..4].try_into().unwrap()) as usize;
    let mut decompressed = vec![0; decompressed_size];

    let result = unsafe {
        oozle::decompress(&compressed[4..], &mut decompressed)
            .unwrap_or_else(|_| panic!("decompression failed"))
    };

    // Make sure the decompressed size matches the expected size.
    assert_eq!(decompressed_size, result);

    // Compare the decompressed data to the expected output.
    let expected = include_bytes!("../test_data/decompressed");
    assert_eq!(decompressed, expected);
}
