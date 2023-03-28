// TODO: Document this function
pub fn copy_whole_match(output: &mut [u8], output_offset: usize, match_distance: usize) {
    let source_offset: usize = output_offset - match_distance;
    let output_len: usize = output.len() - output_offset;
    let mut i: usize = 0;

    if output_offset >= 8 {
        while i + 8 <= output_len {
            output[output_offset + i] = output[source_offset + i];
            i += 8;
        }
    }

    while i < output_len {
        output[output_offset + i] = output[source_offset + i];
        i += 1;
    }
}

// TODO: Document this function
pub fn get_crc(_input: &[u8]) -> u32 {
    // TODO: Implement this function

    return 0;
}
