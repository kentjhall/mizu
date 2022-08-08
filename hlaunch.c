#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mqueue.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <nx-launch-target>\n", argv[0]);
		return 1;
	}

	// open message queue to talk to loader
	mqd_t mqd = mq_open("/mizu_loader", O_WRONLY);
	if (mqd == -1) {
		if (errno == ENOENT)
			fprintf(stderr, "mq_open failed, is mizu running?\n");
		else
			perror("mq_open failed");
		return 1;
	}

	// resolve full path and verify existence
	char *path = realpath(argv[1], NULL);
	if (path == NULL) {
		perror("realpath failed");
		return 1;
	}

	// send request to mizu loader
	if (mq_send(mqd, path, strlen(path), 0) == -1) {
		perror("mq_send failed");
		return 1;
	}
	mq_close(mqd);
	return 0;
}
