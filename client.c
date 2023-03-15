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

void ____asm_impl_client(void)
{
	asm volatile (
	/* -- 4K align -- */
	".align 0x1000 \n\t"
	".globl gate_entry_page \n\t"
	"gate_entry_page: \n\t"

	".space 0x1000 - (gate_entry_end - elisa_gate_entry) \n\t"

	".globl elisa_gate_entry \n\t"
	"elisa_gate_entry: \n\t"

	"endbr64 \n\t"
	"push %rbp \n\t"
	"movq %rsp, %rbp \n\t"
	"push %rbx \n\t"
	"movq $0x0, %rax \n\t"
	"vmfunc \n\t"
	"gate_entry_end: \n\t"

	/* -- 4K align -- */

	/* this 4K page, will map gate_ctx_page in gate context */
	".space 0x1000 \n\t"

	/* -- 4K align -- */

	"gate_exit: \n\t"
	"movq %rbx, %rax \n\t"
	"pop %rbx \n\t"
	"leaveq \n\t"
	"retq \n\t"
	"gate_exit_end: \n\t"
	".space 0x1000 - (gate_exit_end - gate_exit) \n\t"
	);
}

extern void gate_entry_page(void);

int elisa_client(const char *server_addr_str, const int server_port, int client_cb(int))
{
	int fd;

	assert((fd = netfd_init(server_addr_str, server_port)) >= 0);

	{
		uint64_t vcpu_id = get_guest_vcpu_id();
		assert(write(fd, &vcpu_id, sizeof(vcpu_id)) == sizeof(vcpu_id));
	}
	{
		uint64_t request_gva = (uint64_t) gate_entry_page + 0x1000;
		assert(!((uint64_t) request_gva % 0x1000));
		assert(write(fd, &request_gva, sizeof(request_gva)) == sizeof(request_gva));
	}

	assert(!client_cb(fd));

	{
		uint64_t ack;
		assert(read(fd, &ack, sizeof(ack)) == sizeof(ack));
	}

	elisa_guest_activate();

	return fd;
}
