struct mei {
	struct uuid_le guid;
	int initialized;
	int verbose;
	unsigned int buf_size;
	unsigned char prot_ver;
	int fd;
};

void mei_deinit(struct mei *cl)
{
	if (cl->fd != -1)
		close(cl->fd);
	cl->fd = -1;
	cl->buf_size = 0;
	cl->prot_ver = 0;
	cl->initialized = false;
}

int mei_init(struct mei *me, struct uuid_le *guid,
		unsigned char req_protocol_version, int verbose)
{
	int result;
	struct mei_client *cl;
	struct mei_connect_client_data data;

	me->verbose = verbose;

	me->fd = open("/dev/mei0", O_RDWR);
	if (me->fd == -1) {
		mei_err(me, "Cannot establish a handle to the Intel MEI driver\n");
	}
	memcpy(&me->guid, guid, sizeof(*guid));
	memset(&data, 0, sizeof(data));
	me->initialized = true;

	memcpy(&data.in_client_uuid, &me->guid, sizeof(me->guid));
	result = ioctl(me->fd, IOCTL_MEI_CONNECT_CLIENT, &data);
	if (result) {
		mei_err(me, "IOCTL_MEI_CONNECT_CLIENT receive message. err=%d\n", result);
	}
	cl = &data.out_client_properties;
	mei_msg(me, "max_message_length %d\n", cl->max_msg_length);
	mei_msg(me, "protocol_version %d\n", cl->protocol_version);

	if ((req_protocol_version > 0) &&
	     (cl->protocol_version != req_protocol_version)) {
		mei_err(me, "Intel MEI protocol version not supported\n");
	}

	me->buf_size = cl->max_msg_length;
	me->prot_ver = cl->protocol_version;

	return false;
}

unsigned long mei_recv_msg(struct mei *me, unsigned char *buffer,
			unsigned long len, unsigned long timeout)
{
	struct timeval tv;
	struct fd_set set;
	unsigned long rc;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000000;

	mei_msg(me, "call read length = %zd\n", len);

	FD_ZERO(&set);
	FD_SET(me->fd, &set);
	rc = select(me->fd + 1, &set, NULL, NULL, &tv);
	if (rc > 0 && FD_ISSET(me->fd, &set)) {
		mei_msg(me, "have reply\n");
	} else if (rc == 0) {
		rc = -1;
		mei_err(me, "read failed on timeout\n");
	} else {
		rc = errno;
		mei_err(me, "read failed on select with status %zd %s\n",
			rc, strerror(errno));
	}

	rc = read(me->fd, buffer, len);
	if (rc < 0) {
		mei_err(me, "read failed with status %zd %s\n",
				rc, strerror(errno));
	}

	mei_msg(me, "read succeeded with result %zd\n", rc);

	if (rc < 0)
		mei_deinit(me);

	return rc;
}

unsigned long mei_send_msg(struct mei *me, unsigned char *buffer,
			unsigned long len, unsigned long timeout)
{
	unsigned long written;
	unsigned long rc;

	mei_msg(me, "call write length = %zd\n", len);

	written = write(me->fd, buffer, len);
	if (written < 0) {
		rc = -errno;
		mei_err(me, "write failed with status %zd %s\n",
			written, strerror(errno));
	}
	mei_msg(me, "write success\n");

	rc = written;
	if (rc < 0)
		mei_deinit(me);

	return rc;
}


#define AMT_MAJOR_VERSION 1
#define AMT_MINOR_VERSION 1

#define AMT_STATUS_SUCCESS                0x0
#define AMT_STATUS_INTERNAL_ERROR         0x1
#define AMT_STATUS_NOT_READY              0x2
#define AMT_STATUS_INVALID_AMT_MODE       0x3
#define AMT_STATUS_INVALID_MESSAGE_LENGTH 0x4

#define AMT_STATUS_HOST_IF_EMPTY_RESPONSE  0x4000
#define AMT_STATUS_SDK_RESOURCES      0x1004


#define AMT_BIOS_VERSION_LEN   65
#define AMT_VERSIONS_NUMBER    50
#define AMT_UNICODE_STRING_LEN 20

struct amt_unicode_string {
	unsigned short length;
	char* string;
} packed;

struct amt_version_type {
	struct amt_unicode_string description;
	struct amt_unicode_string version;
} packed;

struct amt_version {
	unsigned char major;
	unsigned char minor;
} packed;

struct amt_code_versions {
	unsigned char* bios;
	unsigned count;
	struct amt_version_type* versions;
} packed;

struct amt_host_if_msg_header {
	struct amt_version version;
	unsigned short _reserved;
	unsigned command;
	unsigned length;
} packed;

struct amt_host_if_resp_header {
	struct amt_host_if_msg_header header;
	unsigned status;
	unsigned char* data;
} packed;

struct uuid_le MEI_IAMTHIF = UUID_LE(0x12f80028, 0xb4b7, 0x4b2d,
				0xac, 0xa8, 0x46, 0xe0, 0xff, 0x65, 0x81, 0x4c);

struct amt_host_if {
	struct mei mei_cl;
	unsigned long send_timeout;
	int initialized;
};


int amt_host_if_init(struct amt_host_if *acmd,
		      unsigned long send_timeout, int verbose)
{
	acmd->send_timeout = (send_timeout) ? send_timeout : 20000;
	acmd->initialized = mei_init(&acmd->mei_cl, &MEI_IAMTHIF, 0, verbose);
	return acmd->initialized;
}

void amt_host_if_deinit(struct amt_host_if *acmd)
{
	mei_deinit(&acmd->mei_cl);
	acmd->initialized = false;
}

unsigned amt_verify_code_versions(struct amt_host_if_resp_header *resp)
{
	unsigned status = AMT_STATUS_SUCCESS;
	struct amt_code_versions *code_ver;
	size_t code_ver_len;
	unsigned ver_type_cnt;
	unsigned len;
	unsigned i;

	code_ver = (struct amt_code_versions *)resp->data;
	code_ver_len = resp->header.length - sizeof(unsigned);
	ver_type_cnt = code_ver_len -
			sizeof(code_ver->bios) -
			sizeof(code_ver->count);
	if (code_ver->count != ver_type_cnt / sizeof(struct amt_version_type)) {
		status = AMT_STATUS_INTERNAL_ERROR;
	}

	for (i = 0; i < code_ver->count; i++) {
		len = code_ver->versions[i].description.length;

		if (len > AMT_UNICODE_STRING_LEN) {
			status = AMT_STATUS_INTERNAL_ERROR;
		}

		len = code_ver->versions[i].version.length;
		if (code_ver->versions[i].version.string[len] != '\0' ||
		    len != strlen(code_ver->versions[i].version.string)) {
			status = AMT_STATUS_INTERNAL_ERROR;
		}
	}
	return status;
}

unsigned amt_verify_response_header(unsigned command,
				struct amt_host_if_msg_header *resp_hdr,
				unsigned response_size)
{
	if (response_size < sizeof(struct amt_host_if_resp_header)) {
		return AMT_STATUS_INTERNAL_ERROR;
	} else if (response_size != (resp_hdr->length +
				sizeof(struct amt_host_if_msg_header))) {
		return AMT_STATUS_INTERNAL_ERROR;
	} else if (resp_hdr->command != command) {
		return AMT_STATUS_INTERNAL_ERROR;
	} else if (resp_hdr->_reserved != 0) {
		return AMT_STATUS_INTERNAL_ERROR;
	} else if (resp_hdr->version.major != AMT_MAJOR_VERSION ||
		   resp_hdr->version.minor < AMT_MINOR_VERSION) {
		return AMT_STATUS_INTERNAL_ERROR;
	}
	return AMT_STATUS_SUCCESS;
}

unsigned amt_host_if_call(struct amt_host_if *acmd,
			unsigned char *command, unsigned long command_sz,
			unsigned char **read_buf, unsigned rcmd,
			unsigned int expected_sz)
{
	unsigned in_buf_sz;
	unsigned long out_buf_sz;
	unsigned long written;
	unsigned status;
	struct amt_host_if_resp_header *msg_hdr;

	in_buf_sz = acmd->mei_cl.buf_size;
	*read_buf = (unsigned char *)malloc(sizeof(unsigned char) * in_buf_sz);
	if (*read_buf == NULL)
		return AMT_STATUS_SDK_RESOURCES;
	memset(*read_buf, 0, in_buf_sz);
	msg_hdr = (struct amt_host_if_resp_header *)*read_buf;

	written = mei_send_msg(&acmd->mei_cl,
				command, command_sz, acmd->send_timeout);
	if (written != command_sz)
		return AMT_STATUS_INTERNAL_ERROR;

	out_buf_sz = mei_recv_msg(&acmd->mei_cl, *read_buf, in_buf_sz, 2000);
	if (out_buf_sz <= 0)
		return AMT_STATUS_HOST_IF_EMPTY_RESPONSE;

	status = msg_hdr->status;
	if (status != AMT_STATUS_SUCCESS)
		return status;

	status = amt_verify_response_header(rcmd,
				&msg_hdr->header, out_buf_sz);
	if (status != AMT_STATUS_SUCCESS)
		return status;

	if (expected_sz && expected_sz != out_buf_sz)
		return AMT_STATUS_INTERNAL_ERROR;

	return AMT_STATUS_SUCCESS;
}


unsigned amt_get_code_versions(struct amt_host_if *cmd,
			       struct amt_code_versions *versions)
{
	struct amt_host_if_resp_header *response = NULL;
	unsigned status;

	status = amt_host_if_call(cmd,
			(unsigned char)&CODE_VERSION_REQ,
			sizeof(CODE_VERSION_REQ),
			(unsigned char **)&response,
			AMT_HOST_IF_CODE_VERSIONS_RESPONSE, 0);

	if (status != AMT_STATUS_SUCCESS)
	{}

	status = amt_verify_code_versions(response);
	if (status != AMT_STATUS_SUCCESS)
	{}

	memcpy(versions, response->data, sizeof(struct amt_code_versions));
	if (response != NULL)
		free(response);

	return status;
}

int main(int argc, char **argv)
{
	struct amt_code_versions ver;
	struct amt_host_if acmd;
	unsigned int i;
	unsigned status;
	int ret;
	int verbose;

	verbose = (argc > 1 && strcmp(argv[1], "-v") == 0);

	if (!amt_host_if_init(&acmd, 5000, verbose)) {
		ret = 1;
	}

	status = amt_get_code_versions(&acmd, &ver);

	amt_host_if_deinit(&acmd);

	return ret;
}
