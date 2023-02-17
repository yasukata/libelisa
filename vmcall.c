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

/* vmcall entry point */

void ____asm_impl_vmcall(void)
{
	asm volatile(
	".globl do_vmcall \n\t"
	"do_vmcall: \n\t"
	"endbr64 \n\t"
	"push %rbp \n\t"
	"movq %rsp, %rbp \n\t"

	"push %rbx \n\t"
	"push %rcx \n\t"
	"push %rdx \n\t"
	"push %rsi \n\t"

	/* adjust calling convention */
	"push %r8 \n\t"  // a3
	"push %rcx \n\t" // a2
	"push %rdx \n\t" // a1
	"push %rsi \n\t" // a0
	"push %rdi \n\t" // nr
	"pop %rax \n\t" // rax: nr
	"pop %rbx \n\t" // rbx: a0
	"pop %rcx \n\t" // rcx: a1
	"pop %rdx \n\t" // rdx: a2
	"pop %rsi \n\t" // rsi: a3

	"vmcall \n\t"

	"pop %rsi \n\t"
	"pop %rdx \n\t"
	"pop %rcx \n\t"
	"pop %rbx \n\t"

	"leaveq \n\t"
	"retq \n\t"
	);
}

extern long do_vmcall(long, ...);

/* utilities for EPT setup in manager */

#define KVM_HC_ELISA_UTIL (1000)

enum {
	HC_ELISA_UTIL_DEBUG = 1,
	HC_ELISA_UTIL_VMCS,
	HC_ELISA_UTIL_ADDR,
};

enum {
	HC_ELISA_UTIL_VMCS_READ64 = 1,
	HC_ELISA_UTIL_VMCS_WRITE64,
};

enum {
	HC_ELISA_UTIL_ADDR_GPA_TO_HPA = 1,
};

static inline int gpa2hpa(uint64_t gpa, uint64_t *hpa)
{
	assert((*hpa = do_vmcall(KVM_HC_ELISA_UTIL,
				 HC_ELISA_UTIL_ADDR,
				 HC_ELISA_UTIL_ADDR_GPA_TO_HPA,
				 gpa)) != (uint64_t) -1);
	return 0;
}

static inline int gva2gpa(uint64_t va, uint64_t *pa)
{
	uint64_t p;
	{
		int fd;
		assert((fd = open("/proc/self/pagemap", O_RDONLY)) != -1);
		assert(sizeof(p) == pread(fd, &p, sizeof(p), (va >> 12) * sizeof(p)));
		close(fd);
	}
	assert((p & 0x8000000000000000) != 0); // ensure mapping exists
	assert((p & 0x4000000000000000) == 0); // ensure not swapped
	*pa = ((p & 0x7fffffffffffff) << 12); // addr
	return 0;
}

static inline int gva2hpa(uint64_t gva, uint64_t *hpa)
{
	uint64_t gpa;
	assert(!gva2gpa(gva, &gpa));
	assert(!gpa2hpa(gpa, hpa));
	return 0;
}

/* negotiation between manager and guest */

#define KVM_HC_ELISA_NEGOTIATION (1001)

enum {
	HC_ELISA_NEGOTIATION_DEBUG = 1,
	HC_ELISA_NEGOTIATION_MANAGER,
	HC_ELISA_NEGOTIATION_GUEST,
};

enum {
	HC_ELISA_NEGOTIATION_MANAGER_RENDEZVOUS_POINT_VCPU_ID = 1,
	HC_ELISA_NEGOTIATION_MANAGER_RENDEZVOUS_POINT_EPTP_LIST_HPA,
};

enum {
	HC_ELISA_NEGOTIATION_GUEST_VCPU_ID = 1,
	HC_ELISA_NEGOTIATION_GUEST_ACTIVATE,
};

static inline void set_rendezvous_point_vcpu_id(uint64_t vcpu_id)
{
	assert((uint64_t) (do_vmcall(KVM_HC_ELISA_NEGOTIATION,
				     HC_ELISA_NEGOTIATION_MANAGER,
				     HC_ELISA_NEGOTIATION_MANAGER_RENDEZVOUS_POINT_VCPU_ID,
				     vcpu_id)) != (uint64_t) -1);
}

static inline void set_rendezvous_point_eptp_list_hpa(uint64_t hpa)
{
	assert((uint64_t) (do_vmcall(KVM_HC_ELISA_NEGOTIATION,
				     HC_ELISA_NEGOTIATION_MANAGER,
				     HC_ELISA_NEGOTIATION_MANAGER_RENDEZVOUS_POINT_EPTP_LIST_HPA,
				     hpa)) != (uint64_t) -1);
}

static inline uint64_t get_guest_vcpu_id(void)
{
	uint64_t vcpu_id;
	assert((vcpu_id = do_vmcall(KVM_HC_ELISA_NEGOTIATION,
				    HC_ELISA_NEGOTIATION_GUEST,
				    HC_ELISA_NEGOTIATION_GUEST_VCPU_ID)) != (uint64_t) -1);
	return vcpu_id;
}

static inline void elisa_guest_activate(void)
{
	assert((uint64_t) (do_vmcall(KVM_HC_ELISA_NEGOTIATION,
				     HC_ELISA_NEGOTIATION_GUEST,
				     HC_ELISA_NEGOTIATION_GUEST_ACTIVATE)) != (uint64_t) -1);
}

/* for interrupt setting */

static inline uint64_t rvmcs64(uint64_t field)
{
	uint64_t val;
	assert((val = do_vmcall(KVM_HC_ELISA_UTIL,
				HC_ELISA_UTIL_VMCS,
				HC_ELISA_UTIL_VMCS_READ64,
				field)) != (uint64_t) -1);
	return val;
}

static inline uint64_t wvmcs64(uint64_t field, uint64_t val)
{
	assert(((uint64_t) do_vmcall(KVM_HC_ELISA_UTIL,
				     HC_ELISA_UTIL_VMCS,
				     HC_ELISA_UTIL_VMCS_WRITE64,
				     field, val)) != (uint64_t) -1);
	return 0;
}

void vmcall_cli(void)
{
	wvmcs64(0x00006820 /* GUEST_RFLAGS */, rvmcs64(0x00006820 /* GUEST_RFLAGS */) & (~(1UL << 9)) /* InterruptFlag */);
}

void vmcall_sti(void)
{
	wvmcs64(0x00006820 /* GUEST_RFLAGS */, rvmcs64(0x00006820 /* GUEST_RFLAGS */) | (1UL << 9) /* InterruptFlag */);
}

long vmcall_rflags(void)
{
	return rvmcs64(0x00006820 /* GUEST_RFLAGS */);
}
