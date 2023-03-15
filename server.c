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

#define MMU_ROOT_LEVEL (4) /* TODO: fetch from hypervisor  */
#define MMU_SHADOW_ROOT_LEVEL (4) /* TODO: fetch from hypervisor  */

#define BASE_GPA (1UL << 38) // 256GB

/* NOTE: stack sizes are embedded as string in inline assembly */
#define GATE_STACK_SIZE 0x1000
#define SUB_STACK_SIZE 0x800000
#define __S(__s) #__s
#define _S(_s) __S(_s)

void ____asm_impl_server(void)
{
	asm volatile (
/*
 * --------------------------------------
 * the part below is for gate context
 */

	/* here will come just after gate_entry_end */

	/* -- 4K align --*/

	".align 0x1000 \n\t"
	".globl gate_ctx_page \n\t"
	"gate_ctx_page: \n\t"

	"enter_sub: \n\t"

	/* set sub eptp to eptp_list */
	".globl __RUNTIME_VALUE_0 \n\t"
	"__RUNTIME_VALUE_0: \n\t"
	"movabs $0x123456789abcdef0, %rax \n\t" // 0x123456789abcdef0 (sub eptp) will be replaced at initialization phase
	/* potentially guest can enter here */
	"movq %rax, __eptp_list_map_gate+0x8(%rip) \n\t"

	/* check invalid attempt */
	".globl __RUNTIME_VALUE_1 \n\t"
	"__RUNTIME_VALUE_1: \n\t"
	"movabs $0x123456789abcdef0, %rax \n\t" // 0x123456789abcdef0 (sub eptp) will be replaced at initialization
	"cmp %rax, __eptp_list_map_gate+0x8(%rip) \n\t"
	"jne invalid_attempt_detected \n\t"

	/* ensure interrupt is disabled */
	"movq %rsp, %rcx \n\t" // save rsp
	"lea __stack_in_gate_end+" _S(GATE_STACK_SIZE) "(%rip), %rsp \n\t"
	"pushfq \n\t"
	"popq %rax \n\t"
	"movq %rcx, %rsp \n\t" // restore rsp
	"andq $0x200, %rax \n\t" // $0x200 : (1 << 9) is interrupt flag
	"test %rax, %rax \n\t"
	"jne invalid_attempt_detected \n\t"

	/* execute vmfunc */
	"movq $0x0, %rax \n\t"
	"movq $0x1, %rcx \n\t"
	"vmfunc \n\t"

	"enter_sub_end: \n\t"

	".space 0x1000 - (enter_sub_end - enter_sub) - (invalid_attempt_detected_end - invalid_attempt_detected) \n\t"

	"invalid_attempt_detected: \n\t"
	"movq $-0x1, %rbx \n\t"
	"movq $0x0, %rax \n\t"
	"movq %rax, __eptp_list_map_gate+0x8(%rip) \n\t"
	"movq $0x0, %rax \n\t"
	"cmp %rax, __eptp_list_map_gate+0x8(%rip) \n\t"
	"je __check_ok \n\t"
	"int3 \n\t"
	"__check_ok: \n\t"
	"movq $0x0, %rcx \n\t"
	"vmfunc \n\t"
	"invalid_attempt_detected_end: \n\t"

	/* -- 4K align --*/
	/* just for referencing from the code */
	"__eptp_list_map_gate: \n\t" // eptp_list page will be mapped
	".space 0x1000 \n\t"

	/* -- 4K align --*/
	"__stack_in_gate_end: \n\t"

/*
 * up to here is for gate context
 * --------------------------------------
 * --------------------------------------
 * the part below is for sub context
 */

	".globl sub_ctx_page \n\t"
	"sub_ctx_page: \n\t"

	".space (enter_sub_end - enter_sub) \n\t"

	/* entry to this sub ctx */

	"movq $0x0, %rax \n\t"
	"movq %rax, __eptp_list_map_sub+0x8(%rip) \n\t"

	"movq %rsp, %rax \n\t"
	"lea __stack_in_sub_end+" _S(SUB_STACK_SIZE) "(%rip), %rsp \n\t"

	"push %rax \n\t" // save stack ptr

	"push %rbp \n\t"
	"movq %rsp, %rbp \n\t"

	".globl __RUNTIME_VALUE_2 \n\t"
	"__RUNTIME_VALUE_2: \n\t"
	"movabs $0x123456789abcdef0, %rax \n\t" // 0x123456789abcdef0 (pointer to entry point function) is replaced at initialization
	"call *%rax \n\t"
	"movq %rax, %rbx \n\t" // save return value on rbx

	"leaveq \n\t"

	"pop %rax \n\t"
	"movq %rax, %rsp \n\t"

	"jmp enter_default \n\t"

	"work_in_sub_done: \n\t"

	".space 0x1000 - (enter_default_end - enter_default) - (work_in_sub_done - sub_ctx_page) \n\t"

	"enter_default: \n\t"
	"movq $0x0, %rax \n\t"
	"movq $0x0, %rcx \n\t"
	"vmfunc \n\t"
	"enter_default_end: \n\t"

	/* -- 4K align --*/
	/* just for referencing from the code */
	"__eptp_list_map_sub: \n\t" // eptp_list page will be mapped
	".space 0x1000 \n\t"

	/* -- 4K align --*/
	"__stack_in_sub_end: \n\t"
	);
}

/*
 * 48 b8 f0 de bc 9a 78 56 34 12 : movabs $0x123456789abcdef0,%rax
 */

extern void __RUNTIME_VALUE_0(void);
extern void __RUNTIME_VALUE_1(void);
extern void __RUNTIME_VALUE_2(void);

extern void gate_ctx_page(void);
extern void sub_ctx_page(void);

static void embed_addr_to_code(void *code_mem, uint64_t addr)
{
	assert(!mprotect((void *)((uint64_t) code_mem & (~(0x1000 - 1))), 0x1000, PROT_WRITE));
	*((uint64_t *) code_mem) = addr;
}

struct pg_tbl {
	size_t cnt;
	size_t max;
	bool is_ept;
	int root_level;
	struct {
		uint64_t va;
		uint64_t pa;
	} *pg;
};

static int pa_to_va(struct pg_tbl *pgtbl, uint64_t pa, uint64_t *va)
{
	size_t i;
	for (i = 0; i < pgtbl->cnt; i++) {
		if (pa == pgtbl->pg[i].pa) {
			*va = pgtbl->pg[i].va;
			return 0;
		}
	}
	return -1;
}

static void free_pt(struct pg_tbl *pgtbl)
{
	size_t i;
	for (i = 0; i < pgtbl->cnt; i++)
		munmap((void *) pgtbl->pg[i].va, 0x1000);
	free(pgtbl->pg);
}

static void pt_map(struct pg_tbl *pgtbl, uint64_t va, uint64_t pa,
		   uint64_t flags, int target_level)
{
#define _IDX(_va, _level) ((_va >> (12 + (_level - 1) * 9)) & ((1 << 9) - 1))
	int i;
	uint64_t *pt;

	assert(!(va % (1UL << (12 + 9 * (target_level - 1)))));
	assert(!(pa % (1UL << (12 + 9 * (target_level - 1)))));

	for (i = pgtbl->root_level, pt = (uint64_t *)(pgtbl->pg ? pgtbl->pg[0].va : 0); i > 0; i--) {
		if (!pt)
			goto alloc_root_pt;

		if ((pt[_IDX(va, i)] & EPT__PS) && (i != target_level)) {
			printf("we need to split page table, not implemented yet\n");
			exit(1);
		}

		if (i == target_level) {
			if (pt[_IDX(va, i)])
				printf("overwrite 0x%lx for 0x%lx (%lx)\n",
						pt[_IDX(va, i)], pa,
						flags | (target_level != 1 ? EPT__PS /* same as PT_PS */ : 0) | pa);
			pt[_IDX(va, i)] = flags | (target_level != 1 ? EPT__PS /* same as PT_PS */ : 0) | pa;
			break;
		} else {
			bool has_entry;
			if (pgtbl->is_ept)
				has_entry = ((pt[_IDX(va, i)] & EPT_ADDR_MASK) != 0);
			else
				has_entry = ((pt[_IDX(va, i)] & PT_P) != 0);
			if (has_entry) {
				uint64_t _pt_va, _pt_pa = pt[_IDX(va, i)] & EPT_ADDR_MASK;
				assert(!pa_to_va(pgtbl, _pt_pa, &_pt_va));
				pt = (uint64_t *) _pt_va;
			} else {
alloc_root_pt:
				if (pgtbl->cnt == pgtbl->max) {
					if (!pgtbl->max)
						pgtbl->max = 128;
					else
						pgtbl->max *= 2;
					assert((pgtbl->pg = realloc(pgtbl->pg, pgtbl->max * sizeof(*(pgtbl->pg)))) != NULL);
				}

				assert((void *)(pgtbl->pg[pgtbl->cnt].va = (uint64_t) mmap(NULL,
											0x1000,
											PROT_READ | PROT_WRITE,
											MAP_ANONYMOUS | MAP_PRIVATE |
											MAP_LOCKED | MAP_POPULATE,
											-1, 0)) != MAP_FAILED);
				assert(!gva2gpa(pgtbl->pg[pgtbl->cnt].va, &pgtbl->pg[pgtbl->cnt].pa));

				if (pgtbl->is_ept)
					assert(!gpa2hpa(pgtbl->pg[pgtbl->cnt].pa, &pgtbl->pg[pgtbl->cnt].pa));

				if (!pt) {
					assert(!pgtbl->cnt);
					i++;
					goto again;
				}

				pt[_IDX(va, i)] = (pgtbl->is_ept ? (EPT_R | EPT_W | EPT_X | EPT_U) : (PT_P | PT_W | PT_U)) | pgtbl->pg[pgtbl->cnt].pa;
again:
				pt = (uint64_t *) pgtbl->pg[pgtbl->cnt].va;
				pgtbl->cnt++;
			}
		}
	}
#undef _IDX
}

static uint64_t ept_eptp(struct pg_tbl *pgtbl)
{
	uint64_t hpa;

	assert(!gva2hpa((uint64_t) pgtbl->pg[0].va, &hpa));

	/*  0 -   2: value 0 uncacheable, value 6 write-back
	 *  3 -   5: ept walk length - 1
	 *        6: enable access and dirty flags
	 *  7 -  11: reserved
	 * 12 - N-1: root_hpa
	 *  N -  63: reserved
	 */

	return 6UL | ((pgtbl->root_level - 1) << 3) | (1UL << 6) | (hpa & EPT_ADDR_MASK);
}

struct vcpu_ept_ctx {
	uint64_t apt_mem[512];
	struct pg_tbl pt;
	struct pg_tbl ept;
} __attribute__((aligned(0x1000)));

static void __configure_apt(struct pg_tbl *pgtbl, uint64_t *pt, int level, uint64_t ae[5])
{
	int i;
	for (i = 0; i < 512; i++) {
		if (pt[i]) {
			if (!(pt[i] & EPT__PS) && level > 1) {
				uint64_t _pt_va;
				assert(!pa_to_va(pgtbl, pt[i] & EPT_ADDR_MASK, &_pt_va));
				__configure_apt(pgtbl, (uint64_t *) _pt_va, level - 1, ae);
			}
		} else
			pt[i] = ae[level];
	}
}

static void configure_apt(struct vcpu_ept_ctx *ctx)
{
	uint64_t ae[5] = { 0 };

	assert(!((uint64_t *) ctx->ept.pg[0].va)[511]); // assuming, ((uint64_t) -1) & EPT_ADDR_MASK has no mapping

	memcpy((void *) ctx->apt_mem, (void *) ctx->pt.pg[0].va, 0x1000);

	{
		uint64_t hpa;
		assert(!gva2hpa((uint64_t) ctx->apt_mem, &hpa));
		pt_map(&ctx->ept, ((uint64_t) -1) & EPT_ADDR_MASK, hpa, EPT_R | EPT_W | /*EPT_X | EPT_U |*/ EPT_MT, 1);
	}

	{
		int i;
		uint64_t *pt;
		for (i = ctx->ept.root_level, pt = (uint64_t *) ctx->ept.pg[0].va; i > 0; i--) {
			assert(pt[511]);
			ae[i] = pt[511];
			if (i > 1) {
				uint64_t _pt_va;
				assert(!pa_to_va(&ctx->ept, pt[511] & EPT_ADDR_MASK, &_pt_va));
				pt = (uint64_t *) _pt_va;
			}
		}
	}

	__configure_apt(&ctx->ept, (uint64_t *) ctx->ept.pg[0].va, ctx->ept.root_level, ae);
}

static void free_vcpu_ept_ctx(struct vcpu_ept_ctx *ctx)
{
	free_pt(&ctx->pt);
	free_pt(&ctx->ept);
}

static void setup_vcpu_ept_ctx(struct vcpu_ept_ctx *ctx,
			       struct elisa_map_req req[],
			       int nr_req)
{
	{
		int i;
		for (i = 0; i < nr_req; i++) {
			uint64_t hpa;
			assert(req[i].level > 0);
			if (req[i].flags & ELISA_MAP_REQ_FLAGS_SRC_GPA)
				assert(!gpa2hpa(req[i].src_gxa, &hpa));
			else {
				assert(!mlock((void *) req[i].src_gxa, (1UL << (12 + 9 * (req[i].level - 1)))));
				assert(!gva2hpa(req[i].src_gxa, &hpa));
			}
			pt_map(&ctx->pt, req[i].dst_gva, req[i].dst_gpa, req[i].pt_flags, req[i].level);
			pt_map(&ctx->ept, req[i].dst_gpa, hpa, req[i].ept_flags, req[i].level);
		}
	}

	{ /* map pt to ept ctx */
		size_t i;
		/* ept: use same gpa-hpa mapping for pt */
		for (i = 0; i < ctx->pt.cnt; i++) {
			uint64_t hpa;
			assert(!gpa2hpa(ctx->pt.pg[i].pa, &hpa));
			pt_map(&ctx->ept, ctx->pt.pg[i].pa, hpa, EPT_R | EPT_W | /*EPT_X | EPT_U |*/ EPT_MT, 1);
		}
	}

	/* configure apt */
	configure_apt(ctx);
}

static void server_work(int fd,
			int (*server_cb)(int, uint64_t *, struct elisa_map_req **, int *, uint64_t *),
			int (*server_exit_cb)(uint64_t *))
{
	uint64_t server_user_any = 0;

	assert(!((uint64_t) gate_ctx_page % 0x1000));
	assert(!((uint64_t) sub_ctx_page % 0x1000));

	{
		/* 48 b8 f0 de bc 9a 78 56 34 12 : movabs $0x123456789abcdef0,%rax */
		uint8_t runtime_val_check[10] = { 0x48, 0xb8, 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, };
		assert(!memcmp(runtime_val_check, __RUNTIME_VALUE_0, 10));
		assert(!memcmp(runtime_val_check, __RUNTIME_VALUE_1, 10));
		assert(!memcmp(runtime_val_check, __RUNTIME_VALUE_2, 10));
	}

	{ // zero clear
		embed_addr_to_code((void *)((uintptr_t) __RUNTIME_VALUE_0 + 2), 0);
		embed_addr_to_code((void *)((uintptr_t) __RUNTIME_VALUE_1 + 2), 0);
		embed_addr_to_code((void *)((uintptr_t) __RUNTIME_VALUE_2 + 2), 0);
	}

	{
		uint64_t vcpu_id, requested_gva;

		assert(read(fd, &vcpu_id, sizeof(vcpu_id)) == sizeof(vcpu_id));
		assert(read(fd, &requested_gva, sizeof(requested_gva)) == sizeof(requested_gva));

		{ // preserve virtual address to avoid overlap
			void *m; // we do not free this
			assert((m = mmap((void *) requested_gva, 0x1000 * 3, PROT_READ,
							MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
							-1, 0)) != MAP_FAILED);
		}

		{
			struct vcpu_ept_ctx gate_ctx = {
				.pt.root_level = MMU_ROOT_LEVEL,
				.pt.is_ept = false,
				.ept.root_level = MMU_SHADOW_ROOT_LEVEL,
				.ept.is_ept = true,
			};

			struct vcpu_ept_ctx sub_ctx = {
				.pt.root_level = MMU_ROOT_LEVEL,
				.pt.is_ept = false,
				.ept.root_level = MMU_SHADOW_ROOT_LEVEL,
				.ept.is_ept = true,
			};

			uint64_t eptp_list[512] __attribute__((aligned(0x1000))) = { 0 };

			uint8_t *gate_stack, *sub_stack;

			assert((gate_stack = mmap(NULL, GATE_STACK_SIZE,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_LOCKED,
						-1, 0)) != MAP_FAILED);

			assert((sub_stack = mmap(NULL, SUB_STACK_SIZE,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_LOCKED,
						-1, 0)) != MAP_FAILED);

			{
				struct elisa_map_req __map_req[2 + SUB_STACK_SIZE / 0x1000];
				{
					unsigned long i;
					for (i = 0; i < (2 + SUB_STACK_SIZE / 0x1000); i++) {
						__map_req[i].flags = 0;
						__map_req[i].dst_gva = requested_gva + 0x1000 * i;
						__map_req[i].dst_gpa = BASE_GPA + 0x1000 * i;
						__map_req[i].level = 1;
						switch (i) {
						case 0:
							__map_req[i].src_gxa = (uint64_t) sub_ctx_page;
							__map_req[i].pt_flags = PT_P | /* PT_W |*/ PT_U,
							__map_req[i].ept_flags = /* EPT_R | EPT_W |*/ EPT_X | EPT_U | EPT_MT;
							break;
						case 1:
							__map_req[i].src_gxa = (uint64_t) eptp_list;
							__map_req[i].pt_flags = PT_P | PT_W | PT_U;
							__map_req[i].ept_flags = EPT_R | EPT_W | /* EPT_X |*/ EPT_U | EPT_MT;
							break;
						default:
							__map_req[i].src_gxa = (uint64_t) (sub_stack + 0x1000 * (i - 2));
							__map_req[i].pt_flags = PT_P | PT_W | PT_U;
							__map_req[i].ept_flags = EPT_R | EPT_W | /* EPT_X |*/ EPT_U | EPT_MT;
							break;
						}
					}
				}
				{
					uint64_t entry_function;
					struct elisa_map_req *_map_req;
					int map_req_cnt = 0;

					assert(!server_cb(fd, &server_user_any, &_map_req, &map_req_cnt, &entry_function));

					embed_addr_to_code((void *)((uintptr_t) __RUNTIME_VALUE_2 + 2), entry_function);

					{
						struct elisa_map_req *map_req = (struct elisa_map_req *) malloc(sizeof(struct elisa_map_req) * (2 + SUB_STACK_SIZE / 0x1000 + map_req_cnt));
						memcpy(&(map_req[0]), __map_req, sizeof(struct elisa_map_req) * (2 + SUB_STACK_SIZE / 0x1000));
						memcpy(&(map_req[2 + SUB_STACK_SIZE / 0x1000]), _map_req, sizeof(struct elisa_map_req) * map_req_cnt);
						setup_vcpu_ept_ctx(&sub_ctx, map_req, 2 + SUB_STACK_SIZE / 0x1000 + map_req_cnt);
						free(map_req);
					}

					free(_map_req);
				}
				embed_addr_to_code((void *)((uintptr_t) __RUNTIME_VALUE_0 + 2), ept_eptp(&sub_ctx.ept));
				embed_addr_to_code((void *)((uintptr_t) __RUNTIME_VALUE_1 + 2), ept_eptp(&sub_ctx.ept));
			}

			{
				struct elisa_map_req map_req[] = {
					{
						.src_gxa = (uint64_t) gate_ctx_page,
						.dst_gva = requested_gva + 0x1000 * 0,
						.dst_gpa = BASE_GPA + 0x1000 * 0,
						.pt_flags = PT_P | /* PT_W |*/ PT_U,
						.ept_flags = /* EPT_R | EPT_W |*/ EPT_X | EPT_U | EPT_MT,
						.level = 1,
					},
					{
						.src_gxa = (uint64_t) eptp_list,
						.dst_gva = requested_gva + 0x1000 * 1,
						.dst_gpa = BASE_GPA + 0x1000 * 1,
						.pt_flags = PT_P | PT_W | PT_U,
						.ept_flags = EPT_R | EPT_W | /* EPT_X |*/ EPT_U | EPT_MT,
						.level = 1,
					},
					{
						.src_gxa = (uint64_t) gate_stack,
						.dst_gva = requested_gva + 0x1000 * 2,
						.dst_gpa = BASE_GPA + 0x1000 * 2,
						.pt_flags = PT_P | PT_W | PT_U,
						.ept_flags = EPT_R | EPT_W | /* EPT_X |*/ EPT_U | EPT_MT,
						.level = 1,
					},
				};

				setup_vcpu_ept_ctx(&gate_ctx, map_req, 3);
			}

			eptp_list[0] = 0;
			eptp_list[1] = 0;
			eptp_list[511] = ept_eptp(&gate_ctx.ept);

			{
				uint64_t hpa;
				assert(!gva2hpa((uint64_t) eptp_list, &hpa));
				set_rendezvous_point_eptp_list_hpa(hpa);
			}

			set_rendezvous_point_vcpu_id(vcpu_id);

			{
				uint64_t ack;
				assert(write(fd, &ack, sizeof(ack)) == sizeof(ack));
			}

			{
				uint64_t eptp_list_address;
				assert(!gva2hpa((uint64_t) eptp_list, &eptp_list_address));
				assert(write(fd, &eptp_list_address, sizeof(eptp_list_address)) == sizeof(eptp_list_address));
			}

			{ // wait until the client finishes
				int err;
				uint64_t done;
				if ((err = read(fd, &done, sizeof(done))) == -1)
					assert(errno == ECONNRESET);
			}

			free_vcpu_ept_ctx(&gate_ctx);
			free_vcpu_ept_ctx(&sub_ctx);

			assert(!munmap(sub_stack, SUB_STACK_SIZE));

			assert(!server_exit_cb(&server_user_any));
		}
	}
}

void elisa_server(const int server_port,
		  int (*server_cb)(int, uint64_t *, struct elisa_map_req **, int *, uint64_t *),
		  int (*server_exit_cb)(uint64_t *))
{
	int fd;

	assert((fd = netfd_init(NULL, server_port)) >= 0);

	{
		int newfd;
		struct sockaddr_in caddr_in;
		socklen_t addrlen = sizeof(caddr_in);
		while ((newfd = accept(fd, (struct sockaddr *)&caddr_in, &addrlen)) != -1) {
			pid_t pid;
			assert((pid = fork()) != -1);
			if (pid == 0) {
				close(fd);
				server_work(newfd, server_cb, server_exit_cb);
				exit(0);
			} else
				close(newfd);
		}
		perror("accept");
	}

	close(fd);
}
