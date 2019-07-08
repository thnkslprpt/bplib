/************************************************************************
 * File: bplib_crc.h
 *
 *  Copyright 2019 United States Government as represented by the
 *  Administrator of the National Aeronautics and Space Administration.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * Maintainer(s):
 *  Joe-Paul Swinski, Code 582 NASA GSFC
 *
 *************************************************************************/

#ifndef _BPLIB_CRC_H_
#define _BPLIB_CRC_H_

/******************************************************************************
 INCLUDES
 ******************************************************************************/

#include "bplib_os.h"

/******************************************************************************
 DEFINES
 ******************************************************************************/

// Number of different possible bytes.
#define BYTE_COMBOS 256

/******************************************************************************
 TYPEDEFS
 ******************************************************************************/

// Standard parameters for calculating a CRC.
typedef struct crc_parameters
{
    char* name;                     // Name of the CRC.
    int length;                     // The number of bits in the CRC.
    uint64_t generator_polynomial;  // The generator polynomial used to compute the CRC.
    uint64_t initial_value;         // The value used to initialize a CRC.
    bool should_reflect_input;      // Whether to reflect the bits of the input bytes.
    bool should_reflect_output;     // Whether to reflect the bits of the output crc.
    uint64_t final_xor;             // The final value to xor with the crc before returning.
    uint16_t check_value;           // The crc value resulting from the input string "123456789" used to check validity of the implementation.
    uint16_t table[BYTE_COMBOS];    // A ptr to a lookup table containing the precomputed XOR values of each byte with the generator polynomial.
} crc_parameters;
 
/******************************************************************************
 PROTOTYPES
 ******************************************************************************/

// Creates a crc16_table containing a lookup table and generator polynomial.
void populate_crc16_table(struct crc_parameters* params);
// Calulcates a CRC value for an array of bytes using a set of parameters and 
// a corresponding crc16 lookup table.
uint16_t calculate_crc16(uint8_t* data, int length, struct crc_parameters* params);
// Checks that a crc parameters properly computes its check value.
bool validate_crc_parameters(struct crc_parameters* params);
#endif /* _BPLIB_CRC_H_ */
