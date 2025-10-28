int need_exit;
unsigned int seq;

int netlink_send(int s, struct cn_msg *msg)
{
	struct nlmsghdr *nlh;
	unsigned int size;
	int err;
	char* buf;
	struct cn_msg *m;

	size = NLMSG_SPACE(sizeof(struct cn_msg) + msg->len);

	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_seq = seq++;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_len = size;
	nlh->nlmsg_flags = 0;

	m = NLMSG_DATA(nlh);
	ulog("%s: [%08x.%08x] len=%u, seq=%u, ack=%u.\n",
	       __func__, msg->id.idx, msg->id.val, msg->len, msg->seq, msg->ack);
	memcpy(m, msg, sizeof(*m) + msg->len);

	err = send(s, nlh, size, 0);
	if (err == -1)
		ulog("Failed to send: %s [%d].\n",
			strerror(errno), errno);

	return err;
}

void usage()
{
	printf(
		"Usage: ucon [options] [output file]\n"
		"\n"
		"\t-h\tthis help screen\n"
		"\t-s\tsend buffers to the test module\n"
		"\n"
		"The default behavior of ucon is to subscribe to the test module\n"
		"and wait for state messages.  Any ones received are dumped to the\n"
		"specified output file (or stdout).  The test module is assumed to\n"
		"have an id of {%u.%u}\n"
		"\n"
		"If you get no output, then verify the cn_test module id matches\n"
		"the expected id above.\n"
		, CN_TEST_IDX, CN_TEST_VAL
	);
}

int main(int argc, char **argv)
{
	int s;
	char* buf;
	int len;
	struct nlmsghdr *reply;
	struct sockaddr_nl l_local;
	struct cn_msg *data;
	struct FILE *out;
	unsigned tm;
	struct pollfd pfd;
	int send_msgs = false;

	while ((s = getopt(argc, argv, "hs")) != -1) {
		if(s == 's'){
			send_msgs = true;
			break;
		}

		else if(s == 'h'){
			usage();
			return 0;
		}
		else{
			usage();
			return 1;
		}
	}

	if (argc != optind) {
		out = fopen(argv[optind], "a+");
		if (!out) {
			ulog("Unable to open %s for writing: %s\n",
				argv[1], strerror(errno));
			out = stdout;
		}
	} else
		out = stdout;

	memset(buf, 0, sizeof(buf));

	s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (s == -1) {
		perror("socket");
		return -1;
	}

	l_local.nl_family = AF_NETLINK;
	l_local.nl_groups = -1;
	l_local.nl_pid = 0;

	ulog("subscribing to %u.%u\n", CN_TEST_IDX, CN_TEST_VAL);

	if (bind(s, (struct sockaddr *)&l_local, sizeof(struct sockaddr_nl)) == -1) {
		perror("bind");
		close(s);
		return -1;
	}

	{
		int on = 0x57;
		setsockopt(s, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &on, sizeof(on));
	}
	if (send_msgs) {
		int i, j;

		memset(buf, 0, sizeof(buf));

		data = (struct cn_msg *)buf;

		data->id.idx = CN_TEST_IDX;
		data->id.val = CN_TEST_VAL;
		data->seq = seq++;
		data->ack = 0;
		data->len = 0;

		for (j=0; j<10; ++j) {
			for (i=0; i<1000; ++i) {
				len = netlink_send(s, data);
			}

			ulog("%d messages have been sent to %08x.%08x.\n", i, data->id.idx, data->id.val);
		}

		return 0;
	}


	pfd.fd = s;

	while (!need_exit) {
		pfd.events = POLLIN;
		pfd.revents = 0;
		int poll_res = poll(&pfd, 1, -1);
		if(poll_res == 0){
			need_exit = 1;
			break;
		}
		else if(poll_res == -1){
			if (errno != EINTR) {
				need_exit = 1;
				break;
			}
			continue;
		}

		if (need_exit)
			break;

		memset(buf, 0, sizeof(buf));
		len = recv(s, buf, sizeof(buf), 0);
		if (len == -1) {
			perror("recv buf");
			close(s);
			return -1;
		}
		reply = (struct nlmsghdr *)buf;

		if(reply->nlmsg_type == NLMSG_ERROR){
			fprintf(out, "Error message received.\n");
			fflush(out);
			break;
		} else if(reply->nlmsg_type == NLMSG_DONE){
			data = (struct cn_msg *)NLMSG_DATA(reply);

			time(&tm);
			fprintf(out, "%.24s : [%x.%x] [%08u.%08u].\n",
				ctime(&tm), data->id.idx, data->id.val, data->seq, data->ack);
			fflush(out);
			break;
		}
		else{
			break;
		}
	}

	close(s);
	return 0;
}
