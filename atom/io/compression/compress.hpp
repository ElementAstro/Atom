/*
 * compress.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Umbrella header for compression module.
             Includes all compression sub-module headers for backward
             compatibility.

**************************************************/

#ifndef ATOM_IO_COMPRESSION_COMPRESS_HPP
#define ATOM_IO_COMPRESSION_COMPRESS_HPP

// Common types: CompressionResult, CompressionOptions, DecompressionOptions,
//               ZipFileInfo, concepts, type aliases
#include "types.hpp"

// GZ file compression/decompression: compressFile, decompressFile
#include "gz_compress.hpp"

// ZIP operations: compressFolder, extractZip, createZip, listZipContents,
//                 fileExistsInZip, removeFromZip, getZipSize
#include "zip_operations.hpp"

// Slice-based compression: compressFileInSlices, mergeCompressedSlices
#include "slice_compress.hpp"

// Template data compression: compressData<T>, decompressData<T>
#include "data_compress.hpp"

// Backup/restore and async processing: createBackup, restoreFromBackup,
//                                      processFilesAsync
#include "backup.hpp"

#endif  // ATOM_IO_COMPRESSION_COMPRESS_HPP
