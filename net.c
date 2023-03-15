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

static int netfd_init(const char *server_addr_str, const int server_port)
{
	int sockfd;
	in_addr_t server_addr;
	bool is_server = true;

	if (server_addr_str) {
		assert(inet_pton(AF_INET, server_addr_str, &server_addr) == 1);
		is_server = false;
	}

	{
		assert((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) != -1);

		if (is_server) {
			{
				int on = 1;
				assert(!setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)));
			}

			{
				int on = 1;
				assert(!setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)));
			}
			{
				struct sockaddr_in sin = {
					.sin_family = AF_INET,
					.sin_addr.s_addr = htonl(INADDR_ANY),
					.sin_port = htons(server_port),
				};
				assert(!bind(sockfd, (struct sockaddr *) &sin, sizeof(sin)));
			}
			assert(!listen(sockfd, SOMAXCONN));
		} else {
			{
				struct sockaddr_in sin = {
					.sin_family = AF_INET,
					.sin_addr.s_addr = server_addr,
					.sin_port = htons(server_port),
				};
				assert(!connect(sockfd, (struct sockaddr *) &sin, sizeof(sin)));
			}
		}
	}

	return sockfd;
}
