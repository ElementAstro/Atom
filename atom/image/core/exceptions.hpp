/*
 * exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-24

Description: Backward compatibility header - redirects to unified exceptions

**************************************************/

#pragma once

// This header is deprecated. Use atom/image/exceptions.hpp instead.
// Kept for backward compatibility only.

#include "../exceptions.hpp"

// All exception types are now available through atom::image namespace
// and atom::image::core namespace (for backward compatibility)
