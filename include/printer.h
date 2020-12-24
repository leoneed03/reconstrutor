//
// Copyright (c) Leonid Seniukov. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef GLOBALRECONSTRUCTIONRGBD_PRINTER_H
#define GLOBALRECONSTRUCTIONRGBD_PRINTER_H

#include <ostream>

#ifdef DEBUG_PRINT_PROGRESS
#define PRINT_PROGRESS(str) do { std::cout << str << std::endl; } while (false)
#define APPEND_PROGRESS(str) do { std::cout << str; } while (false)
#else
#define PRINT_PROGRESS(str) do { } while (false)
#define APPEND_PROGRESS(str) do { } while (false)
#endif

#endif