/* 
 * Copyright (c) 2015-2017 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "vspm_public.h"
#include "vspm_if.h"

#define APRINT(fmt, args...)		\
	fprintf(stderr, FUNCNAME ": " fmt, ##args)
#define ERRPRINT(fmt, args...)		\
	fprintf(stderr, FUNCNAME ":%s: " fmt, strerror(errno), ##args)

#define FUNCNAME		"vspm_api"
#define PRIO_OFFSET		10

struct vspm_handle {
	int fd;
	pthread_t thread;
};

/* callback thread routine */
static int vspm_cb_thread(struct vspm_handle *hdl)
{
	struct vspm_if_cb_rsp_t cb_info;

	while (1) {
		if (ioctl(hdl->fd, VSPM_IOC_CMD_WAIT_INTERRUPT, &cb_info))
			continue;

		/* check error code */
		if (cb_info.ercd < 0)
			break;

		/* execute callback function */
		(*(PFN_VSPM_COMPLETE_CALLBACK)cb_info.cb_func)(
			cb_info.job_id, cb_info.result,
			(void *)cb_info.user_data);
	}

	return 0;
}

/* Initialize the VSP manager */
long vspm_init_driver(void **handle, struct vspm_init_t *param)
{
	struct vspm_handle *hdl = 0;
	int policy;
	struct sched_param sched_param;
	pthread_attr_t thread_attr;

	long rtcd = R_VSPM_NG;
	int ercd;

	/* check parameter */
	if ((handle == NULL) || (param == NULL)) {
		rtcd = R_VSPM_PARAERR;
		goto err_exit1;
	}

	/* allocate memory */
	hdl = malloc(sizeof(*hdl));
	if (!hdl) {
		APRINT("failed to malloc()\n");
		goto err_exit1;
	}

	/* open VSP magnager */
	hdl->fd = open("/dev/" DEVFILE, O_RDWR);
	if (hdl->fd == -1) {
		ERRPRINT("failed to open()\n");
		goto err_exit2;
	}

	/* initialize VSP manager */
	if (ioctl(hdl->fd, VSPM_IOC_CMD_INIT, param)) {
		/* change error number */
		switch (errno) {
		case EINVAL:
			rtcd = R_VSPM_PARAERR;
			break;
		case EBUSY:
			rtcd = R_VSPM_ALREADY_USED;
			break;
		default:
			ERRPRINT("failed to ioctl(VSPM_IOC_CMD_INIT)\n");
			rtcd = R_VSPM_NG;
			break;
		}

		goto err_exit3;
	}

	/* init attribution of thread */
	ercd = pthread_attr_init(&thread_attr);
	if (ercd) {
		APRINT("failed to pthread_attr_init() %d\n", ercd);
		goto err_exit3;
	}

	ercd = pthread_getschedparam(pthread_self(), &policy, &sched_param);
	if (ercd) {
		APRINT("failed to pthread_getschedparam() %d\n", ercd);
		goto err_exit3;
	}

	ercd = pthread_attr_setschedpolicy(&thread_attr, policy);
	if (ercd) {
		APRINT("failed to pthread_attr_setschedpolicy() %d\n", ercd);
		goto err_exit3;
	}

	if ((policy == SCHED_FIFO) || (policy == SCHED_RR)) {
		sched_param.sched_priority += PRIO_OFFSET;
		ercd = pthread_attr_setschedparam(&thread_attr, &sched_param);
		if (ercd) {
			APRINT("failed to pthread_attr_setschedparam() %d\n", ercd);
			goto err_exit3;
		}
	}

	/* create callback thread */
	ercd = pthread_create(
		&hdl->thread, &thread_attr, (void *)vspm_cb_thread, (void *)hdl);
	if (ercd) {
		APRINT("failed to pthread_create() %d\n", ercd);
		goto err_exit3;
	}

	/* start callback */
	if (ioctl(hdl->fd, VSPM_IOC_CMD_WAIT_THREAD, NULL)) {
		ERRPRINT("failed to ioctl(VSPM_IOC_CMD_WAIT_CB_START)\n");
		goto err_exit4;
	}

	*handle = (void *)hdl;

	return R_VSPM_OK;

err_exit4:
	(void)pthread_join(hdl->thread, NULL);

err_exit3:
	(void)close(hdl->fd);

err_exit2:
	free(hdl);

err_exit1:
	return rtcd;
}

/* Finalize the VSP manager */
long vspm_quit_driver(void *handle)
{
	struct vspm_handle *hdl = (struct vspm_handle *)handle;

	int ercd;

	/* check parameter */
	if (hdl == NULL)
		return R_VSPM_PARAERR;

	/* Finalize VSP manager */
	if (ioctl(hdl->fd, VSPM_IOC_CMD_QUIT, NULL)) {
		ERRPRINT("failed to ioctl(VSPM_IOC_CMD_QUIT)\n");
		return R_VSPM_NG;
	}

	/* waiting callback end */
	if (ioctl(hdl->fd, VSPM_IOC_CMD_STOP_THREAD, NULL)) {
		ERRPRINT("failed to ioctl(VSPM_IOC_CMD_STOP_THREAD)\n");
		return R_VSPM_NG;
	}

	/* waiting pthread end */
	ercd = pthread_join(hdl->thread, NULL);
	if (ercd) {
		APRINT("failed to pthread_join() %d\n", ercd);
		return R_VSPM_NG;
	}

	/* close VSP manager */
	if (close(hdl->fd)) {
		ERRPRINT("failed to close()\n");
		return R_VSPM_NG;
	}

	/* release memory */
	free(hdl);

	return R_VSPM_OK;
}

/* Entry the job */
long vspm_entry_job(
	void *handle,
	unsigned long *job_id,
	char job_priority,
	struct vspm_job_t *ip_param,
	void *user_data,
	PFN_VSPM_COMPLETE_CALLBACK cb_func)
{
	struct vspm_handle *hdl = (struct vspm_handle *)handle;
	struct vspm_if_entry_t entry_info;

	/* check parameter */
	if (hdl == NULL)
		return R_VSPM_PARAERR;

	/* entry job */
	memset(&entry_info, 0, sizeof(struct vspm_if_entry_t));
	entry_info.req.priority  = job_priority;
	entry_info.req.job_param = ip_param;
	entry_info.req.user_data = user_data;
	entry_info.req.cb_func   = cb_func;

	if (ioctl(hdl->fd, VSPM_IOC_CMD_ENTRY, &entry_info)) {
		ERRPRINT("failed to ioctl(VSPM_IOC_CMD_ENTRY)\n");
		return R_VSPM_NG;
	}

	/* check return code */
	if (entry_info.rsp.ercd)
		return entry_info.rsp.ercd;

	if (job_id)
		*job_id = entry_info.rsp.job_id;

	return R_VSPM_OK;
}

/* Cancel the job */
long vspm_cancel_job(void *handle, unsigned long job_id)
{
	struct vspm_handle *hdl = (struct vspm_handle *)handle;
	long rtcd = R_VSPM_OK;

	/* check parameter */
	if (hdl == NULL)
		return R_VSPM_PARAERR;

	/* cancel job */
	if (ioctl(hdl->fd, VSPM_IOC_CMD_CANCEL, &job_id)) {
		/* change error number */
		switch (errno) {
		case EBUSY:
			rtcd = VSPM_STATUS_ACTIVE;
			break;
		case ENOENT:
			rtcd = VSPM_STATUS_NO_ENTRY;
			break;
		default:
			ERRPRINT("failed to ioctl(VSPM_IOC_CMD_CANCEL)\n");
			rtcd = R_VSPM_NG;
			break;
		}
	}

	return rtcd;
}

/* Get a status */
long vspm_get_status(void *handle, struct vspm_status_t *status)
{
	struct vspm_handle *hdl = (struct vspm_handle *)handle;
	long rtcd = R_VSPM_OK;

	/* check parameter */
	if ((hdl == NULL) || (status == NULL))
		return R_VSPM_PARAERR;

	/* get a status */
	if (ioctl(hdl->fd, VSPM_IOC_CMD_GET_STATUS, status)) {
		/* change error number */
		switch (errno) {
		case EINVAL:
			rtcd = R_VSPM_PARAERR;
			break;
		default:
			ERRPRINT("failed to ioctl(VSPM_IOC_GET_STATUS)\n");
			rtcd = R_VSPM_NG;
			break;
		}
	}

	return rtcd;
}

