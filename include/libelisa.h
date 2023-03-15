/*
 *
 * Copyright 2023 Kenichi Yasukata
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _LIBELISA_H_
#define _LIBELISA_H_

#include <stdint.h>

/*
 * configurable memory type
 * 0: UC
 * 1: WC
 * 4: WT
 * 5: WP
 * 6: WB
 */
#define EPT_MEMORY_TYPE (6UL)

#define EPT_R (1UL << 0) // read
#define EPT_W (1UL << 1) // write
#define EPT_X (1UL << 2) // exec
#define EPT_MT_SHIFT (3) // memory type 3-5
#define EPT_MT (EPT_MEMORY_TYPE << EPT_MT_SHIFT) // memory type
#define EPT_A (1UL << 8) // accessed
#define EPT_U (1UL << 10) // user mode exec
#define EPT_ADDR_MASK (((1UL << 52) - 1) & ~((1UL << 12) - 1)) // 12 to N-1

#define EPT__I (1UL << 6) // ignore pat memory type
#define EPT__PS (1UL << 7) // if 1, huge page
#define EPT__A (1UL << 8) // accessed (if EPT__I is 1)
#define EPT__D (1UL << 9) // dirty (if EPT__I is 1)

#define CR3_ADDR_MASK (((1UL << 63) - 1) & ~((1UL << 12) - 1)) // 12 to M-1

#define PT_P (1UL << 0) // present
#define PT_W (1UL << 1) // writable
#define PT_U (1UL << 2) // user-mode exec
#define PT_WT (1UL << 3) // write-through
#define PT_CD (1UL << 4) // cache disable
#define PT_PS (1UL << 7) // if 1, huge page
//#define PT_ADDR_MASK EPT_ADDR_MASK // same as ept
#define PT_NX (1UL << 63) // non-executable

#define ELISA_MAP_REQ_FLAGS_SRC_GPA (1UL << 1)

struct elisa_map_req {
	uint64_t flags;
	uint64_t src_gxa;
	uint64_t dst_gva;
	uint64_t dst_gpa;
	uint64_t pt_flags;
	uint64_t ept_flags;
	int level;
};

int elisa_client(const char *, const int, int (*)(int));
void elisa_server(const int,
		  int (*)(int, uint64_t *, struct elisa_map_req **, int *, uint64_t *),
		  int(*)(uint64_t *));

long elisa_gate_entry(long, ...);

void vmcall_cli(void);
void vmcall_sti(void);
long vmcall_rflags(void);

#endif
